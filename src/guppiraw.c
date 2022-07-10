#include "guppiraw.h"

static const uint64_t KEY_BLOCSIZE  = GUPPI_RAW_KEY_UINT64_ID_LE('B','L','O','C','S','I','Z','E');
static const uint64_t KEY_OBSNCHAN  = GUPPI_RAW_KEY_UINT64_ID_LE('O','B','S','N','C','H','A','N');
static const uint64_t KEY_NPOL      = GUPPI_RAW_KEY_UINT64_ID_LE('N','P','O','L',' ',' ',' ',' ');
static const uint64_t KEY_NBITS     = GUPPI_RAW_KEY_UINT64_ID_LE('N','B','I','T','S',' ',' ',' ');
static const uint64_t KEY_DIRECTIO  = GUPPI_RAW_KEY_UINT64_ID_LE('D','I','R','E','C','T','I','O');

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
  char entries[GUPPI_RAW_HEADER_DIGEST_BYTES] __attribute__ ((aligned (512)));
  char *entry = entries;
  while(strncmp(entry, GUPPI_RAW_HEADER_END_STR, 80) != 0 && header_entry_count < GUPPI_RAW_HEADER_MAX_ENTRIES) {

    if(header_entry_count%GUPPI_RAW_HEADER_DIGEST_ENTRIES == 0){
      // read GUPPI_RAW_HEADER_DIGEST_ENTRIES at a time
      if(read(fd, entries, GUPPI_RAW_HEADER_DIGEST_BYTES) == 0) {
        return -1;
      }
      entry = entries;
    }

    if(gr_blockinfo != NULL && parse) {
      if(((uint64_t*)entry)[0] == KEY_BLOCSIZE)
        hgetu8(entry, "BLOCSIZE", &gr_blockinfo->datashape.block_size);
      else if(((uint64_t*)entry)[0] == KEY_OBSNCHAN)
        hgetu4(entry, "OBSNCHAN", &gr_blockinfo->datashape.n_obschan);
      else if(((uint64_t*)entry)[0] == KEY_NPOL)
        hgetu4(entry, "NPOL", &gr_blockinfo->datashape.n_pol);
      else if(((uint64_t*)entry)[0] == KEY_NBITS)
        hgetu4(entry, "NBITS", &gr_blockinfo->datashape.n_bit);
      else if(((uint64_t*)entry)[0] == KEY_DIRECTIO)
        hgeti4(entry, "DIRECTIO", &gr_blockinfo->directio);

      if(gr_blockinfo->header_entry_callback != 0) {
        gr_blockinfo->header_entry_callback(entry, gr_blockinfo->header_user_data);
      }
    }
    entry += 80;
    header_entry_count++;
  }
  // seek to before the excess bytes read (to after the uncounted END header_entry)
  off_t data_start_pos = lseek(
    fd,
    (GUPPI_RAW_HEADER_DIGEST_ENTRIES-((header_entry_count+1)%GUPPI_RAW_HEADER_DIGEST_ENTRIES))*-80,
    SEEK_CUR
  );

  if(header_entry_count == GUPPI_RAW_HEADER_MAX_ENTRIES) {
    fprintf(stderr, "GuppiRaw: header END not found within %ld entries.\n", header_entry_count);
    return 1;
  }

  if(gr_blockinfo != NULL) {
    gr_blockinfo->file_data_pos = data_start_pos;
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

  int rv = guppiraw_read_blockheader(fd, ptr_blockinfo);
  if(rv){
    return rv;
  }
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

int _guppiraw_iterate_open(guppiraw_iterate_info_t* gr_iterate) {
  if(gr_iterate->fd > 0) {
    close(gr_iterate->fd);
    gr_iterate->fileenum++;
  }
  char* filepath = malloc(gr_iterate->stempath_len+9+1);
  sprintf(filepath, "%s.%04d.raw", gr_iterate->stempath, gr_iterate->fileenum%10000);
  gr_iterate->fd = open(filepath, O_RDONLY);
  free(filepath);
  if(gr_iterate->fd <= 0) {
    return 1;
  }
  gr_iterate->block_index = 0;
  return guppiraw_skim_file(gr_iterate->fd, &gr_iterate->file_info);
}

int guppiraw_iterate_open_stem(const char* filepath, guppiraw_iterate_info_t* gr_iterate) {
  gr_iterate->stempath_len = strlen(filepath);

  // handle if filepath is not just the stempath
  gr_iterate->fd = open(filepath, O_RDONLY);
  if(gr_iterate->fd != -1) {
    close(gr_iterate->fd);

    // `filepath` is the `filestem.\d{4}.raw`
    gr_iterate->stempath_len = strlen(filepath)-9;
    gr_iterate->fileenum = atoi(filepath + gr_iterate->stempath_len + 1);
  }

  gr_iterate->stempath = malloc(gr_iterate->stempath_len);
  strncpy(gr_iterate->stempath, filepath, gr_iterate->stempath_len);
  gr_iterate->stempath[gr_iterate->stempath_len] = '\0';
  
  gr_iterate->fd = 0;
  return _guppiraw_iterate_open(gr_iterate);
}

static inline long _guppiraw_read_time_gap(
  const guppiraw_iterate_info_t* gr_iterate,
  const size_t time, const size_t time_step,
  const size_t chan, const size_t chan_step,
  const size_t read_size, const size_t chan_step_stride,
  void* buffer
) {
  const guppiraw_datashape_t* datashape = &gr_iterate->file_info.block_info.datashape;
  
  const size_t chan_steps = chan/chan_step;
  const size_t time_steps = time/time_step;
  long bytes_read = 0;

  for (size_t time_i = 0; time_i < time_steps; time_i++) {
    const size_t time_index = gr_iterate->time_index + time_i*time_step;
    for (size_t chan_i = 0; chan_i < chan_steps; chan_i++) {
      lseek(
        gr_iterate->fd,
        gr_iterate->file_info.file_data_pos[gr_iterate->block_index + (time_index / datashape->n_time)] + 
          (time_index % datashape->n_time) * datashape->bytestride_time +
          (gr_iterate->chan_index + chan_i*chan_step) * datashape->bytestride_frequency,
        SEEK_SET
      );
      bytes_read += read(
        gr_iterate->fd,
        buffer + 
          chan_i*chan_step_stride +
          time_i*read_size,
        read_size
      );
    }
  }
  return bytes_read;
}

long guppiraw_iterate_read(guppiraw_iterate_info_t* gr_iterate, const size_t time, const size_t chan, void* buffer) {
  const guppiraw_datashape_t* datashape = &gr_iterate->file_info.block_info.datashape;
  if(gr_iterate->chan_index + chan > datashape->n_obschan) {
    // cannot gather in channel dimension
    fprintf(stderr, "Error: cannot gather in channel dimension.\n");
    return -1;
  }
  // check that time request is a factor of remaining file_ntime
  if(guppiraw_iterate_filentime_remaining(gr_iterate) < time) {
    // TODO handle 2 files at a time.
    fprintf(
      stderr,
      "Error: remaining file_ntime (%d*%lu-%lu) is less than iteration time (%lu).\n",
      (gr_iterate->file_info.n_blocks - gr_iterate->block_index),
      gr_iterate->time_index,
      datashape->n_time,
      time
    );
    return -1;
  }

  long bytes_read = 0;
  if(buffer != NULL) {
    if(time == datashape->n_time && chan == datashape->n_obschan && gr_iterate->chan_index == 0 && gr_iterate->time_index == 0) {
      lseek(gr_iterate->fd, gr_iterate->file_info.file_data_pos[gr_iterate->block_index], SEEK_SET);
      bytes_read += read(gr_iterate->fd, buffer, datashape->block_size);
    }
    else {
      // interleave time-slice reads for different channels (maintain frequency as slowest axis)
      const size_t chan_step = time != datashape->n_time ? 1 : chan;
      // can read at most NTIME in the time dimension
      const size_t time_step = time > datashape->n_time ? datashape->n_time : time;

      const size_t read_size = guppiraw_iterate_bytesize(gr_iterate, time_step, chan_step);
      const size_t chan_step_stride = guppiraw_iterate_bytesize(gr_iterate, time, 1);

      bytes_read += _guppiraw_read_time_gap(
        gr_iterate,
        time, time_step,
        chan, chan_step,
        read_size, chan_step_stride,
        buffer
      );
    }
  }

  gr_iterate->chan_index = (gr_iterate->chan_index + chan) % datashape->n_obschan;
  if(gr_iterate->chan_index == 0) {
    if(gr_iterate->time_index + time >= datashape->n_time) {
      // increment to at least the next block
      if(time < datashape->n_time) {
        gr_iterate->block_index += 1;  
      }
      else {
        gr_iterate->block_index += time / datashape->n_time;
      }
      if(gr_iterate->block_index == gr_iterate->file_info.n_blocks) {
        _guppiraw_iterate_open(gr_iterate);
      }
    }

    gr_iterate->time_index = (gr_iterate->time_index + time) % datashape->n_time;
  }

  return bytes_read;
}
