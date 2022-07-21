#include "guppiraw.h"

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
  while(!guppiraw_header_entry_is_END((uint64_t*)entry) && header_entry_count < GUPPI_RAW_HEADER_MAX_ENTRIES) {

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
      gr_blockinfo->file_data_pos = guppiraw_directio_align(gr_blockinfo->file_data_pos);
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
  if(rv == 0){
    guppiraw_datashape_t* datashape = &gr_blockinfo->metadata.datashape;
    if(datashape->block_size == 0) {
      fprintf(stderr, "GuppiRaw Error: Header is missing a definition for `BLOCSIZE`.\n");
      return 2;
    }

    if(datashape->n_obschan * datashape->n_pol * datashape->n_bit == 0) {
      // some factor is zero!
      fprintf(
        stderr,
        "GuppiRaw Warning: some dimension-lengths are zero, will fallback! [OBSNCHAN:%u->1, NPOL:%d->1, NBITS:%d->4]\n",
        datashape->n_obschan, datashape->n_pol, datashape->n_bit
      );
      datashape->n_obschan = 1;
      datashape->n_pol = 1;
      datashape->n_bit = 4;
    }

    datashape->n_aspect = 1;
    if(datashape->n_ant > 0) {
      datashape->n_aspect = datashape->n_ant;
    }
    if(datashape->n_beam > 0) {
      datashape->n_aspect = datashape->n_beam;
    }
    datashape->n_aspectchan = datashape->n_obschan/datashape->n_aspect;

    datashape->bytestride_aspect = datashape->block_size/datashape->n_aspect;
    datashape->bytestride_channel = datashape->bytestride_aspect/datashape->n_aspectchan;
    
    datashape->bytestride_polarization = (2*datashape->n_bit)/8; // TODO assert > 0
    datashape->bytestride_time = datashape->n_pol*datashape->bytestride_polarization;

    datashape->n_time = datashape->bytestride_channel / datashape->bytestride_time;
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
  size_t bytesize_first_block = tmp_blockinfo.file_data_pos + guppiraw_directio_align(tmp_blockinfo.metadata.datashape.block_size);
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

ssize_t guppiraw_write_block(const int fd, const guppiraw_header_t* header, const void* data, const uint32_t block_size, const char directio) {
  char* header_string = guppiraw_header_malloc_string(header, directio);
  const size_t header_entries_len = (header->n_entries+1) * 80;
  const size_t header_string_len = directio ? guppiraw_directio_align(header_entries_len) : header_entries_len;

  struct iovec* block_iovecs = malloc(2*sizeof(struct iovec));
  block_iovecs[0].iov_base = header_string;
  block_iovecs[0].iov_len = header_string_len;

  block_iovecs[1].iov_base = data;
  block_iovecs[1].iov_len = directio ? guppiraw_directio_align(block_size) : block_size;

  ssize_t bytes_written = writev(fd, block_iovecs, 2);
  free(header_string);
  return bytes_written;
}
