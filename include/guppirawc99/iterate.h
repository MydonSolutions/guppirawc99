#ifndef GUPPI_RAW_C99_ITERATE_H_
#define GUPPI_RAW_C99_ITERATE_H_

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
#include "guppirawc99/file.h"
#include "guppirawc99/calculations.h"

typedef struct {
	int file_index;
	int fileblock_index;
} guppiraw_block_location_t;

typedef struct {
  char* stempath;
  int stempath_len;
  int fileenum_offset;
  
  guppiraw_file_info_t* file_info;
  int n_file;
  int file_index;

  // over all files
	guppiraw_block_location_t* block_location;
  int n_block;
  int block_index;

  size_t time_index;
  size_t chan_index;
  size_t aspect_index;

  char iterate_time_first_not_frequency_first;
} guppiraw_iterate_info_t;

/*
 * 
 *
 * Returns:
 *  -1: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR`
 *  0 : Successfully parsed the header
 *  1 : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES`
 */
int guppiraw_skim_file(guppiraw_file_info_t* gr_fileinfo);

/*
 * If the filepath provided cannot be opened (i.e. if it is not a singular file),
 * it is deemed the stem-filepath:
 *   Opens at most gr_iterate->n_file if n_file > 0, otherwise 10000.
 *   Starts at fileenum_offset and increments.
 * Otherwise just the single filepath provided is opened.
 *
 * Returns (status_code applies to last file opened):
 *  0 : Successfully parsed the first and skimmed all the other headers in the file
 *  -X: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR` for Block X
 *  2 : First Header is inappropriate (missing `BLOCSIZE`)
 *  3 : Could not open the file `"%s.%04d.raw": gr_iterate->stempath, gr_iterate->fileenum_offset`
 *  X : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES` for Block X
 */
int guppiraw_iterate_open_with_user_metadata(
  guppiraw_iterate_info_t* gr_iterate,
  const char* filepath,
  size_t user_datasize,
  guppiraw_header_entry_parser user_callback
);
#define guppiraw_iterate_open(gr_iterate, filepath) guppiraw_iterate_open_with_user_metadata(gr_iterate, filepath, 0, NULL)

long guppiraw_iterate_read(guppiraw_iterate_info_t* gr_iterate, const size_t ntime, const size_t nchan, const size_t naspect, void* buffer);
void guppiraw_iterate_close(guppiraw_iterate_info_t* gr_iterate);

#define guppiraw_iterate_file_info(gr_iterate, index) ((gr_iterate)->file_info + (index))
#define guppiraw_iterate_file_info_offset(gr_iterate, offset) guppiraw_iterate_file_info(gr_iterate, gr_iterate->file_index + offset)
#define guppiraw_iterate_file_info_current(gr_iterate) guppiraw_iterate_file_info_offset(gr_iterate, 0)

#define guppiraw_iterate_metadata(/* const guppiraw_iterate_info_t* */ gr_iterate) (&((gr_iterate)->file_info[0].metadata))
#define guppiraw_iterate_datashape(/* const guppiraw_iterate_info_t* */ gr_iterate) (&((gr_iterate)->file_info[0].metadata.datashape))

int guppiraw_iterate_file_index_of_block(const guppiraw_iterate_info_t* gr_iterate, int* block_index);
int guppiraw_iterate_file_index_of_block_offset(const guppiraw_iterate_info_t* gr_iterate, int* block_index);

guppiraw_file_info_t* guppiraw_iterate_file_info_of_block(const guppiraw_iterate_info_t* gr_iterate, int* block_index);
guppiraw_file_info_t* guppiraw_iterate_file_info_of_block_offset(const guppiraw_iterate_info_t* gr_iterate, int* block_index);

static inline long guppiraw_iterate_read_block(guppiraw_iterate_info_t* gr_iterate,void* buffer) {
  return guppiraw_iterate_read(
    gr_iterate,
    guppiraw_iterate_datashape(gr_iterate)->n_time,
    guppiraw_iterate_datashape(gr_iterate)->n_aspectchan,
    guppiraw_iterate_datashape(gr_iterate)->n_aspect,
    buffer
  );
}

static inline size_t guppiraw_iterate_bytesize(const guppiraw_iterate_info_t* gr_iterate, size_t ntime, size_t nchan, size_t naspect) {
  return naspect * nchan * ntime * guppiraw_iterate_file_info_current(gr_iterate)->metadata.datashape.bytestride_time;
}

static inline size_t guppiraw_iterate_ntime_remaining(const guppiraw_iterate_info_t* gr_iterate) {
  return (gr_iterate->n_block - gr_iterate->block_index)*guppiraw_iterate_datashape(gr_iterate)->n_time - gr_iterate->time_index;
}

#endif // GUPPI_RAW_C99_ITERATE_H_