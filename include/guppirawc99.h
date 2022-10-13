#ifndef GUPPI_RAW_C99_H_
#define GUPPI_RAW_C99_H_

#include "guppirawc99/directio.h"

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <limits.h>

#include "guppirawc99/structs.h"
#include "guppirawc99/header_key.h"
#include "guppirawc99/file.h"
#include "guppirawc99/iterate.h"
#include "guppirawc99/header.h"
#include "guppirawc99/calculations.h"

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

static inline off_t guppiraw_seek_next_block(int fd, const guppiraw_block_info_t* gr_blockinfo) {
  return lseek(
    fd,
    gr_blockinfo->metadata.directio == 1 ? 
      guppiraw_calc_directio_aligned(gr_blockinfo->file_data_pos + gr_blockinfo->metadata.datashape.block_size) :
      gr_blockinfo->file_data_pos + gr_blockinfo->metadata.datashape.block_size,
    0
  );
}

static inline int guppiraw_read_blockdata(int fd, const guppiraw_block_info_t* gr_blockinfo, void* buffer) {
  lseek(fd, gr_blockinfo->file_data_pos, SEEK_SET);
  return read(fd, buffer, gr_blockinfo->metadata.datashape.block_size);
}

ssize_t guppiraw_write_block_batched(const int fd, const guppiraw_header_t* header, const void* data, const size_t n_aspect_batch, const size_t n_chan_batch);
#define guppiraw_write_block(fd, header, data) guppiraw_write_block_batched(fd, header, data, 1, 1)

ssize_t guppiraw_write_block_arbitrary(
  const int fd,
  const guppiraw_header_t* header,
  const void* data,
  const size_t bytestride_aspect,
  const size_t bytestride_channel,
  const size_t bytestride_time,
  const size_t bytestride_polarization
);

#endif// GUPPI_RAW_C99_H_