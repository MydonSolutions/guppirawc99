#include "guppiraw.h"

int main(int argc, char const *argv[])
{
  if (argc > 1){
    guppiraw_block_info_t gr_blockinfo = {0};
    int raw_fd = open(argv[1], O_RDONLY);
    int block_id = 0;
    while(guppiraw_read_blockheader(raw_fd, &gr_blockinfo) == 0) {
      printf("\nblock #%d\n", block_id);
      printf("\tblock_size: %lu\n", gr_blockinfo.block_size);
      printf("\tdirectio: %d\n", gr_blockinfo.directio);
      printf("\tn_obschan: %u\n", gr_blockinfo.n_obschan);
      printf("\tn_pol: %u\n", gr_blockinfo.n_pol);
      printf("\tn_bit: %u\n", gr_blockinfo.n_bit);
      printf("\tn_time: %lu\n", gr_blockinfo.n_time);
      block_id ++;
      guppiraw_seek_next_block(raw_fd, &gr_blockinfo);
    }
    close(raw_fd);
  }
  
  return 0;
}