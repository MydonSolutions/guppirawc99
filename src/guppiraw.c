#include "guppiraw.h"

static const uint64_t KEY_UINT64_BLOCSIZE  = GUPPI_RAW_KEY_UINT64_ID_LE('B','L','O','C','S','I','Z','E');
static const uint64_t KEY_UINT64_NANTS     = GUPPI_RAW_KEY_UINT64_ID_LE('N','A','N','T','S',' ',' ',' ');
static const uint64_t KEY_UINT64_NBEAMS    = GUPPI_RAW_KEY_UINT64_ID_LE('N','B','E','A','M','S',' ',' ');
static const uint64_t KEY_UINT64_OBSNCHAN  = GUPPI_RAW_KEY_UINT64_ID_LE('O','B','S','N','C','H','A','N');
static const uint64_t KEY_UINT64_NPOL      = GUPPI_RAW_KEY_UINT64_ID_LE('N','P','O','L',' ',' ',' ',' ');
static const uint64_t KEY_UINT64_NBITS     = GUPPI_RAW_KEY_UINT64_ID_LE('N','B','I','T','S',' ',' ',' ');
static const uint64_t KEY_UINT64_DIRECTIO  = GUPPI_RAW_KEY_UINT64_ID_LE('D','I','R','E','C','T','I','O');

static const uint64_t KEY_UINT64_END  = GUPPI_RAW_KEY_UINT64_ID_LE('E','N','D',' ',' ',' ',' ',' ');
static const uint64_t _UINT64_BLANK   = GUPPI_RAW_KEY_UINT64_ID_LE(' ',' ',' ',' ',' ',' ',' ',' ');

static inline void _parse_entry(const char* entry, guppiraw_metadata_t* metadata) {
  if(((uint64_t*)entry)[0] == KEY_UINT64_BLOCSIZE)
    hgetu8(entry, "BLOCSIZE", &metadata->datashape.block_size);
  else if(((uint64_t*)entry)[0] == KEY_UINT64_NANTS)
    hgetu4(entry, "NANTS", &metadata->datashape.n_ant);
  else if(((uint64_t*)entry)[0] == KEY_UINT64_NBEAMS)
    hgetu4(entry, "NBEAMS", &metadata->datashape.n_beam);
  else if(((uint64_t*)entry)[0] == KEY_UINT64_OBSNCHAN)
    hgetu4(entry, "OBSNCHAN", &metadata->datashape.n_obschan);
  else if(((uint64_t*)entry)[0] == KEY_UINT64_NPOL)
    hgetu4(entry, "NPOL", &metadata->datashape.n_pol);
  else if(((uint64_t*)entry)[0] == KEY_UINT64_NBITS)
    hgetu4(entry, "NBITS", &metadata->datashape.n_bit);
  else if(((uint64_t*)entry)[0] == KEY_UINT64_DIRECTIO)
    hgeti4(entry, "DIRECTIO", &metadata->directio);

  if(metadata->user_callback != 0) {
    metadata->user_callback(entry, metadata->user_data);
  }
}

static inline char _guppiraw_header_entry_is_END(const uint64_t* entry_uint64) {
  return entry_uint64[0] == KEY_UINT64_END &&
    entry_uint64[1] == _UINT64_BLANK &&
    entry_uint64[2] == _UINT64_BLANK &&
    entry_uint64[3] == _UINT64_BLANK &&
    entry_uint64[4] == _UINT64_BLANK &&
    entry_uint64[5] == _UINT64_BLANK &&
    entry_uint64[6] == _UINT64_BLANK &&
    entry_uint64[7] == _UINT64_BLANK &&
    entry_uint64[8] == _UINT64_BLANK &&
    entry_uint64[9] == _UINT64_BLANK;
}

void guppiraw_parse_blockheader_string(guppiraw_metadata_t* metadata, char* header_string, int64_t header_length) {
  int32_t entry_count = 0;
  while(
    !_guppiraw_header_entry_is_END((uint64_t*)header_string) && 
    ((++entry_count)*80 < header_length || header_length < 0)
  ) {
    _parse_entry(header_string, metadata);
    header_string += 80;
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
  while(!_guppiraw_header_entry_is_END((uint64_t*)entry) && header_entry_count < GUPPI_RAW_HEADER_MAX_ENTRIES) {

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
      gr_blockinfo->file_data_pos = guppiraw_directio_align(gr_blockinfo->file_data_pos);
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

    datashape->n_aspect = 1;
    if(datashape->n_ant > 0) {
      datashape->n_aspect = datashape->n_ant;
    }
    if(datashape->n_beam > 0) {
      datashape->n_aspect = datashape->n_beam;
    }
    datashape->n_aspectchan = datashape->n_obschan/datashape->n_aspect;

    datashape->bytestride_aspect = datashape->block_size/datashape->n_aspect;
    datashape->bytestride_channel = datashape->bytestride_aspect/datashape->n_aspectchan;
    
    datashape->bytestride_polarization = (2*datashape->n_bit)/8; // TODO assert > 0
    datashape->bytestride_time = datashape->n_pol*datashape->bytestride_polarization;

    datashape->n_time = datashape->bytestride_channel / datashape->bytestride_time;
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
int guppiraw_skim_file(guppiraw_file_info_t* gr_fileinfo) {
  guppiraw_block_info_t tmp_blockinfo = {0};//&gr_fileinfo->block_info;
  int fd = gr_fileinfo->fd;

  gr_fileinfo->bytesize_file = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  int rv = guppiraw_read_blockheader(fd, &tmp_blockinfo);
  if(rv){
    return rv;
  }
  guppiraw_seek_next_block(fd, &tmp_blockinfo);
  size_t bytesize_first_block = tmp_blockinfo.file_data_pos + guppiraw_directio_align(tmp_blockinfo.metadata.datashape.block_size);
  gr_fileinfo->n_block = (gr_fileinfo->bytesize_file + bytesize_first_block-1)/bytesize_first_block;

  gr_fileinfo->file_header_pos = malloc(gr_fileinfo->n_block * sizeof(off_t));
  gr_fileinfo->file_data_pos = malloc(gr_fileinfo->n_block * sizeof(off_t));
  gr_fileinfo->file_header_pos[0] = tmp_blockinfo.file_header_pos;
  gr_fileinfo->file_data_pos[0] = tmp_blockinfo.file_data_pos;

  memcpy(&gr_fileinfo->metadata, &tmp_blockinfo.metadata, sizeof(guppiraw_metadata_t));

  for(int i = 1; i < gr_fileinfo->n_block; i++) {
    rv = guppiraw_skim_blockheader(fd, &tmp_blockinfo);
    if(rv != 0) {
      rv *= i;
      gr_fileinfo->n_block = i;
      break;
    }
    gr_fileinfo->file_header_pos[i] = tmp_blockinfo.file_header_pos;
    gr_fileinfo->file_data_pos[i] = tmp_blockinfo.file_data_pos;
    guppiraw_seek_next_block(fd, &tmp_blockinfo);
  }

  return rv;
}

/*
 * Returns:
 *  -X: `read(...)` returned 0 before `GUPPI_RAW_HEADER_END_STR` for Block X
 *  0 : Successfully parsed the first and skimmed all the other headers in the file
 *  2 : First Header is inappropriate (missing `BLOCSIZE`)
 *  3 : Could not open the file `"%s.%04d.raw": gr_iterate->stempath, gr_iterate->fileenum`
 *  4 : Could not open the file `"%s.%04d.raw": gr_iterate->stempath, gr_iterate->fileenum` with O_DIRECT
 *  X : `GUPPI_RAW_HEADER_END_STR` not seen in `GUPPI_RAW_HEADER_MAX_ENTRIES` for Block X
 */
int _guppiraw_iterate_open(guppiraw_iterate_info_t* gr_iterate) {
  if(gr_iterate->file_info.fd > 0) {
    close(gr_iterate->file_info.fd);
    gr_iterate->fileenum++;
  }
  char* filepath = malloc(gr_iterate->stempath_len+9+1);
  sprintf(filepath, "%s.%04d.raw", gr_iterate->stempath, gr_iterate->fileenum%10000);
  gr_iterate->file_info.fd = open(filepath, O_RDONLY);
  if(gr_iterate->file_info.fd <= 0) {
    return 3;
  }
  gr_iterate->block_index = 0;
  int rv = guppiraw_skim_file(&gr_iterate->file_info);
  if(gr_iterate->file_info.metadata.directio) {
    close(gr_iterate->file_info.fd);
    gr_iterate->file_info.fd = open(filepath, O_RDONLY|O_DIRECT);
    if(gr_iterate->file_info.fd <= 0) {
      return 4;
    }
  }
  free(filepath);
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
int guppiraw_iterate_open_stem(const char* filepath, guppiraw_iterate_info_t* gr_iterate) {
  gr_iterate->stempath_len = strlen(filepath);

  // handle if filepath is not just the stempath
  gr_iterate->file_info.fd = open(filepath, O_RDONLY);
  if(gr_iterate->file_info.fd != -1) {
    close(gr_iterate->file_info.fd);

    // `filepath` is the `filestem.\d{4}.raw`
    gr_iterate->stempath_len = strlen(filepath)-9;
    gr_iterate->fileenum = atoi(filepath + gr_iterate->stempath_len + 1);
  }

  gr_iterate->stempath = malloc(gr_iterate->stempath_len+1);
  strncpy(gr_iterate->stempath, filepath, gr_iterate->stempath_len);
  gr_iterate->stempath[gr_iterate->stempath_len] = '\0';
  
  gr_iterate->file_info.fd = 0;
  return _guppiraw_iterate_open(gr_iterate);
}

typedef struct {
  struct iovec* iovecs;
  int iovec_count;
  off_t fd_offset;
} preadv_parameters_t;

static inline long _guppiraw_read_time_span(
  const guppiraw_iterate_info_t* gr_iterate,
  const size_t ntime, const size_t time_step,
  const size_t nchan, const size_t chan_step,
  const size_t naspect, const size_t aspect_step,
  const size_t time_step_stride,
  void* buffer
) {
  const guppiraw_datashape_t* datashape = &gr_iterate->file_info.metadata.datashape;
  if(ntime <= datashape->n_time) {
    // should never happen
    fprintf(
      stderr,
      "_guppiraw_read_time_span exclusively for reading across block_!\n"
    );
    exit(1);
  }
  
  const size_t aspect_steps = naspect/aspect_step;
  const size_t chan_steps = nchan/chan_step;
  const size_t time_steps = ntime/time_step;

  long bytes_read = 0;

  const long max_iovecs = sysconf(_SC_IOV_MAX);
  preadv_parameters_t pread_params = {0};
  pread_params.iovecs = malloc(max_iovecs * sizeof(struct iovec));
  pread_params.iovec_count = 0;

  for(size_t time_i = 0; time_i < time_steps; time_i++) {
    const size_t time_index = gr_iterate->time_index + time_i*time_step;

    const size_t fd_time_offset_chan_index = 
      gr_iterate->file_info.file_data_pos[gr_iterate->block_index + (time_index / datashape->n_time)] + 
        (time_index % datashape->n_time) * datashape->bytestride_time +
        (gr_iterate->chan_index)*datashape->bytestride_channel;

    for(size_t aspect_i = 0; aspect_i < aspect_steps; aspect_i++) {
      pread_params.fd_offset = fd_time_offset_chan_index + (gr_iterate->aspect_index+aspect_i)*datashape->bytestride_aspect;
      for(size_t chan_i = 0; chan_i < chan_steps; chan_i++) {
        pread_params.iovecs[pread_params.iovec_count].iov_len = time_step_stride;
        pread_params.iovecs[pread_params.iovec_count].iov_base = buffer + 
          ((aspect_i*chan_steps + chan_i)*time_steps + time_i)*time_step_stride;
        pread_params.iovec_count++;

        if(pread_params.iovec_count == max_iovecs) {
          const long bytes_preadv = preadv(
            gr_iterate->file_info.fd,
            pread_params.iovecs,
            pread_params.iovec_count,
            pread_params.fd_offset
          );
          if(bytes_preadv != time_step_stride*max_iovecs) {
            fprintf(
              stderr,
              "preadv(..., %d, %ld) errored: %ld (fd:%d @ %ld)\n\t",
              pread_params.iovec_count,
              pread_params.fd_offset,
              bytes_preadv,
              gr_iterate->file_info.fd,
              lseek(gr_iterate->file_info.fd, 0, SEEK_CUR)
            );
            perror("");
          }
          bytes_read += bytes_preadv;

          pread_params.iovec_count = 0;
          pread_params.fd_offset += time_step_stride*max_iovecs;
        }
      }

      // read any stragglers for this block, before fd_offset changes
      if(pread_params.iovec_count > 0) {
        const long bytes_preadv = preadv(
          gr_iterate->file_info.fd,
          pread_params.iovecs,
          pread_params.iovec_count,
          pread_params.fd_offset
        );
        if(bytes_preadv != time_step_stride*pread_params.iovec_count) {
          fprintf(
            stderr,
            "preadv(..., %d, %ld) errored: %ld (fd:%d @ %ld)\n\t",
            pread_params.iovec_count,
            pread_params.fd_offset,
            bytes_preadv,
            gr_iterate->file_info.fd,
            lseek(gr_iterate->file_info.fd, 0, SEEK_CUR)
          );
          perror("");
        }
        bytes_read += bytes_preadv;
        pread_params.iovec_count = 0;
      }
    }

  }

  free(pread_params.iovecs);
  return bytes_read;
}

static inline long _guppiraw_read_time_gap(
  const guppiraw_iterate_info_t* gr_iterate,
  const size_t ntime, const size_t time_step,
  const size_t nchan, const size_t chan_step,
  const size_t naspect, const size_t aspect_step,
  const size_t time_step_stride,
  void* buffer
) {
  const guppiraw_datashape_t* datashape = &gr_iterate->file_info.metadata.datashape;
  
  const size_t aspect_steps = naspect/aspect_step;
  const size_t chan_steps = nchan/chan_step;
  const size_t time_steps = ntime/time_step;
  long bytes_read = 0;

  for (size_t time_i = 0; time_i < time_steps; time_i++) {
    const size_t time_index = gr_iterate->time_index + time_i*time_step;
    for (size_t aspect_i = 0; aspect_i < aspect_steps; aspect_i++) {
      for (size_t chan_i = 0; chan_i < chan_steps; chan_i++) {
        lseek(
          gr_iterate->file_info.fd,
          gr_iterate->file_info.file_data_pos[gr_iterate->block_index + (time_index / datashape->n_time)] + 
            (time_index % datashape->n_time) * datashape->bytestride_time +
            (gr_iterate->chan_index + chan_i*chan_step) * datashape->bytestride_channel +
            (gr_iterate->aspect_index + aspect_i*aspect_step) * datashape->bytestride_aspect,
          SEEK_SET
        );
        const long _bytes_read = read(
          gr_iterate->file_info.fd,
          buffer + 
            ((aspect_i*chan_steps + chan_i)*time_steps + time_i)*time_step_stride,
          time_step_stride
        );
        if(_bytes_read != time_step_stride) {
          fprintf(
            stderr,
            "Did not read %lu bytes: %ld\n\ta=%lu c=%lu t=%lu: %lu\n\t@ %ld/%lu\n\t",
            time_step_stride,
            _bytes_read,
            aspect_i, chan_i, time_i,
            ((aspect_i*chan_steps + chan_i)*time_steps + time_i)*time_step_stride,
            lseek(gr_iterate->file_info.fd, 0, SEEK_CUR),
            gr_iterate->file_info.bytesize_file
          );
          perror("");
        }
        bytes_read += _bytes_read;
      }
    }
  }
  return bytes_read;
}

/*
 * Returns:
 *  -1: An error occurred, see stderr
 *  0 : Could not open the subsequent file
 *  X : Bytes read 
 */
long guppiraw_iterate_read(guppiraw_iterate_info_t* gr_iterate, const size_t ntime, const size_t nchan, const size_t naspect, void* buffer) {
  const guppiraw_metadata_t* metadata = &gr_iterate->file_info.metadata;  
  const guppiraw_datashape_t* datashape = &metadata->datashape;
  if(gr_iterate->chan_index + nchan > datashape->n_aspectchan) {
    // cannot gather in channel dimension
    fprintf(stderr, "Error: cannot gather in channel dimension.\n");
    return -1;
  }
  if(gr_iterate->aspect_index + naspect > datashape->n_aspect) {
    // cannot gather in aspect dimension
    fprintf(stderr, "Error: cannot gather in aspect dimension.\n");
    return -1;
  }
  if(gr_iterate->block_index == gr_iterate->file_info.n_block) {
    _guppiraw_iterate_open(gr_iterate);
    if(gr_iterate->file_info.fd <= 0) {
      return 0;
    }
  }
  // check that time request is a factor of remaining file_ntime
  if(guppiraw_iterate_filentime_remaining(gr_iterate) < ntime) {
    // TODO handle 2 files at a time.
    fprintf(
      stderr,
      "Error: remaining file_ntime (%d*%lu-%lu) is less than iteration time (%lu).\n",
      (gr_iterate->file_info.n_block - gr_iterate->block_index),
      gr_iterate->time_index,
      datashape->n_time,
      ntime
    );
    return -1;
  }

  long bytes_read = 0;
  if(buffer != NULL) {
    if(
      ntime == datashape->n_time && nchan == datashape->n_aspectchan && naspect == datashape->n_aspect &&
      gr_iterate->aspect_index == 0 && gr_iterate->chan_index == 0 && gr_iterate->time_index == 0
    ) {
      // plain and simple block verbatim read
      lseek(gr_iterate->file_info.fd, gr_iterate->file_info.file_data_pos[gr_iterate->block_index], SEEK_SET);
      bytes_read += read(
        gr_iterate->file_info.fd,
        buffer,
        metadata->directio ? guppiraw_directio_align(datashape->block_size) : datashape->block_size
      );
    }
    else {
      // interleave time-slice reads for different channels (maintain frequency as slowest axis)
      const size_t chan_step = ntime != datashape->n_time ? 1 : nchan;
      const size_t aspect_step = ntime != datashape->n_time || nchan != datashape->n_aspectchan ? 1 : naspect;

      // can read at most NTIME in the time dimension
      const size_t time_step = ntime > datashape->n_time ? datashape->n_time : ntime;

      const size_t time_step_stride = guppiraw_iterate_bytesize(gr_iterate, time_step, chan_step, aspect_step);
      if(metadata->directio && O_DIRECT != 0 && time_step_stride%512 != 0) {
        fprintf(
          stderr,
          "DIRECTIO (%d) enabled file cannot be read in increments of %lu: (%%512 != 0)\n",
          metadata->directio,
          time_step_stride
        );
        return -1;
      }

      if ((gr_iterate->time_index + ntime - 1)/datashape->n_time == 0) {
        // if time iteration spans < 1 block, iovecs striding is useless
        //   because iovecs spread contiguous file-data, and the
        //   file-data is being broken up at the time index with only
        //   the [Pol, Sample] dimensions being smaller.
        bytes_read += _guppiraw_read_time_gap(
          gr_iterate,
          ntime, time_step,
          nchan, chan_step,
          naspect, aspect_step,
          time_step_stride,
          buffer
        );
      }
      else {
        bytes_read += _guppiraw_read_time_span(
          gr_iterate,
          ntime, time_step,
          nchan, chan_step,
          naspect, aspect_step,
          time_step_stride,
          buffer
        );
      }
    }
  }
  
  if(gr_iterate->chan_index + nchan >= datashape->n_aspectchan) {
    if(gr_iterate->aspect_index + naspect >= datashape->n_aspect) {
      if(gr_iterate->time_index + ntime >= datashape->n_time) {
        gr_iterate->block_index += (gr_iterate->time_index + ntime) / datashape->n_time;
      }
      gr_iterate->time_index = (gr_iterate->time_index + ntime) % datashape->n_time;
    }
    gr_iterate->aspect_index = (gr_iterate->aspect_index + naspect) % datashape->n_aspect;
  }
  gr_iterate->chan_index = (gr_iterate->chan_index + nchan) % datashape->n_aspectchan;

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

int _guppiraw_header_put_integer(guppiraw_header_llnode_t* head, const char* key, const int64_t value) {
  memset(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, ' ', 80);
  hputi8(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, key, value);
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
int guppiraw_header_put_integer(guppiraw_header_t* header, const char* key, const int64_t value) {
  _guppiraw_header_ensure_initialised(header, key);
  header->n_entries += _guppiraw_header_put_integer(header->head, key, value);
  return 0;
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

char* guppiraw_header_malloc_string(const guppiraw_header_t* header, const char directio) {
  const int n_entries = header->n_entries;
  const size_t header_entries_len = (n_entries + 1) * 80;
  guppiraw_header_llnode_t* header_entry = header->head;
  char* header_string;
  if(directio) {
    const size_t header_entries_len_aligned = guppiraw_directio_align(header_entries_len);
    header_string = memalign(512, header_entries_len_aligned);
    memcpy(
      header_string + header_entries_len,
      _guppiraw_directio_padding_buffer,
      header_entries_len_aligned - header_entries_len
    );
  }
  else {
    header_string = malloc(header_entries_len);
  }

  for(int i = 0; i < n_entries; i++) {
    memcpy(header_string + i*80, header_entry->keyvalue, 80);
    header_entry = header_entry->next;
  }
  memcpy(header_string + n_entries*80, GUPPI_RAW_HEADER_END_STR, 80);
  return header_string;
}

ssize_t guppiraw_write_block(const int fd, const guppiraw_header_t* header, const void* data, const uint32_t block_size, const char directio) {
  char* header_string = guppiraw_header_malloc_string(header, directio);
  const size_t header_entries_len = (header->n_entries+1) * 80;
  const size_t header_string_len = directio ? guppiraw_directio_align(header_entries_len) : header_entries_len;

  struct iovec* block_iovecs = malloc(2*sizeof(struct iovec));
  block_iovecs[0].iov_base = header_string;
  block_iovecs[0].iov_len = header_string_len;

  block_iovecs[1].iov_base = data;
  block_iovecs[1].iov_len = directio ? guppiraw_directio_align(block_size) : block_size;

  ssize_t bytes_written = writev(fd, block_iovecs, 2);
  free(header_string);
  return bytes_written;
}
