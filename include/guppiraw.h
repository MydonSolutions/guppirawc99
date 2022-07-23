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
#include "guppirawc99/structs.h"
#include "guppirawc99/header_key.h"
#include "guppirawc99/file.h"
#include "guppirawc99/iterate.h"
#include "guppirawc99/header.h"
#include "guppirawc99/directio.h"

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

ssize_t guppiraw_write_block(const int fd, const guppiraw_header_t* header, const void* data);

#endif// GUPPI_RAW_C99_H_