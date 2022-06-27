#ifndef GUPPI_RAW_C99_H_
#define GUPPI_RAW_C99_H_

#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>

#include "fitsheader.h"

#define GUPPI_RAW_HEADER_MAX_ENTRIES 1024
#define GUPPI_RAW_HEADER_END_STR \
"END                                                                             "

#define KEY_UINT64_ID_BE(c0,c1,c2,c3,c4,c5,c6,c7) \
  (\
  (((uint64_t)c0)<<56) + \
  (((uint64_t)c1)<<48) + \
  (((uint64_t)c2)<<40) + \
  (((uint64_t)c3)<<32) + \
  (((uint64_t)c4)<<24) + \
  (((uint64_t)c5)<<16) + \
  (((uint64_t)c6)<<8) + \
  ((uint64_t)c7)\
  )

#define KEY_UINT64_ID_LE(c0,c1,c2,c3,c4,c5,c6,c7) \
  (uint64_t)(\
  ((uint64_t)c0) + \
  (((uint64_t)c1)<<8) + \
  (((uint64_t)c2)<<16) + \
  (((uint64_t)c3)<<24) + \
  (((uint64_t)c4)<<32) + \
  (((uint64_t)c5)<<40) + \
  (((uint64_t)c6)<<48) + \
  (((uint64_t)c7)<<56)\
  )

#define KEYSTR_UINT64_ID_BE(key) \
  KEY_UINT64_ID_BE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7])

#define KEYSTR_UINT64_ID_LE(key) \
  KEY_UINT64_ID_LE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7])

typedef struct {
  uint64_t block_size;
  int directio;
  uint32_t n_obschan;
  uint32_t n_pol;
  uint32_t n_bit;
  uint32_t n_ant;

  off_t file_header_pos;
  off_t file_data_pos;
} guppiraw_block_info_t;

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
        case KEY_UINT64_ID_LE('N','A','N','T','S',' ',' ',' '):
          hgetu4(entry, "NANTS", &gr_blockinfo->n_ant);
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

  gr_blockinfo->file_data_pos = lseek(fd, 0, SEEK_CUR);
  if(gr_blockinfo->directio == 1) {
    gr_blockinfo->file_data_pos = (gr_blockinfo->file_data_pos + 511) & ~((off_t)511);
  }

  return 0;
}

int guppiraw_seek_next_block(int fd, guppiraw_block_info_t* gr_blockinfo) {
  off_t next_header_pos = gr_blockinfo->file_data_pos + gr_blockinfo->block_size;
  if(gr_blockinfo->directio == 1) {
    next_header_pos = (next_header_pos + 511) & ~((off_t)511);
  }
  return lseek(fd, next_header_pos, 0);
}

#endif// GUPPI_RAW_C99_H_