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

typedef struct _block_pos_ll {
  off_t header_pos;
  off_t data_pos;
  struct _block_pos_ll *next;
} _block_pos_ll_t;

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

  int rv = guppiraw_read_blockheader(fd, &tmp_blockinfo);
  if(rv){
    return rv;
  }
  memcpy(&gr_fileinfo->metadata, &tmp_blockinfo.metadata, sizeof(guppiraw_metadata_t));
  gr_fileinfo->n_block = 1;
  gr_fileinfo->bytesize_file = lseek(fd, 0, SEEK_END);

  _block_pos_ll_t *block_cur = NULL, *block_ll_head;
  block_ll_head = malloc(sizeof(_block_pos_ll_t));
  block_ll_head->header_pos = tmp_blockinfo.file_header_pos;
  block_ll_head->data_pos = tmp_blockinfo.file_data_pos;
  block_cur = block_ll_head;

  while(guppiraw_seek_next_block(fd, &tmp_blockinfo) < gr_fileinfo->bytesize_file) {
    gr_fileinfo->block_index = gr_fileinfo->n_block;

    block_cur->next = malloc(sizeof(_block_pos_ll_t));
    block_cur = block_cur->next;
    gr_fileinfo->n_block += 1;

    rv = guppiraw_skim_blockheader(fd, &tmp_blockinfo);
    block_cur->header_pos = tmp_blockinfo.file_header_pos;
    block_cur->data_pos = tmp_blockinfo.file_data_pos;

    if(rv != 0) {
      rv *= gr_fileinfo->block_index;
      break;
    }
  }

  if(rv == 0) {
    gr_fileinfo->block_index = 0;
  }

  gr_fileinfo->file_header_pos = malloc(gr_fileinfo->n_block * sizeof(off_t));
  gr_fileinfo->file_data_pos = malloc(gr_fileinfo->n_block * sizeof(off_t));

  for(int i = 0; i < gr_fileinfo->n_block; i++) {
    gr_fileinfo->file_header_pos[i] = block_ll_head->header_pos;
    gr_fileinfo->file_data_pos[i] = block_ll_head->data_pos;
    block_cur = block_ll_head;
    block_ll_head = block_ll_head->next;
    free(block_cur);
  }

  return rv;
}

typedef struct {
  struct iovec* iovecs;
  int iovec_count;
} writev_parameters_t;

char* _guppiraw_writev_parameters_header(writev_parameters_t* params, const guppiraw_header_t* header) {
  char* header_string = guppiraw_header_malloc_string(header);
  const size_t header_entries_len = (header->n_entries+1) * 80;

  params->iovecs[0].iov_base = header_string;
  params->iovecs[0].iov_len = header->metadata.directio ? guppiraw_calc_directio_aligned(header_entries_len) : header_entries_len;
  params->iovec_count = 1;
  return header_string;
}

ssize_t guppiraw_write_block_batched(
  const int fd,
  const guppiraw_header_t* header,
  const void* data,
  const size_t n_aspect_batch,
  const size_t n_chan_batch
) {
  const long max_iovecs = sysconf(_SC_IOV_MAX);
  writev_parameters_t writev_params = {0};
  writev_params.iovecs = malloc(max_iovecs * sizeof(struct iovec));
  writev_params.iovec_count = 0;

  char* header_string = _guppiraw_writev_parameters_header(&writev_params, header);

  const char directio = header->metadata.directio;
  const size_t block_size = header->metadata.datashape.block_size;

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
          bytes_written += _bytes_written;
          
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

ssize_t guppiraw_write_block_arbitrary(
  const int fd,
  const guppiraw_header_t* header,
  const void* data,
  const size_t bytestride_aspect,
  const size_t bytestride_channel,
  const size_t bytestride_time,
  const size_t bytestride_polarization
) {
  const long max_iovecs = sysconf(_SC_IOV_MAX);
  writev_parameters_t writev_params = {0};
  writev_params.iovecs = malloc(max_iovecs * sizeof(struct iovec));
  writev_params.iovec_count = 0;

  char* header_string = _guppiraw_writev_parameters_header(&writev_params, header);

  const char directio = header->metadata.directio;
  const guppiraw_datashape_t* datashape = &header->metadata.datashape;
  const size_t sample_size = (datashape->n_bit * 2)/8;

  size_t aspect_i, time_i, polarization_i, channel_i;
  ssize_t bytes_written = 0;
  for(aspect_i = 0; aspect_i < datashape->n_aspect; aspect_i++){
    for(channel_i = 0; channel_i < datashape->n_aspectchan; channel_i++){
      for(time_i = 0; time_i < datashape->n_time; time_i++){
        for(polarization_i = 0; polarization_i < datashape->n_pol; polarization_i++){

          writev_params.iovecs[writev_params.iovec_count].iov_base = data
            + aspect_i * bytestride_aspect
            + time_i * bytestride_time
            + polarization_i * bytestride_polarization
            + channel_i * bytestride_channel;

          writev_params.iovecs[writev_params.iovec_count].iov_len = sample_size;
          writev_params.iovec_count++;
          if(writev_params.iovec_count == max_iovecs) {
            const ssize_t _bytes_written = writev(fd, writev_params.iovecs, writev_params.iovec_count);
            if(_bytes_written <= 0) {
              fprintf(stderr, "writev() error: %ld (@ %lu, %lu, %lu, %lu)\n", _bytes_written, aspect_i, time_i, polarization_i, channel_i);
              return _bytes_written;
            }
            bytes_written += _bytes_written;

            writev_params.iovec_count = 0;
          }

        }
      }
    }
  }

  if(directio) {
    const size_t directio_pad_length = guppiraw_calc_directio_aligned(datashape->block_size) - datashape->block_size;
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
