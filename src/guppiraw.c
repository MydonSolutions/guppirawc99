#include "guppirawc99.h"

/*
 *
 * Returns:
 *  -1: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR`
 *  0 : Successfully parsed the header
 *  1 : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES`
 */
int _guppiraw_parse_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo, int parse) {
  const off_t header_start_pos = lseek(fd, 0, SEEK_CUR);
  if(gr_blockinfo != NULL) {
    gr_blockinfo->file_header_pos = header_start_pos;
  }

  size_t header_entry_count = 0;
  
  // Aligned to a 512-byte boundary so that it can be used
  // with files opened with O_DIRECT.
  char entries[GUPPI_RAW_HEADER_DIGEST_BYTES] __attribute__ ((aligned (512)));
  char *entry = entries;
  while(header_entry_count < GUPPI_RAW_HEADER_MAX_ENTRIES) {

    if(header_entry_count%GUPPI_RAW_HEADER_DIGEST_ENTRIES == 0){
      // read GUPPI_RAW_HEADER_DIGEST_ENTRIES at a time
      if(read(fd, entries, GUPPI_RAW_HEADER_DIGEST_BYTES) == 0) {
        return -1;
      }
      entry = entries;
    }

    if(gr_blockinfo != NULL && parse) {
      guppiraw_header_parse_entry(entry, &gr_blockinfo->metadata);
    }
    if(guppiraw_header_entry_is_END((uint64_t*)entry)) {
      break;
    }
    entry += 80;
    header_entry_count++;
  }
  // seek to before the excess bytes read (to after the uncounted END header_entry)
  off_t data_start_pos = lseek(
    fd,
    header_start_pos + (header_entry_count+1)*80,
    SEEK_SET
  );

  if(header_entry_count == GUPPI_RAW_HEADER_MAX_ENTRIES) {
    fprintf(stderr, "GuppiRaw: header END not found within %ld entries.\n", header_entry_count);
    return 1;
  }

  if(gr_blockinfo != NULL) {
    gr_blockinfo->file_data_pos = data_start_pos;
    if(gr_blockinfo->metadata.directio == 1) {
      gr_blockinfo->file_data_pos = guppiraw_calc_directio_aligned(gr_blockinfo->file_data_pos);
    }
  }
  return 0;
}

/*
 * 
 *
 * Returns:
 *  -1: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR`
 *  0 : Successfully parsed the header
 *  1 : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES`
 *  2 : Header is inappropriate (missing `BLOCSIZE`)
 */
int guppiraw_read_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo) {
  int rv = _guppiraw_parse_blockheader(fd, gr_blockinfo, 1);
  if(rv == 0) {
    if(gr_blockinfo->metadata.datashape.block_size == 0) {
      fprintf(stderr, "GuppiRaw Error: Header is missing a non-zero definition for `BLOCSIZE`.\n");
      return 2;
    }
  }
  return rv;
}

/*
 * Returns:
 *  -1: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR`
 *  0 : Successfully parsed the header
 *  1 : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES`
 */
int guppiraw_skim_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo) {
  return _guppiraw_parse_blockheader(fd, gr_blockinfo, 0);
}

/*
 * Returns:
 *  -X: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR` for Block X
 *  0 : Successfully parsed the first and skimmed all the other headers in the file
 *  2 : First Header is inappropriate (missing `BLOCSIZE`)
 *  X : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES` for Block X
 */
int guppiraw_skim_file(guppiraw_file_info_t* gr_fileinfo) {
  guppiraw_block_info_t tmp_blockinfo = {0};
  tmp_blockinfo.metadata.user_data = gr_fileinfo->metadata.user_data;
  tmp_blockinfo.metadata.user_callback = gr_fileinfo->metadata.user_callback;
  const int fd = gr_fileinfo->fd;

  gr_fileinfo->bytesize_file = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  int rv = guppiraw_read_blockheader(fd, &tmp_blockinfo);
  if(rv){
    return rv;
  }
  guppiraw_seek_next_block(fd, &tmp_blockinfo);
  size_t bytesize_first_block = tmp_blockinfo.file_data_pos + guppiraw_calc_directio_aligned(tmp_blockinfo.metadata.datashape.block_size);
  gr_fileinfo->n_block = (gr_fileinfo->bytesize_file + bytesize_first_block-1)/bytesize_first_block;

  gr_fileinfo->file_header_pos = malloc(gr_fileinfo->n_block * sizeof(off_t));
  gr_fileinfo->file_data_pos = malloc(gr_fileinfo->n_block * sizeof(off_t));
  gr_fileinfo->file_header_pos[0] = tmp_blockinfo.file_header_pos;
  gr_fileinfo->file_data_pos[0] = tmp_blockinfo.file_data_pos;

  memcpy(&gr_fileinfo->metadata, &tmp_blockinfo.metadata, sizeof(guppiraw_metadata_t));

  for(int i = 1; i < gr_fileinfo->n_block; i++) {
    rv = guppiraw_skim_blockheader(fd, &tmp_blockinfo);
    if(rv != 0) {
      rv *= i;
      gr_fileinfo->n_block = i;
      break;
    }
    gr_fileinfo->file_header_pos[i] = tmp_blockinfo.file_header_pos;
    gr_fileinfo->file_data_pos[i] = tmp_blockinfo.file_data_pos;
    guppiraw_seek_next_block(fd, &tmp_blockinfo);
  }
  gr_fileinfo->block_index = 0;

  return rv;
}

typedef struct {
  struct iovec* iovecs;
  int iovec_count;
} writev_parameters_t;

ssize_t guppiraw_write_block_batched(
  const int fd,
  const guppiraw_header_t* header,
  const void* data,
  const size_t n_aspect_batch,
  const size_t n_chan_batch
) {
  const char directio = header->metadata.directio;
  const size_t block_size = header->metadata.datashape.block_size;
  char* header_string = guppiraw_header_malloc_string(header);
  const size_t header_entries_len = (header->n_entries+1) * 80;
  const size_t header_string_len = directio ? guppiraw_calc_directio_aligned(header_entries_len) : header_entries_len;

  const long max_iovecs = sysconf(_SC_IOV_MAX);
  writev_parameters_t writev_params = {0};
  writev_params.iovecs = malloc(max_iovecs * sizeof(struct iovec));
  writev_params.iovec_count = 0;

  writev_params.iovecs[0].iov_base = header_string;
  writev_params.iovecs[0].iov_len = header_string_len;
  writev_params.iovec_count++;

  const size_t n_batched_aspect = header->metadata.datashape.n_aspect / n_aspect_batch;
  const size_t aspect_batch_block_size = block_size / n_aspect_batch;
  const size_t batch_block_size = aspect_batch_block_size / n_chan_batch;
  const size_t batch_aspect_stride = batch_block_size / n_batched_aspect;

  size_t aspect_batch_i, batch_aspect_i, chan_batch_i;
  ssize_t bytes_written = 0;
  for(aspect_batch_i = 0; aspect_batch_i < n_aspect_batch; aspect_batch_i++) {
    for(batch_aspect_i = 0; batch_aspect_i < n_batched_aspect; batch_aspect_i++) {
      for(chan_batch_i = 0; chan_batch_i < n_chan_batch; chan_batch_i++) {
        writev_params.iovecs[writev_params.iovec_count].iov_base = data + 
          aspect_batch_i*aspect_batch_block_size
          + chan_batch_i*batch_block_size
          + batch_aspect_i*batch_aspect_stride;

        writev_params.iovecs[writev_params.iovec_count].iov_len = batch_aspect_stride;
        writev_params.iovec_count++;
        if(writev_params.iovec_count == max_iovecs) {
          const ssize_t _bytes_written = writev(fd, writev_params.iovecs, writev_params.iovec_count);
          if(_bytes_written <= 0) {
            fprintf(stderr, "writev() error: %ld (@ %lu, %lu, %lu)\n", _bytes_written, aspect_batch_i, batch_aspect_i, chan_batch_i);
            return _bytes_written;
          }
          bytes_written += bytes_written;
          
          writev_params.iovec_count = 0;
        }
      }
    }
  }

  if(directio) {
    const size_t directio_pad_length = guppiraw_calc_directio_aligned(block_size) - block_size;
    if(writev_params.iovec_count > 0) {
      writev_params.iovecs[writev_params.iovec_count].iov_len += directio_pad_length;
    }
    else {
        writev_params.iovecs[writev_params.iovec_count].iov_base = data;
        writev_params.iovecs[writev_params.iovec_count].iov_len = directio_pad_length;
        writev_params.iovec_count++;
    }
  }
  bytes_written += writev(fd, writev_params.iovecs, writev_params.iovec_count);

  free(header_string);
  free(writev_params.iovecs);
  return bytes_written;
}
