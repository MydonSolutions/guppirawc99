#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "guppiraw.h"

size_t validate_iteration(guppiraw_iterate_info_t *gr_iterate, size_t ntime, size_t nchan) {
  const size_t chan_offset = gr_iterate->chan_index;
  const size_t time_offset = gr_iterate->time_index;
  size_t bytes_invalid = guppiraw_iterate_bytesize(gr_iterate, ntime, nchan);
  
  const off_t gr_filepos = lseek(gr_iterate->fd, 0, SEEK_CUR);

  const guppiraw_datashape_t *datashape = &gr_iterate->file_info.block_info.datashape;

  const size_t nblocks = (time_offset + ntime + datashape->n_time-1)/datashape->n_time;

  char *data_blocks = malloc(datashape->block_size*nblocks);
  for(int block_i = 0; block_i < nblocks; block_i++) {
    lseek(
      gr_iterate->fd,
      gr_iterate->file_info.file_data_pos[gr_iterate->block_index + block_i],
      SEEK_SET
    );
    read(gr_iterate->fd, data_blocks + block_i*datashape->block_size, datashape->block_size);
  }
  lseek(gr_iterate->fd, gr_filepos, SEEK_SET);

  char *iterate_buffer = malloc(bytes_invalid);
  if(guppiraw_iterate_read(
      gr_iterate,
      ntime,
      nchan,
      iterate_buffer
    ) == bytes_invalid
  ) {
    size_t pol_sample_bytes = guppiraw_iterate_bytesize(gr_iterate, 1, 1);
    char *iterate_buffer_ptr = iterate_buffer;
    for (size_t c = chan_offset; c < chan_offset + nchan; c++) {
      const size_t chan_offset = c*datashape->bytestride_frequency;

      for (size_t t = time_offset; t < time_offset + ntime; t++) {
        const size_t time_offset = (t%datashape->n_time)*datashape->bytestride_time + (t/datashape->n_time)*datashape->block_size;

        for (size_t b = 0; b < pol_sample_bytes; b++) {
          if(data_blocks[chan_offset + time_offset + b] == *iterate_buffer_ptr++) {
            bytes_invalid--;
          }
        }
      }
    }
  }

  free(data_blocks);
  free(iterate_buffer);
  return bytes_invalid;
}

int main(int argc, char const *argv[])
{
  if (argc != 2){
    fprintf(stderr, "Provide the input path.\n");
    return 1;
  }

  guppiraw_iterate_info_t gr_iterate = {0};
  if(guppiraw_iterate_open_stem(argv[1], &gr_iterate)) {
    printf("Could not open: %s.%04d.raw\n", gr_iterate.stempath, gr_iterate.fileenum);
    return 1;
  }

  const int factors[] = {1, 2, 3, 4, 5, 7, 8, 11, 13, 16, 17, 19};
  const int nfactors = sizeof(factors)/sizeof(int);

  size_t ntime, nchan;
  guppiraw_datashape_t *datashape = &gr_iterate.file_info.block_info.datashape;

  for(int multiply_not_divide = 0; multiply_not_divide <= 1; multiply_not_divide++) {
    for(int ci = 0; ci < nfactors; ci++) {
      if(datashape->n_obschan % factors[ci] == 0) {
        nchan = datashape->n_obschan / factors[ci];

        for(int ti = multiply_not_divide; ti < nfactors; ti++) {
          if(multiply_not_divide || datashape->n_time % factors[ti] == 0) {
            ntime = multiply_not_divide ? datashape->n_time * factors[ti] : datashape->n_time / factors[ti];

            for(int repeat = 0; repeat < (multiply_not_divide ? 2 : 2*factors[ti]); repeat ++) {
              fprintf(
                stderr,
                "Iteration (%d/%d): block #%d.c=%lu.t=%lu time=%lu (%d), chan=%lu (%d)...",
                repeat, multiply_not_divide ? 2 : 2*factors[ti],
                gr_iterate.block_index,
                gr_iterate.chan_index, gr_iterate.time_index,
                ntime, factors[ti],
                nchan, factors[ci]
              );

              const size_t bytes_invalid = validate_iteration(&gr_iterate, ntime, nchan);
              if(bytes_invalid != 0){
                if(guppiraw_iterate_filentime_remaining(&gr_iterate) < ntime) {
                  return 0;
                }
                fprintf(stderr, "failed (%lu/%lu bytes invalid).\n",
                  bytes_invalid, datashape->block_size
                );
                return 1;
              }
              else {
                fprintf(stderr, "passed\n");
              }
            }

          }
        }

        gr_iterate.chan_index = 0; // reset
      }
    }
  }

  return 0;
}