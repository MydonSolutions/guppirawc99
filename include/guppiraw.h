#ifndef GUPPI_RAW_C99_H_
#define GUPPI_RAW_C99_H_

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
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
  // Header populated fields
  uint32_t n_obschan;
  uint32_t n_pol;
  uint32_t n_bit;
  uint64_t block_size;

  // Header inferred fields
  size_t n_time;

  size_t bytesize_complexsample;
  size_t bytestride_polarization;
  size_t bytestride_time;
  size_t bytestride_frequency;
} guppiraw_datashape_t;

typedef struct {
  // Header populated fields
  int directio;

  guppiraw_datashape_t datashape;

  // User data
  void (*header_entry_callback)(char* entry, void* user_data);
  void* header_user_data;

  // File position fields
  off_t file_header_pos;
  off_t file_data_pos;
} guppiraw_block_info_t;

typedef struct {
  guppiraw_block_info_t block_info;
  off_t bytesize_file;
  int n_blocks;
  // Block-position fields
  off_t *file_header_pos;
  off_t *file_data_pos;
} guppiraw_file_info_t;

/*
 * 
 *
 * Returns:
 *  -1: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR`
 *  0 : Successfully parsed the header
 *  1 : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES`
 */
int guppiraw_read_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo);
int guppiraw_skim_blockheader(int fd, guppiraw_block_info_t* gr_blockinfo);
int guppiraw_skim_file(int fd, guppiraw_file_info_t* gr_fileinfo);

static inline off_t directio_align_value(off_t value) {
  return (value + 511) & ~((off_t)511);
}

static inline int guppiraw_seek_next_block(int fd, guppiraw_block_info_t* gr_blockinfo) {
  return lseek(
    fd,
    gr_blockinfo->directio == 1 ? directio_align_value(gr_blockinfo->file_data_pos + gr_blockinfo->datashape.block_size) : gr_blockinfo->file_data_pos + gr_blockinfo->datashape.block_size,
    0
  );
}

static inline int guppiraw_read_blockdata(int fd, guppiraw_block_info_t* gr_blockinfo, void* buffer) {
  lseek(fd, gr_blockinfo->file_data_pos, 0);
  return read(fd, buffer, gr_blockinfo->datashape.block_size);
}

#endif// GUPPI_RAW_C99_H_