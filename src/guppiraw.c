#include "guppiraw.h"

/*
 * 
 *
 * Returns:
 *  -1: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR`
 *  0 : Successfully parsed the header
 *  1 : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES`
 */
int _guppiraw_parse_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo, int parse) {
  if(gr_blockinfo != NULL) {
    gr_blockinfo->file_header_pos = lseek(fd, 0, SEEK_CUR);
  }

  size_t header_entry_count = 0;
  
  // Aligned to a 512-byte boundary so that it can be used
  // with files opened with O_DIRECT.
  char entry[81] __attribute__ ((aligned (512)));
  while(header_entry_count < GUPPI_RAW_HEADER_MAX_ENTRIES) {
    if(read(fd, entry, 80) == 0) {
      return -1;
    }

    if(strncmp(entry, GUPPI_RAW_HEADER_END_STR, 80) == 0) {
      break;
    }
    else if(gr_blockinfo != NULL && parse) {
      switch (((uint64_t*)entry)[0]) {
        case KEY_UINT64_ID_LE('B','L','O','C','S','I','Z','E'):
          hgetu8(entry, "BLOCSIZE", &gr_blockinfo->datashape.block_size);
          break;
        case KEY_UINT64_ID_LE('O','B','S','N','C','H','A','N'):
          hgetu4(entry, "OBSNCHAN", &gr_blockinfo->datashape.n_obschan);
          break;
        case KEY_UINT64_ID_LE('N','P','O','L',' ',' ',' ',' '):
          hgetu4(entry, "NPOL", &gr_blockinfo->datashape.n_pol);
          break;
        case KEY_UINT64_ID_LE('N','B','I','T','S',' ',' ',' '):
          hgetu4(entry, "NBITS", &gr_blockinfo->datashape.n_bit);
          break;

        case KEY_UINT64_ID_LE('D','I','R','E','C','T','I','O'):
          hgetl(entry, "DIRECTIO", &gr_blockinfo->directio);
          break;
        default:
          break;
      }
      if(gr_blockinfo->header_entry_callback != 0) {
        gr_blockinfo->header_entry_callback(entry, gr_blockinfo->header_user_data);
      }
    }
  }

  if(header_entry_count == GUPPI_RAW_HEADER_MAX_ENTRIES) {
    fprintf(stderr, "GuppiRaw: header END not found within %ld entries.", header_entry_count);
    return 1;
  }

  if(gr_blockinfo != NULL) {
    gr_blockinfo->file_data_pos = lseek(fd, 0, SEEK_CUR);
    if(gr_blockinfo->directio == 1) {
      gr_blockinfo->file_data_pos = directio_align_value(gr_blockinfo->file_data_pos);
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
 */
int guppiraw_read_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo) {
  int rv = _guppiraw_parse_blockheader(fd, gr_blockinfo, 1);
  if(rv == 0){
    gr_blockinfo->datashape.bytesize_complexsample = (2*gr_blockinfo->datashape.n_bit)/8;
    gr_blockinfo->datashape.n_time = gr_blockinfo->datashape.block_size / (
      gr_blockinfo->datashape.n_obschan * gr_blockinfo->datashape.n_pol * gr_blockinfo->datashape.bytesize_complexsample
    );

    gr_blockinfo->datashape.bytestride_polarization = gr_blockinfo->datashape.bytesize_complexsample;
    gr_blockinfo->datashape.bytestride_time = gr_blockinfo->datashape.bytestride_polarization*gr_blockinfo->datashape.n_pol;
    gr_blockinfo->datashape.bytestride_frequency = gr_blockinfo->datashape.bytestride_time*gr_blockinfo->datashape.n_time;
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
 *  X : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES` for Block X
 */
int guppiraw_skim_file(int fd, guppiraw_file_info_t* gr_fileinfo) {
  guppiraw_block_info_t *ptr_blockinfo = &gr_fileinfo->block_info;

  gr_fileinfo->bytesize_file = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  guppiraw_read_blockheader(fd, ptr_blockinfo);
  guppiraw_seek_next_block(fd, ptr_blockinfo);
  size_t bytesize_first_block = ptr_blockinfo->file_data_pos + directio_align_value(ptr_blockinfo->datashape.block_size);
  gr_fileinfo->n_blocks = (gr_fileinfo->bytesize_file + bytesize_first_block-1)/bytesize_first_block;

  gr_fileinfo->file_header_pos = malloc(gr_fileinfo->n_blocks * sizeof(off_t));
  gr_fileinfo->file_data_pos = malloc(gr_fileinfo->n_blocks * sizeof(off_t));
  gr_fileinfo->file_header_pos[0] = ptr_blockinfo->file_header_pos;
  gr_fileinfo->file_data_pos[0] = ptr_blockinfo->file_data_pos;

  guppiraw_block_info_t temp_blockinfo;
  ptr_blockinfo = &temp_blockinfo;
  memcpy(ptr_blockinfo, &gr_fileinfo->block_info, sizeof(guppiraw_block_info_t));

  int rv = 0;
  for(int i = 1; i < gr_fileinfo->n_blocks; i++) {
    rv = guppiraw_skim_blockheader(fd, ptr_blockinfo);
    if(rv != 0) {
      rv *= i;
      gr_fileinfo->n_blocks = i;
      break;
    }
    gr_fileinfo->file_header_pos[i] = ptr_blockinfo->file_header_pos;
    gr_fileinfo->file_data_pos[i] = ptr_blockinfo->file_data_pos;
    guppiraw_seek_next_block(fd, ptr_blockinfo);
  }

  return rv;
}