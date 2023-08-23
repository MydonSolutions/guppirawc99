#include "guppirawc99/iterate.h"

typedef struct _file_info_ll {
  guppiraw_file_info_t file_info;
  struct _file_info_ll *next;
} _file_info_ll_t;

int _guppiraw_iterate_open(
  guppiraw_iterate_info_t* gr_iterate,
  size_t user_datasize,
  guppiraw_header_entry_parser user_callback
) {
  int rv = 0;
  _file_info_ll_t *file_cur = NULL, *file_ll_head = NULL;

  char* filepath = malloc(gr_iterate->stempath_len+9+1);
  int fd;
  gr_iterate->n_block = 0;
  const int n_file_limit = gr_iterate->n_file <= 0 ? 10000 : gr_iterate->n_file;
  for(gr_iterate->n_file = 0; gr_iterate->n_file < n_file_limit; gr_iterate->n_file++) {
    sprintf(filepath, "%s.%04d.raw", gr_iterate->stempath, gr_iterate->n_file + gr_iterate->fileenum_offset);
    fd = open(filepath, O_RDONLY);
    if(fd <= 0) {
      break;
    }

    if(gr_iterate->n_file > 0) {
      file_cur->next = malloc(sizeof(_file_info_ll_t));
      file_cur = file_cur->next;
    }
    else {
      file_ll_head = malloc(sizeof(_file_info_ll_t));
      file_cur = file_ll_head;
    }

    memset(file_cur, 0, sizeof(_file_info_ll_t));
    file_cur->file_info.fd = fd;
    if(user_datasize > 0) {
      file_cur->file_info.metadata.user_data = malloc(user_datasize);
      file_cur->file_info.metadata.user_callback = user_callback;
    }

    rv = guppiraw_skim_file(&file_cur->file_info);
    if(rv != 0) {
      break;
    }
    gr_iterate->n_block += file_cur->file_info.n_block;

    if(file_cur->file_info.metadata.directio) {
      close(file_cur->file_info.fd);
      file_cur->file_info.fd = open(filepath, O_RDONLY|O_DIRECT);
      if(file_cur->file_info.fd <= 0) {
        rv = -2;
        break;
      }
    }
  }

  // Collate `file_info` linked-list under array.
  gr_iterate->file_info = malloc(gr_iterate->n_file*sizeof(guppiraw_file_info_t));
  gr_iterate->block_location = malloc(gr_iterate->n_block*sizeof(guppiraw_block_location_t));

	guppiraw_block_location_t* cur_block_loc = gr_iterate->block_location;
	int b;
  for(int i = 0; i < gr_iterate->n_file; i++) {
    memcpy(gr_iterate->file_info + i, &file_ll_head->file_info, sizeof(guppiraw_file_info_t));
    (gr_iterate->file_info + i)->metadata.user_data = file_ll_head->file_info.metadata.user_data;

		for(b = 0; b < file_ll_head->file_info.n_block; b++) {
			cur_block_loc->file_index = i;
			cur_block_loc->fileblock_index = b;
			cur_block_loc++;
		}

    file_cur = file_ll_head;
    file_ll_head = file_ll_head->next;
    free(file_cur);
  }

  free(filepath);
  return rv;
}

int guppiraw_iterate_open_with_user_metadata(
  guppiraw_iterate_info_t* gr_iterate,
  const char* filepath,
  size_t user_datasize,
  guppiraw_header_entry_parser user_callback
) {
  gr_iterate->fileenum_offset = 0;
  gr_iterate->stempath_len = strlen(filepath);

  // handle if filepath is not just the stempath
  int fd = open(filepath, O_RDONLY);
  if(fd != -1) {
    close(fd);

    // `filepath` is the `filestem.\d{4}.raw`
    gr_iterate->stempath_len = strlen(filepath)-9;
    gr_iterate->fileenum_offset = atoi(filepath + gr_iterate->stempath_len + 1);
    // initial file is the one provided, the file limit is assumed to be set in gr_iterate->n_file
    //  <= 0 for no limit...
  }

  gr_iterate->stempath = malloc(gr_iterate->stempath_len+1);
  strncpy(gr_iterate->stempath, filepath, gr_iterate->stempath_len);
  gr_iterate->stempath[gr_iterate->stempath_len] = '\0';

  int rv = _guppiraw_iterate_open(gr_iterate, user_datasize, user_callback);
  if(rv == 0 && gr_iterate->n_file == 0) {
    return 3;
  }
  return rv;
}

void guppiraw_iterate_close(guppiraw_iterate_info_t* gr_iterate) {
  for (size_t i = 0; i < gr_iterate->n_file; i++) {
    close(gr_iterate->file_info[i].fd);
  }
  free(gr_iterate->file_info);
  free(gr_iterate->block_location);
  free(gr_iterate->stempath);
}

int guppiraw_iterate_file_index_of_block(const guppiraw_iterate_info_t* gr_iterate, int* block_index) {
	int file_index;
	if(*block_index >= gr_iterate->n_block) {
		file_index = gr_iterate->block_location[gr_iterate->n_block-1].file_index;
		*block_index = (*block_index - gr_iterate->n_block) + guppiraw_iterate_file_info(gr_iterate, file_index)->n_block;
	}
	else {
		file_index = gr_iterate->block_location[*block_index].file_index;
		*block_index = gr_iterate->block_location[*block_index].fileblock_index;
	}
	return file_index;
}

int guppiraw_iterate_file_index_of_block_offset(const guppiraw_iterate_info_t* gr_iterate, int* block_index) {
  *block_index += gr_iterate->block_index;
  int file_index = guppiraw_iterate_file_index_of_block(gr_iterate, block_index);
	*block_index -= guppiraw_iterate_file_info(gr_iterate, file_index)->block_index;
	return file_index;
}

guppiraw_file_info_t* guppiraw_iterate_file_info_of_block(
  const guppiraw_iterate_info_t* gr_iterate, int* block_index
) {
  return *block_index < gr_iterate->n_block ?
		gr_iterate->file_info + guppiraw_iterate_file_index_of_block(gr_iterate, block_index) :
		NULL
	;
}

guppiraw_file_info_t* guppiraw_iterate_file_info_of_block_offset(
  const guppiraw_iterate_info_t* gr_iterate, int* block_index
) {
  *block_index += gr_iterate->block_index;
  guppiraw_file_info_t* rv = guppiraw_iterate_file_info_of_block(gr_iterate, block_index);
  if(rv != NULL) {
    *block_index -= rv->block_index;
  }
  return rv;
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
  const guppiraw_datashape_t* datashape = &guppiraw_iterate_metadata(gr_iterate)->datashape;
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

  guppiraw_file_info_t* file_info;
  int fileblock_indexoffset;

  for(size_t time_i = 0; time_i < time_steps; time_i++) {
    const size_t time_index = gr_iterate->time_index + time_i*time_step;
    fileblock_indexoffset = time_index / datashape->n_time;
    file_info = guppiraw_iterate_file_info_of_block_offset(gr_iterate, &fileblock_indexoffset);
    if(file_info == NULL) {
      return -fileblock_indexoffset;
    }

    const size_t fd_time_offset_chan_index = 
      guppiraw_file_data_pos_offset(file_info, fileblock_indexoffset) + 
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
            file_info->fd,
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
              file_info->fd,
              lseek(file_info->fd, 0, SEEK_CUR)
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
          file_info->fd,
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
            file_info->fd,
            lseek(file_info->fd, 0, SEEK_CUR)
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
  const guppiraw_datashape_t* datashape = &guppiraw_iterate_metadata(gr_iterate)->datashape;
  
  const size_t aspect_steps = naspect/aspect_step;
  const size_t chan_steps = nchan/chan_step;
  const size_t time_steps = ntime/time_step;
  long bytes_read = 0;

  guppiraw_file_info_t* file_info;
  int fileblock_indexoffset;

  for (size_t time_i = 0; time_i < time_steps; time_i++) {
    const size_t time_index = gr_iterate->time_index + time_i*time_step;
    fileblock_indexoffset = time_index / datashape->n_time;
    file_info = guppiraw_iterate_file_info_of_block_offset(gr_iterate, &fileblock_indexoffset);
    if(file_info == NULL) {
      return -fileblock_indexoffset;
    }

    for (size_t aspect_i = 0; aspect_i < aspect_steps; aspect_i++) {
      for (size_t chan_i = 0; chan_i < chan_steps; chan_i++) {
        lseek(
          file_info->fd,
          guppiraw_file_data_pos_offset(file_info, fileblock_indexoffset) + 
            (time_index % datashape->n_time) * datashape->bytestride_time +
            (gr_iterate->chan_index + chan_i*chan_step) * datashape->bytestride_channel +
            (gr_iterate->aspect_index + aspect_i*aspect_step) * datashape->bytestride_aspect,
          SEEK_SET
        );
        const long _bytes_read = read(
          file_info->fd,
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
            lseek(file_info->fd, 0, SEEK_CUR),
            file_info->bytesize_file
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
long guppiraw_iterate_peek(const guppiraw_iterate_info_t* gr_iterate, const size_t ntime, const size_t nchan, const size_t naspect, void* buffer) {
  const guppiraw_file_info_t* file_info = guppiraw_iterate_file_info_current(gr_iterate);  
  const guppiraw_metadata_t* metadata = &file_info->metadata;  
  const guppiraw_datashape_t* datashape = &metadata->datashape;
	if((datashape->n_aspectchan - gr_iterate->chan_index) % nchan != 0) {
    fprintf(
			stderr,
			"Error: channel dimension step is not a factor: n_aspectchan - chan_index (%u - %ld) %% (%ld) nchan != 0.\n",
			datashape->n_aspectchan, gr_iterate->chan_index, nchan
		);
    return -1;
	}
	if((datashape->n_aspect - gr_iterate->aspect_index) % naspect != 0) {
    fprintf(
			stderr,
			"Error: aspect dimension step is not a factor: n_aspect - aspect_index (%u - %ld) %% (%ld) naspect != 0.\n",
			datashape->n_aspect, gr_iterate->aspect_index, naspect
		);
    return -1;
	}

  if(gr_iterate->chan_index + nchan > datashape->n_aspectchan) {
    // cannot gather in channel dimension
    fprintf(
			stderr,
			"Error: cannot gather in channel dimension: chan_index + nchan (%ld + %ld) > (%u) n_aspectchan.\n",
			gr_iterate->chan_index, nchan, datashape->n_aspectchan
		);
    return -1;
  }
  if(gr_iterate->aspect_index + naspect > datashape->n_aspect) {
    // cannot gather in aspect dimension
    fprintf(
			stderr,
			"Error: cannot gather in aspect dimension: aspect_index + naspect (%ld + %ld) > (%u) n_aspect.\n",
			gr_iterate->aspect_index, naspect, datashape->n_aspect
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
      lseek(file_info->fd, guppiraw_file_data_pos_current(file_info), SEEK_SET);
      bytes_read += read(
        file_info->fd,
        buffer,
        metadata->directio ? guppiraw_calc_directio_aligned(datashape->block_size) : datashape->block_size
      );
    }
    else {
      // interleave time-slice reads for different channels (maintain channel as slowest axis)
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

        // check that time request is a factor of remaining file_ntime
        if(guppiraw_file_ntime_remaining(file_info) < ntime) {
          fprintf(
            stderr,
            "Error: remaining file_ntime (%d*%lu-%lu) is less than iteration time (%lu).\n",
            (file_info->n_block - file_info->block_index),
            datashape->n_time,
            gr_iterate->time_index,
            ntime
          );
          return -1;
        }

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
  return bytes_read;
}

uint8_t guppiraw_iterate_increment_in_time(guppiraw_iterate_info_t* gr_iterate, const size_t ntime, const size_t nchan, const size_t naspect) {
  const guppiraw_file_info_t* file_info = guppiraw_iterate_file_info_current(gr_iterate);  
  const guppiraw_metadata_t* metadata = &file_info->metadata;  
  const guppiraw_datashape_t* datashape = &metadata->datashape;
  uint8_t exhausted = 0;

  if(gr_iterate->time_index + ntime >= datashape->n_time) {
    const int block_increment = (gr_iterate->time_index + ntime) / datashape->n_time;
    int fileblock_offset = block_increment;
    const int file_index = guppiraw_iterate_file_index_of_block_offset(gr_iterate, &fileblock_offset);
    for(; gr_iterate->file_index != file_index; gr_iterate->file_index++) {
      gr_iterate->file_info[gr_iterate->file_index].block_index = gr_iterate->file_info[gr_iterate->file_index].n_block;
    }
    gr_iterate->block_index += block_increment;
    gr_iterate->file_info[gr_iterate->file_index].block_index += fileblock_offset;

    if (gr_iterate->block_index >= gr_iterate->n_block) {
      exhausted = 1;
      guppiraw_iterate_reset_time(gr_iterate);

      // iterate slower dimensions
      if(gr_iterate->aspect_index + naspect >= datashape->n_aspect) {
        if(gr_iterate->chan_index + nchan >= datashape->n_aspectchan) {
        }
        gr_iterate->chan_index = (gr_iterate->chan_index + nchan) % datashape->n_aspectchan;
      }
      gr_iterate->aspect_index = (gr_iterate->aspect_index + naspect) % datashape->n_aspect;
    }
  }
  gr_iterate->time_index = (gr_iterate->time_index + ntime) % datashape->n_time;

  return exhausted;
}

uint8_t guppiraw_iterate_increment_in_channel(guppiraw_iterate_info_t* gr_iterate, const size_t ntime, const size_t nchan, const size_t naspect) {
  const guppiraw_file_info_t* file_info = guppiraw_iterate_file_info_current(gr_iterate);  
  const guppiraw_metadata_t* metadata = &file_info->metadata;  
  const guppiraw_datashape_t* datashape = &metadata->datashape;
  uint8_t exhausted = 0;

  if(gr_iterate->chan_index + nchan >= datashape->n_aspectchan) {
    exhausted = 1;
    if(gr_iterate->aspect_index + naspect >= datashape->n_aspect) {
      if(gr_iterate->time_index + ntime >= datashape->n_time) {

        const int block_increment = (gr_iterate->time_index + ntime) / datashape->n_time;
        int fileblock_offset = block_increment;
        const int file_index = guppiraw_iterate_file_index_of_block_offset(gr_iterate, &fileblock_offset);
        for(; gr_iterate->file_index != file_index; gr_iterate->file_index++) {
          gr_iterate->file_info[gr_iterate->file_index].block_index = gr_iterate->file_info[gr_iterate->file_index].n_block;
        }
        gr_iterate->block_index += block_increment;
        gr_iterate->file_info[gr_iterate->file_index].block_index += fileblock_offset;
      }
      gr_iterate->time_index = (gr_iterate->time_index + ntime) % datashape->n_time;
    }
    gr_iterate->aspect_index = (gr_iterate->aspect_index + naspect) % datashape->n_aspect;
  }
  gr_iterate->chan_index = (gr_iterate->chan_index + nchan) % datashape->n_aspectchan;

  return exhausted;
}

void guppiraw_iterate_set_time_index(guppiraw_iterate_info_t* gr_iterate, const size_t block_index, const size_t time_index) {
  gr_iterate->block_index = 0;
  gr_iterate->file_index = 0;
  gr_iterate->time_index = time_index;
  
  size_t file_n_block;
  for (int file_i = 0; file_i < gr_iterate->n_file; file_i++) {
    file_n_block = gr_iterate->file_info[file_i].n_block;
    if (gr_iterate->block_index + file_n_block <= block_index) {
      // not in this file
      gr_iterate->file_index += 1;
      gr_iterate->block_index += file_n_block;
      gr_iterate->file_info[file_i].block_index = file_n_block;
    }
    else if (file_i == gr_iterate->file_index) {
      // settle on this block
      gr_iterate->file_info[gr_iterate->file_index].block_index = block_index - gr_iterate->block_index;
      gr_iterate->block_index = block_index;
    }
    else {
      // previously settled, this file is reset
      gr_iterate->file_info[file_i].block_index = 0;
    }
  }
}
