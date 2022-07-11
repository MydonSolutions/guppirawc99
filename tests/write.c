#include <stdlib.h>
#include "guppiraw.h"

int main(int argc, char const *argv[])
{
	const int block_size = 1000;
	int block_idx = 0;
	const int blocks = 5;

	guppiraw_header_t header = {0};
	guppiraw_header_put_string(&header, "OBSID", "Synth Observation");
	guppiraw_header_put_integer(&header, "BLOCSIZE", block_size);
	guppiraw_header_put_integer(&header, "DIRECTIO", 1);

	void* data = malloc(block_size);

	int fd = open("./synth.0000.raw", O_WRONLY|O_CREAT, 0644);
	if(fd < 1) {
		fprintf(stderr, "Could not write to './synth.0000.raw': %d\n", fd);
		return 1;
	}

	for(; block_idx < blocks; block_idx++) {
		memset(data, block_idx+48, block_size);
		guppiraw_header_put_integer(&header, "BLKIDX", block_idx);
		guppiraw_write_block(fd, &header, data, block_size, 1);
	}
	close(fd);

	guppiraw_iterate_info_t gr_iterate = {0};
	int rv = guppiraw_iterate_open_stem("./synth", &gr_iterate);
  if(rv) {
		fprintf(stderr, "Error opening: %s.%04d.raw: %d\n", gr_iterate.stempath, gr_iterate.fileenum, rv);
		return 1;
	}
	guppiraw_datashape_t* datashape = &gr_iterate.file_info.block_info.datashape;
	printf("\nRead datashape:\n");
	printf("\tblock_size: %lu\n", datashape->block_size);
	printf("\tdirectio: %d\n", gr_iterate.file_info.block_info.directio);
	printf("\tn_obschan: %u\n", datashape->n_obschan);
	printf("\tn_pol: %u\n", datashape->n_pol);
	printf("\tn_bit: %u\n", datashape->n_bit);
	printf("\tn_time: %lu\n", datashape->n_time);

	printf("Number of blocks in file: %d\n", gr_iterate.file_info.n_blocks);
	for(int i = 0; i < gr_iterate.file_info.n_blocks; i++) {
		printf(
			"\tblock#%d: header @ %ld, data @ %ld\n",
			i,
			gr_iterate.file_info.file_header_pos[i],
			gr_iterate.file_info.file_data_pos[i]
		);
	}
	

	while(gr_iterate.fd > 0) {
		const int expected_data = gr_iterate.block_index;
		fprintf(stderr,
			"block #%d[c=%lu,t=%lu] time=%lu, chan=%u...",
			gr_iterate.block_index,
			gr_iterate.chan_index, gr_iterate.time_index,
			datashape->n_time,
			datashape->n_obschan
		);

		guppiraw_iterate_read_block(&gr_iterate, data);
		if(((char*)data)[0] == expected_data+48) {
			block_idx--;
			fprintf(stderr, "correct!\n");
		}
		else {
			fprintf(stderr, "Data[0] is %c instead of %c!\n", ((char*)data)[0], expected_data+48);
		}
	}

	free(data);
	printf("Blocks incorrect: %d.\n", block_idx);
  return block_idx;
}