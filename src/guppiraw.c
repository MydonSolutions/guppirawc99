#include "guppiraw.h"

/*
 * 
 *
 * Returns:
 *  -1: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR`
 *  0 : Successfully parsed the header
 *  1 : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES`
 */
int guppiraw_read_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo) {
  gr_blockinfo->file_header_pos = lseek(fd, 0, SEEK_CUR);

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
    else if(gr_blockinfo != NULL) {
      switch (((uint64_t*)entry)[0]) {
        case KEY_UINT64_ID_LE('B','L','O','C','S','I','Z','E'):
          hgetu8(entry, "BLOCSIZE", &gr_blockinfo->block_size);
          break;
        case KEY_UINT64_ID_LE('D','I','R','E','C','T','I','O'):
          hgetl(entry, "DIRECTIO", &gr_blockinfo->directio);
          break;
        case KEY_UINT64_ID_LE('O','B','S','N','C','H','A','N'):
          hgetu4(entry, "OBSNCHAN", &gr_blockinfo->n_obschan);
          break;
        case KEY_UINT64_ID_LE('N','P','O','L',' ',' ',' ',' '):
          hgetu4(entry, "NPOL", &gr_blockinfo->n_pol);
          break;
        case KEY_UINT64_ID_LE('N','B','I','T','S',' ',' ',' '):
          hgetu4(entry, "NBITS", &gr_blockinfo->n_bit);
          break;
        default:
          break;
      }
    }
  }

  if(header_entry_count == GUPPI_RAW_HEADER_MAX_ENTRIES) {
    fprintf(stderr, "GuppiRaw: header END not found within %ld entries.", header_entry_count);
    return 1;
  }
  
  gr_blockinfo->bytesize_complexsample = (2*gr_blockinfo->n_bit)/8;
  gr_blockinfo->n_time = gr_blockinfo->block_size / (
    gr_blockinfo->n_obschan * gr_blockinfo->n_pol * gr_blockinfo->bytesize_complexsample
  );

  gr_blockinfo->bytestride_polarization = gr_blockinfo->bytesize_complexsample;
  gr_blockinfo->bytestride_time = gr_blockinfo->bytestride_polarization*gr_blockinfo->n_pol;
  gr_blockinfo->bytestride_frequency = gr_blockinfo->bytestride_time*gr_blockinfo->n_time;

  gr_blockinfo->file_data_pos = lseek(fd, 0, SEEK_CUR);
  if(gr_blockinfo->directio == 1) {
    gr_blockinfo->file_data_pos = directio_align_value(gr_blockinfo->file_data_pos);
  }

  return 0;
}

