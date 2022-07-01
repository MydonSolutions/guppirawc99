#include <stdlib.h>
#include "guppiraw.h"

typedef struct {
  float chan_bw;
  int nants;
} guppiraw_block_meta_t;

void guppiraw_parse_block_meta(char* entry, void* block_meta_void) {
  guppiraw_block_meta_t* block_meta = (guppiraw_block_meta_t*) block_meta_void;
  switch (((uint64_t*)entry)[0]) {
    case KEY_UINT64_ID_LE('C','H','A','N','_','B','W',' '):
      hgetr4(entry, "CHAN_BW", &block_meta->chan_bw);
      break;
    case KEY_UINT64_ID_LE('N','A','N','T','S',' ',' ',' '):
      hgeti4(entry, "NANTS", &block_meta->nants);
      break;
    default:
      break;
  }
}

int main(int argc, char const *argv[])
{
  if (argc > 1){
    guppiraw_block_info_t gr_blockinfo = {0};
    gr_blockinfo.header_user_data = malloc(sizeof(guppiraw_block_meta_t));
    gr_blockinfo.header_entry_callback = guppiraw_parse_block_meta;

    int raw_fd = open(argv[1], O_RDONLY);
    int block_id = 0;
    while(guppiraw_read_blockheader(raw_fd, &gr_blockinfo) == 0) {
      printf("\nblock #%d\n", block_id);
      printf("\tblock_size: %lu\n", gr_blockinfo.datashape.block_size);
      printf("\tdirectio: %d\n", gr_blockinfo.directio);
      printf("\tn_obschan: %u\n", gr_blockinfo.datashape.n_obschan);
      printf("\tn_pol: %u\n", gr_blockinfo.datashape.n_pol);
      printf("\tn_bit: %u\n", gr_blockinfo.datashape.n_bit);
      printf("\tn_time: %lu\n", gr_blockinfo.datashape.n_time);
      printf("\tnants: %d\n", ((guppiraw_block_meta_t*)gr_blockinfo.header_user_data)->nants);
      printf("\tchan_bw: %f\n", ((guppiraw_block_meta_t*)gr_blockinfo.header_user_data)->chan_bw);
      block_id ++;
      guppiraw_seek_next_block(raw_fd, &gr_blockinfo);
    }
    close(raw_fd);
  }
  
  return 0;
}