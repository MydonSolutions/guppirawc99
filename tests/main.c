#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "guppiraw.h"

int main(int argc, char const *argv[])
{
  if (argc > 1){
    guppiraw_header_t raw_header = {0};
    int raw_fd = open(argv[1], O_RDONLY);
    int block_id = 0;
    while(guppiraw_read_header(raw_fd, &raw_header) == 0) {
      printf("\nblock #%d\n", block_id);
      printf("\tblock_size: %lu\n", raw_header.block_size);
      printf("\tdirectio: %d\n", raw_header.directio);
      printf("\tn_obschan: %u\n", raw_header.n_obschan);
      printf("\tn_pol: %u\n", raw_header.n_pol);
      printf("\tn_bit: %u\n", raw_header.n_bit);
      printf("\tn_ant: %u\n", raw_header.n_ant);
      block_id ++;
      guppiraw_seek_next(raw_fd, &raw_header);
    }
  }
  
  return 0;
}