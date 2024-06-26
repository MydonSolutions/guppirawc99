#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "guppirawc99/header.h"
#include "guppirawc99/iterate.h"

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

typedef struct {
  double chan_bw;
  double tbin;
  int test_opener;
} guppiraw_block_meta_t;

const uint64_t KEY_UINT64_CHAN_BW  = GUPPI_RAW_KEY_UINT64_ID_LE('C','H','A','N','_','B','W',' ');

void guppiraw_parse_block_meta(const char* entry, void* block_meta) {
  if(((uint64_t*)entry)[0] == KEY_UINT64_CHAN_BW) {
    hgetr8(entry, "CHAN_BW", &((guppiraw_block_meta_t*)block_meta)->chan_bw);
    ((guppiraw_block_meta_t*)block_meta)->tbin = 1.0/((guppiraw_block_meta_t*)block_meta)->chan_bw;
  } else if (guppiraw_header_entry_is_OPENER((uint64_t*)entry)) {
    ((guppiraw_block_meta_t*)block_meta)->test_opener = 314159265;
  }
}

long validate_iteration(guppiraw_iterate_info_t *gr_iterate, size_t ntime, size_t nchan, size_t naspect, size_t repeat_time) {
  const guppiraw_datashape_t *datashape = guppiraw_iterate_datashape(gr_iterate);

  // validate all channels
  const size_t repeat_aspect = datashape->n_aspect/naspect;
  const size_t repeat_chan = datashape->n_aspectchan/nchan;
  const size_t repetitions = repeat_time * repeat_chan * repeat_aspect;

  const size_t time_index = gr_iterate->time_index;
  const size_t chan_index = gr_iterate->chan_index;
  const size_t aspect_index = gr_iterate->aspect_index;

  const size_t pol_sample_bytes = guppiraw_iterate_bytesize(gr_iterate, 1, 1, 1);
  const size_t bytes_per_iter = guppiraw_iterate_bytesize(gr_iterate, ntime, nchan, naspect);

  long bytes_invalid = repetitions*bytes_per_iter;
  char *iterate_buffer __attribute__ ((aligned (512))) = memalign(
    512,
    guppiraw_calc_directio_aligned(bytes_per_iter)
  );

  const size_t nblocks = ((time_index + repeat_time*ntime + datashape->n_time-1)/datashape->n_time);
  char *data_blocks __attribute__ ((aligned (512))) = memalign(
    512,
    guppiraw_calc_directio_aligned(datashape->block_size*nblocks)
  );
  
  guppiraw_file_info_t *file_info;
  int fileblock_indexoffset;
  for(int block_i = 0; block_i < nblocks; block_i++) {
    fileblock_indexoffset = block_i;
    file_info = guppiraw_iterate_file_info_of_block_offset(gr_iterate, &fileblock_indexoffset);
    lseek(
      file_info->fd,
      guppiraw_file_data_pos_offset(file_info, fileblock_indexoffset),
      SEEK_SET
    );
    read(file_info->fd, data_blocks + block_i*datashape->block_size, datashape->block_size);
  }
  
  size_t aspect_offset, chan_offset, d_t, time_offset;
  char *iterate_buffer_ptr;
  for(size_t rt = 0; rt < repeat_time; rt++) {
    for(size_t ra = 0; ra < repeat_aspect; ra++) {
      for(size_t rc = 0; rc < repeat_chan; rc++) {
        const long bytes_read = guppiraw_iterate_read(
          gr_iterate,
          ntime,
          nchan,
          naspect,
          iterate_buffer
        );
        if(bytes_read == bytes_per_iter) {
          iterate_buffer_ptr = iterate_buffer;
          for (size_t a = 0; a < naspect; a++) {
            aspect_offset = (aspect_index + a + ra*naspect)*datashape->bytestride_aspect;
            for (size_t c = 0; c < nchan; c++) {
              chan_offset = (chan_index + c + rc*nchan)*datashape->bytestride_channel;

              for (size_t t = 0; t < ntime; t++) {
                d_t = time_index + t + rt*ntime;
                time_offset = (d_t%datashape->n_time)*datashape->bytestride_time + (d_t/datashape->n_time)*datashape->block_size;

                for (size_t b = 0; b < pol_sample_bytes; b++) {
                  if(data_blocks[aspect_offset + chan_offset + time_offset + b] == *iterate_buffer_ptr++) {
                    bytes_invalid--;
                  }
                }

              }
            }
          }
        }
        else {
          fprintf(
            stderr,
            "Iteration didn't read the correct number of bytes: %ld/%lu\n",
            bytes_read, 
            bytes_per_iter
          );
        }

      }
    }
  }

  free(data_blocks);
  free(iterate_buffer);
  return bytes_invalid;
}

long benchmark_iteration(guppiraw_iterate_info_t *gr_iterate, size_t ntime, size_t nchan, size_t naspect, size_t repeat_time) {
  const guppiraw_datashape_t *datashape = guppiraw_iterate_datashape(gr_iterate);
  size_t bytesize = guppiraw_iterate_bytesize(gr_iterate, ntime, nchan, naspect);

  // validate all channels
  const size_t repeat_aspect = datashape->n_aspect/naspect;
  const size_t repeat_chan = datashape->n_aspectchan/nchan;
  const size_t repetitions = repeat_time * repeat_chan * repeat_aspect;
  
  void *buffer __attribute__ ((aligned (512))) = memalign(
    512,
    guppiraw_calc_directio_aligned(bytesize)
  );
  struct timespec start, stop;
	uint64_t reading_ns = 0;

  size_t repitition;
  size_t rv;
  for(repitition = 0; repitition < repetitions && gr_iterate->block_index <= gr_iterate->n_block; repitition++) {
    clock_gettime(CLOCK_MONOTONIC, &start);
      rv = guppiraw_iterate_read(
        gr_iterate,
        ntime,
        nchan,
        naspect,
        buffer
      );
		clock_gettime(CLOCK_MONOTONIC, &stop);
		reading_ns += ELAPSED_NS(start, stop);
    if(rv != bytesize) {
      fprintf(stderr, "ERROR (%ld)!\n\t", rv);
      perror("");
      return 1;
    }
  };

  printf("iterations: %lu / %f s (%f GB/s)\n",
    repitition,
    (double)reading_ns/1e9,
    (double)repitition*bytesize/(double)reading_ns
  );
  
  free(buffer);
  return 0;
}

int main(int argc, char const *argv[])
{
  int benchmark_not_validate = 0;
  if(argc != 2) {
    if(!(argc == 3 && strncmp("-V", argv[1], 2)) == 0) {
      fprintf(
        stderr,
        "Usage: `%s [options] input_filepath`\n"\
        "options:\n"\
        "\t-V\t\tDo not validate iterations, benchmark instead.\n",
        argv[0]
      );
      return 1;
    }
    benchmark_not_validate = 1;
  }

  guppiraw_iterate_info_t gr_iterate = {0};
  
  if(guppiraw_iterate_open_with_user_metadata(&gr_iterate, argv[argc-1], sizeof(guppiraw_block_meta_t), guppiraw_parse_block_meta)) {
    printf("Error opening: %s.%04d.raw\n", gr_iterate.stempath, gr_iterate.fileenum_offset);
    return 1;
  }

  assert(
    abs(((guppiraw_block_meta_t*)guppiraw_iterate_metadata(&gr_iterate)->user_data)->chan_bw - 3.1415926535) < 1e-3
  );

  assert(
    ((guppiraw_block_meta_t*)guppiraw_iterate_metadata(&gr_iterate)->user_data)->test_opener == 314159265
  );

  const int factors[] = {1, 2, 3, 4, 5, 7, 8, 16};
  const int nfactors = sizeof(factors)/sizeof(int);

  size_t ntime, nchan, naspect;
  guppiraw_datashape_t *datashape = guppiraw_iterate_datashape(&gr_iterate);

  for(int multiply_not_divide = 0; multiply_not_divide <= 1; multiply_not_divide++) {
    for(int ai = 0; ai < nfactors; ai++) {
      if(datashape->n_aspect % factors[ai] == 0) {
        naspect = datashape->n_aspect / factors[ai];

        for(int ci = 0; ci < nfactors; ci++) {
          if(datashape->n_aspectchan % factors[ci] == 0) {
            nchan = datashape->n_aspectchan / factors[ci];

            for(int ti = multiply_not_divide; ti < nfactors; ti++) {
              if(multiply_not_divide || datashape->n_time % factors[ti] == 0) {
                ntime = multiply_not_divide ? datashape->n_time * factors[ti] : datashape->n_time / factors[ti];

                // Read at least 2 blocks, all channels, each time.
                const size_t time_repetitions = multiply_not_divide ? 2 : 2*factors[ti];
                if(ntime*time_repetitions > guppiraw_iterate_ntime_remaining(&gr_iterate)) {
                  // Reset to beginning of file
                  printf("Resetting iterations to beginning of stem!\n");
                  gr_iterate.block_index = 0;
                  for(int fi = 0; fi < gr_iterate.n_file; fi++) {
                    gr_iterate.file_info[fi].block_index = 0;
                  }
                  gr_iterate.file_index = 0;
                }
                printf(
                  "Iteration (x%lu full bands):\tblock #%d[a=%lu,c=%lu,t=%lu] time=%lu/%lu, chan=%lu/%u, aspect=%lu/%u...",
                  time_repetitions,
                  gr_iterate.block_index,
                  gr_iterate.aspect_index, gr_iterate.chan_index, gr_iterate.time_index,
                  ntime, datashape->n_time,
                  nchan, datashape->n_aspectchan,
                  naspect, datashape->n_aspect
                );

                const long bytes_invalid = benchmark_not_validate ? 
                  benchmark_iteration(&gr_iterate, ntime, nchan, naspect, time_repetitions) :
                  validate_iteration(&gr_iterate, ntime, nchan, naspect, time_repetitions);

                if(bytes_invalid != 0){
                  printf("failed (%ld/%lu bytes invalid, %f%%).\n",
                    bytes_invalid, time_repetitions*guppiraw_iterate_bytesize(&gr_iterate, ntime, nchan, naspect),
                    100.0*(double)bytes_invalid / 
                    (double)(
                      (datashape->n_aspect/naspect)*(datashape->n_aspectchan/nchan)*time_repetitions*
                        guppiraw_iterate_bytesize(&gr_iterate, ntime, nchan, naspect)
                    )
                  );
                  return 1;
                }
                if(!benchmark_not_validate) {
                  printf("passed\n");
                }
              }
            }

          }
        }

      }
    }
  }

  return 0;
}