#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "guppiraw.h"

int main(int argc, char const *argv[])
{
  if (argc > 1){
    guppiraw_header_t raw_header = {0};
    int raw_fd = open(argv[1], O_RDONLY);
    guppiraw_read_header(raw_fd, &raw_header);
    printf("block_size: %lu\n", raw_header.block_size);
    printf("directio: %d\n", raw_header.directio);
    printf("n_obschan: %u\n", raw_header.n_obschan);
    printf("n_pol: %u\n", raw_header.n_pol);
    printf("n_bit: %u\n", raw_header.n_bit);
    printf("n_ant: %u\n", raw_header.n_ant);
  }
  
  return 0;
}