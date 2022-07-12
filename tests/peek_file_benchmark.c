#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "guppiraw.h"

int main(int argc, char const *argv[])
{
  if (argc > 1){
    guppiraw_file_info_t gr_fileinfo = {0};
    guppiraw_metadata_t* metadata = &gr_fileinfo.block_info.metadata;

    int raw_fd = open(argv[1], O_RDONLY);
    clock_t start = clock();
    guppiraw_skim_file(raw_fd, &gr_fileinfo);
    double elapsed_s = (double)(clock() - start)/ CLOCKS_PER_SEC;

    printf("file_size: %lu (%f GB/s)\n", gr_fileinfo.bytesize_file, gr_fileinfo.bytesize_file/(elapsed_s * 1e9));
    printf("number of blocks: %d\n", gr_fileinfo.n_blocks);
    printf("\tblock_size: %lu\n", metadata->datashape.block_size);
    printf("\tdirectio: %d\n", metadata->directio);
    printf("\tn_obschan: %u\n", metadata->datashape.n_obschan);
    printf("\tn_pol: %u\n", metadata->datashape.n_pol);
    printf("\tn_bit: %u\n", metadata->datashape.n_bit);
    printf("\tn_time: %lu\n", metadata->datashape.n_time);
    
    lseek(raw_fd, gr_fileinfo.file_header_pos[gr_fileinfo.n_blocks-1], SEEK_SET);
    guppiraw_read_blockheader(raw_fd, &gr_fileinfo.block_info);
    printf("Last block info:\n");
    printf("\tblock_size: %lu\n", metadata->datashape.block_size);
    printf("\tdirectio: %d\n", metadata->directio);
    printf("\tn_obschan: %u\n", metadata->datashape.n_obschan);
    printf("\tn_pol: %u\n", metadata->datashape.n_pol);
    printf("\tn_bit: %u\n", metadata->datashape.n_bit);
    printf("\tn_time: %lu\n", metadata->datashape.n_time);

    guppiraw_seek_next_block(raw_fd, &gr_fileinfo.block_info);
    assert(guppiraw_read_blockheader(raw_fd, &gr_fileinfo.block_info) == -1);

    close(raw_fd);
  }
  
  return 0;
}