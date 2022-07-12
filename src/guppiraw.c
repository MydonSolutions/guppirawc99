#include "guppiraw.h"

static const uint64_t KEY_BLOCSIZE  = GUPPI_RAW_KEY_UINT64_ID_LE('B','L','O','C','S','I','Z','E');
static const uint64_t KEY_OBSNCHAN  = GUPPI_RAW_KEY_UINT64_ID_LE('O','B','S','N','C','H','A','N');
static const uint64_t KEY_NPOL      = GUPPI_RAW_KEY_UINT64_ID_LE('N','P','O','L',' ',' ',' ',' ');
static const uint64_t KEY_NBITS     = GUPPI_RAW_KEY_UINT64_ID_LE('N','B','I','T','S',' ',' ',' ');
static const uint64_t KEY_DIRECTIO  = GUPPI_RAW_KEY_UINT64_ID_LE('D','I','R','E','C','T','I','O');

static inline void _parse_entry(const char* entry, guppiraw_metadata_t* metadata) {
  if(((uint64_t*)entry)[0] == KEY_BLOCSIZE)
    hgetu8(entry, "BLOCSIZE", &metadata->datashape.block_size);
  else if(((uint64_t*)entry)[0] == KEY_OBSNCHAN)
    hgetu4(entry, "OBSNCHAN", &metadata->datashape.n_obschan);
  else if(((uint64_t*)entry)[0] == KEY_NPOL)
    hgetu4(entry, "NPOL", &metadata->datashape.n_pol);
  else if(((uint64_t*)entry)[0] == KEY_NBITS)
    hgetu4(entry, "NBITS", &metadata->datashape.n_bit);
  else if(((uint64_t*)entry)[0] == KEY_DIRECTIO)
    hgeti4(entry, "DIRECTIO", &metadata->directio);

  if(metadata->user_callback != 0) {
    metadata->user_callback(entry, metadata->user_data);
  }
}

void guppiraw_parse_blockheader_string(guppiraw_metadata_t* metadata, char* header_string, int64_t header_length) {
  int32_t entry_count = 0;
  while(
    strncmp(header_string, GUPPI_RAW_HEADER_END_STR, 80) != 0 && 
    ((entry_count+1)*80 < header_length || header_length < 0)
  ) {
    fprintf(stderr, "%d: %s\n", entry_count, header_string);
    _parse_entry(header_string, metadata);
    header_string += 80;
    entry_count++;
  }
}

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
  while(strncmp(entry, GUPPI_RAW_HEADER_END_STR, 80) != 0 && header_entry_count < GUPPI_RAW_HEADER_MAX_ENTRIES) {

    if(header_entry_count%GUPPI_RAW_HEADER_DIGEST_ENTRIES == 0){
      // read GUPPI_RAW_HEADER_DIGEST_ENTRIES at a time
      if(read(fd, entries, GUPPI_RAW_HEADER_DIGEST_BYTES) == 0) {
        return -1;
      }
      entry = entries;
    }

    if(gr_blockinfo != NULL && parse) {
      _parse_entry(entry, &gr_blockinfo->metadata);
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
      gr_blockinfo->file_data_pos = guppiraw_directio_align_value(gr_blockinfo->file_data_pos);
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

    datashape->bytesize_complexsample = (2*datashape->n_bit)/8;
    const size_t denominator = datashape->n_obschan * datashape->n_pol * datashape->bytesize_complexsample;
    datashape->n_time = datashape->block_size / (denominator);

    datashape->bytestride_polarization = datashape->bytesize_complexsample;
    datashape->bytestride_time = datashape->bytestride_polarization*datashape->n_pol;
    datashape->bytestride_frequency = datashape->bytestride_time*datashape->n_time;
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
int guppiraw_skim_file(int fd, guppiraw_file_info_t* gr_fileinfo) {
  guppiraw_block_info_t *ptr_blockinfo = &gr_fileinfo->block_info;

  gr_fileinfo->bytesize_file = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  int rv = guppiraw_read_blockheader(fd, ptr_blockinfo);
  if(rv){
    return rv;
  }
  guppiraw_seek_next_block(fd, ptr_blockinfo);
  size_t bytesize_first_block = ptr_blockinfo->file_data_pos + guppiraw_directio_align_value(ptr_blockinfo->metadata.datashape.block_size);
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

/*
 * Returns:
 *  -X: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR` for Block X
 *  0 : Successfully parsed the first and skimmed all the other headers in the file
 *  2 : First Header is inappropriate (missing `BLOCSIZE`)
 *  3 : Could not open the file `"%s.%04d.raw": gr_iterate->stempath, gr_iterate->fileenum`
 *  X : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES` for Block X
 */
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
    return 3;
  }
  gr_iterate->block_index = 0;
  return guppiraw_skim_file(gr_iterate->fd, &gr_iterate->file_info);
}

/*
 * Returns:
 *  -X: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR` for Block X
 *  0 : Successfully parsed the first and skimmed all the other headers in the file
 *  2 : First Header is inappropriate (missing `BLOCSIZE`)
 *  3 : Could not open the file `"%s.%04d.raw": gr_iterate->stempath, gr_iterate->fileenum`
 *  X : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES` for Block X
 */
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
  const guppiraw_datashape_t* datashape = &gr_iterate->file_info.block_info.metadata.datashape;
  
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
  const guppiraw_datashape_t* datashape = &gr_iterate->file_info.block_info.metadata.datashape;
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

static char _GUPPI_RAW_FTISHEADER_VALUEBUF_STR[] =
"                                                                                "
GUPPI_RAW_HEADER_END_STR;  

int _guppiraw_header_put_entry(guppiraw_header_llnode_t* head, const char* keyvalue) {
  while(1) {
    if(strncmp(head->keyvalue, keyvalue, 8) == 0) {
      memcpy(head->keyvalue, keyvalue, 80);
      return 0;
    }

    if(head->next == NULL) {
      break;
    }
    head = head->next;
  }
  head->next = malloc(sizeof(guppiraw_header_llnode_t));
  memcpy(head->next->keyvalue, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR, 80);
  head->next->next = NULL;
  return 1;
}

int _guppiraw_header_put_string(guppiraw_header_llnode_t* head, const char* key, const char* value) {
  memset(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, ' ', 80);
  hputs(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, key, value);
  return _guppiraw_header_put_entry(head, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR);
}

int _guppiraw_header_put_double(guppiraw_header_llnode_t* head, const char* key, const double value) {
  memset(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, ' ', 80);
  hputr8(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, key, value);
  return _guppiraw_header_put_entry(head, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR);
}

int _guppiraw_header_put_integer(guppiraw_header_llnode_t* head, const char* key, const int value) {
  memset(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, ' ', 80);
  hputi4(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, key, value);
  return _guppiraw_header_put_entry(head, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR);
}

static inline void _guppiraw_header_ensure_initialised(guppiraw_header_t* header, const char* key) {
  if(header->head == NULL || header->n_entries == 0) {
    header->head = malloc(sizeof(guppiraw_header_llnode_t));
    memset(header->head->keyvalue, ' ', 80);
    memcpy(header->head->keyvalue, key, strlen(key));
    header->head->next = NULL;
    header->n_entries = 1;
  }
}

int guppiraw_header_put_string(guppiraw_header_t* header, const char* key, const char* value) {
  _guppiraw_header_ensure_initialised(header, key);
  header->n_entries += _guppiraw_header_put_string(header->head, key, value);
  return 0;
}
int guppiraw_header_put_double(guppiraw_header_t* header, const char* key, const double value) {
  _guppiraw_header_ensure_initialised(header, key);
  header->n_entries += _guppiraw_header_put_double(header->head, key, value);
  return 0;
}
int guppiraw_header_put_integer(guppiraw_header_t* header, const char* key, const int value) {
  _guppiraw_header_ensure_initialised(header, key);
  header->n_entries += _guppiraw_header_put_integer(header->head, key, value);
  return 0;
}

char* guppiraw_header_malloc_string(guppiraw_header_t* header) {
  const int n_entries = header->n_entries;
  guppiraw_header_llnode_t* header_entry = header->head;
  char* header_string = malloc((n_entries + 1) * 80);
  for(int i = 0; i < n_entries; i++) {
    memcpy(header_string + i*80, header_entry->keyvalue, 80);
    header_entry = header_entry->next;
  }
  memcpy(header_string + n_entries*80, GUPPI_RAW_HEADER_END_STR, 80);
  return header_string;
}

void _guppiraw_header_free(guppiraw_header_llnode_t* head) {
  if(head->next != NULL) {
    _guppiraw_header_free(head->next);
  }
  free(head->next);
}

void guppiraw_header_free(guppiraw_header_t* header) {
  _guppiraw_header_free(header->head);
  free(header->head);
  header->n_entries = 0;
}

const char _guppiraw_directio_padding_buffer[513] = 
"********************************************************************************************************************************"
"********************************************************************************************************************************"
"********************************************************************************************************************************"
"********************************************************************************************************************************";

void guppiraw_write_block(int fd, guppiraw_header_t* header, void* data, uint32_t block_size, char directio) {
  char* header_string = guppiraw_header_malloc_string(header);
  const size_t header_string_len = (header->n_entries+1) * 80;
  off_t file_pos = lseek(fd, 0, SEEK_CUR);
  file_pos += write(fd, header_string, header_string_len);
  if(directio) {
    file_pos += write(fd, _guppiraw_directio_padding_buffer, guppiraw_directio_align_value(file_pos) - file_pos);
  }
  file_pos += write(fd, data, block_size);
  if(directio) {
    write(fd, _guppiraw_directio_padding_buffer, guppiraw_directio_align_value(file_pos) - file_pos);
  }
  free(header_string);
}
