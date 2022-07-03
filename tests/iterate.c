#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "guppiraw.h"

size_t validate_iteration(guppiraw_iterate_info_t *gr_iterate, size_t ntime, size_t nchan, size_t repeat_time) {
  const guppiraw_datashape_t *datashape = &gr_iterate->file_info.block_info.datashape;

  // validate all channels
  const size_t repeat_chan = datashape->n_obschan/nchan;
  const size_t repetitions = repeat_time * repeat_chan;

  const size_t time_index = gr_iterate->time_index;
  const size_t chan_index = gr_iterate->chan_index;

  const size_t pol_sample_bytes = guppiraw_iterate_bytesize(gr_iterate, 1, 1);
  size_t bytes_per_iter = guppiraw_iterate_bytesize(gr_iterate, ntime, nchan);
  size_t bytes_invalid = repetitions*bytes_per_iter;
  char *iterate_buffer = malloc(bytes_per_iter);

  const size_t nblocks = ((time_index + repeat_time*ntime + datashape->n_time-1)/datashape->n_time);
  char *data_blocks = malloc(datashape->block_size*nblocks);
  
  const off_t gr_filepos = lseek(gr_iterate->fd, 0, SEEK_CUR);

  for(int block_i = 0; block_i < nblocks; block_i++) {
    lseek(
      gr_iterate->fd,
      gr_iterate->file_info.file_data_pos[gr_iterate->block_index + block_i],
      SEEK_SET
    );
    read(gr_iterate->fd, data_blocks + block_i*datashape->block_size, datashape->block_size);
  }
  lseek(gr_iterate->fd, gr_filepos, SEEK_SET);
  
  size_t chan_offset, d_t, time_offset;
  char *iterate_buffer_ptr;
  for(size_t rt = 0; rt < repeat_time; rt++) {
    for(size_t rc = 0; rc < repeat_chan; rc++) {

      if(
        guppiraw_iterate_read(
          gr_iterate,
          ntime,
          nchan,
          iterate_buffer
        ) == bytes_per_iter
      ) {
        iterate_buffer_ptr = iterate_buffer;
        for (size_t c = 0; c < nchan; c++) {
          chan_offset = (chan_index + c + rc*nchan)*datashape->bytestride_frequency;

          for (size_t t = 0; t < ntime; t++) {
            d_t = time_index + t + rt*ntime;
            time_offset = (d_t%datashape->n_time)*datashape->bytestride_time + (d_t/datashape->n_time)*datashape->block_size;

            for (size_t b = 0; b < pol_sample_bytes; b++) {
              if(data_blocks[chan_offset + time_offset + b] == *iterate_buffer_ptr++) {
                bytes_invalid--;
              }
            }
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
  if(argc != 2) {
    printf(
      "Usage: `%s input_filepath`\n",
      argv[0]
    );
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

            // Read at least 2 blocks, all channels, each time.
            const size_t time_repetitions = multiply_not_divide ? 2 : 2*factors[ti];
            if(ntime*time_repetitions > guppiraw_iterate_filentime_remaining(&gr_iterate)) {
              // Reset to beginning of file
              printf("Resetting iterations to beginning of file!\n");
              gr_iterate.block_index = 0;
            }
            printf(
              "Iteration (x%lu): block #%d[c=%lu,t=%lu] time=%lu (%d), chan=%lu (%d)...",
              time_repetitions,
              gr_iterate.block_index,
              gr_iterate.chan_index, gr_iterate.time_index,
              ntime, factors[ti],
              nchan, factors[ci]
            );

            const size_t bytes_invalid = validate_iteration(&gr_iterate, ntime, nchan, time_repetitions);
            if(bytes_invalid != 0){
              if(guppiraw_iterate_filentime_remaining(&gr_iterate) < ntime) {
                return 0;
              }
              printf("failed (%lu/%lu bytes invalid).\n",
                bytes_invalid, datashape->block_size
              );
              return 1;
            }
            else {
              printf("passed\n");
            }

          }
        }

      }
    }
  }

  return 0;
}