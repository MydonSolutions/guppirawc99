#ifndef GUPPI_RAW_C99_H_
#define GUPPI_RAW_C99_H_

#ifndef _GNU_SOURCE
#pragma message("_GNU_SOURCE not defined so DIRECTIO is disabled!") 
#define O_DIRECT 0
#endif

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <limits.h>

#include "fitsheader.h"

#define GUPPI_RAW_HEADER_MAX_ENTRIES 2048
#define GUPPI_RAW_HEADER_DIGEST_BYTES 5*4096 // LCM(80, 4096)
#define GUPPI_RAW_HEADER_DIGEST_ENTRIES (GUPPI_RAW_HEADER_DIGEST_BYTES/80)
#define GUPPI_RAW_HEADER_END_STR \
"END                                                                             "

#define GUPPI_RAW_KEY_UINT64_ID_BE(c0,c1,c2,c3,c4,c5,c6,c7) \
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

#define GUPPI_RAW_KEY_UINT64_ID_LE(c0,c1,c2,c3,c4,c5,c6,c7) \
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

#define GUPPI_RAW_KEYSTR_UINT64_ID_BE(key) \
  GUPPI_RAW_KEY_UINT64_ID_BE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7])

#define GUPPI_RAW_KEYSTR_UINT64_ID_LE(key) \
  GUPPI_RAW_KEY_UINT64_ID_LE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7])

typedef struct {
  // Header populated fields
  uint32_t n_obschan;
  uint32_t n_ant;
  uint32_t n_beam;
  uint32_t n_pol;
  uint32_t n_bit;
  uint64_t block_size;

  // Header inferred fields
  size_t n_time;
  uint32_t n_aspect;
  uint32_t n_aspectchan;

  size_t bytestride_polarization;
  size_t bytestride_time;
  size_t bytestride_channel;
  size_t bytestride_aspect;
} guppiraw_datashape_t;

typedef struct {
  // Header populated fields
  int directio;

  guppiraw_datashape_t datashape;

  // User data
  void (*user_callback)(const char* entry, void* user_data);
  void* user_data;
} guppiraw_metadata_t;

typedef struct {
  guppiraw_metadata_t metadata;

  // File position fields
  off_t file_header_pos;
  off_t file_data_pos;
} guppiraw_block_info_t;

typedef struct {
  guppiraw_metadata_t metadata;
  off_t bytesize_file;
  int n_blocks;
  // Block-position fields
  off_t *file_header_pos;
  off_t *file_data_pos;
} guppiraw_file_info_t;

// Negative `header_length` indicates no hard limit on `header_string` length
void guppiraw_parse_blockheader_string(guppiraw_metadata_t* metadata, char* header_string, int64_t header_length);

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

static inline off_t guppiraw_directio_align(off_t value) {
  return (value + 511) & ~((off_t)511);
}

static inline int guppiraw_seek_next_block(int fd, const guppiraw_block_info_t* gr_blockinfo) {
  return lseek(
    fd,
    gr_blockinfo->metadata.directio == 1 ? 
      guppiraw_directio_align(gr_blockinfo->file_data_pos + gr_blockinfo->metadata.datashape.block_size) :
      gr_blockinfo->file_data_pos + gr_blockinfo->metadata.datashape.block_size,
    0
  );
}

static inline int guppiraw_read_blockdata(int fd, const guppiraw_block_info_t* gr_blockinfo, void* buffer) {
  lseek(fd, gr_blockinfo->file_data_pos, SEEK_SET);
  return read(fd, buffer, gr_blockinfo->metadata.datashape.block_size);
}

typedef struct {
  int fd;
  int fileenum;
  char* stempath;
  int stempath_len;

  guppiraw_file_info_t file_info;

  int block_index;
  size_t time_index;
  size_t chan_index;
  size_t aspect_index;
} guppiraw_iterate_info_t;

int guppiraw_iterate_open_stem(const char* filepath, guppiraw_iterate_info_t* gr_iterate);
long guppiraw_iterate_read(guppiraw_iterate_info_t* gr_iterate, const size_t ntime, const size_t nchan, const size_t naspect, void* buffer);

static inline long guppiraw_iterate_read_block(guppiraw_iterate_info_t* gr_iterate, void* buffer) {
  return guppiraw_iterate_read(
    gr_iterate,
    gr_iterate->file_info.metadata.datashape.n_time,
    gr_iterate->file_info.metadata.datashape.n_aspectchan,
    gr_iterate->file_info.metadata.datashape.n_aspect,
    buffer
  );
}

static inline size_t guppiraw_iterate_bytesize(const guppiraw_iterate_info_t* gr_iterate, size_t ntime, size_t nchan, size_t naspect) {
  return naspect * nchan * ntime * gr_iterate->file_info.metadata.datashape.bytestride_time;
}

static inline size_t guppiraw_iterate_filentime_remaining(const guppiraw_iterate_info_t* gr_iterate) {
  return (gr_iterate->file_info.n_blocks - gr_iterate->block_index)*
    gr_iterate->file_info.metadata.datashape.n_time -
     gr_iterate->time_index
  ;
}

typedef struct guppiraw_header_llnode {
  char keyvalue[81];
  struct guppiraw_header_llnode* next;
} guppiraw_header_llnode_t;

typedef struct {
  guppiraw_header_llnode_t* head;
  uint32_t n_entries;
} guppiraw_header_t;

int guppiraw_header_put_string(guppiraw_header_t* header, const char* key, const char* value); 
int guppiraw_header_put_double(guppiraw_header_t* header, const char* key, const double value); 
int guppiraw_header_put_integer(guppiraw_header_t* header, const char* key, const int64_t value); 

void guppiraw_header_free(guppiraw_header_t* header);

char* guppiraw_header_malloc_string(const guppiraw_header_t* header, const char directio);
ssize_t guppiraw_write_block(const int fd, const guppiraw_header_t* header, const void* data, const uint32_t block_size, const char directio);

#endif// GUPPI_RAW_C99_H_