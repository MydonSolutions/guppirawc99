#include "guppiraw.h"
#include <time.h>

int main(int argc, char const *argv[])
{
	if(argc < 2) {
		printf("Usage: %s output_stempath.\n",argv[0]);
		return 1;
	}
	else if(strlen(argv[1]) > 245) {
		printf("output_stempath length is larger than 245.\n");
		return 1;
	}

	unsigned int seed = time(NULL);
	srand(seed);

	const int64_t n_bits = 4;
	const int64_t n_pols = 512; // ensure each time index is 512 bytes, directio aligned
	const int64_t n_time = 3*5*7;
	const int64_t n_chan_perant = 3*5*7;
	const int64_t n_ant = 2*3;

	const int64_t block_bytesize = (n_ant*n_chan_perant*n_time*n_pols*2*n_bits)/8;
	int block_idx = 0;
	const int blocks = 32;

	guppiraw_header_t header = {0};
	guppiraw_header_put_integer(&header, "NBITS", n_bits);
	guppiraw_header_put_integer(&header, "NPOL", n_pols);
	guppiraw_header_put_integer(&header, "OBSNCHAN", n_ant*n_chan_perant);
	guppiraw_header_put_integer(&header, "NANTS", n_ant);
	guppiraw_header_put_integer(&header, "BLOCSIZE", block_bytesize);
	guppiraw_header_put_string(&header, "OBSID", "Synth Observation");
	guppiraw_header_put_double(&header, "CHAN_BW", 3.14159265);
	guppiraw_header_put_integer(&header, "DIRECTIO", 1);

	void* data __attribute__ ((aligned (512))) = memalign(512, block_bytesize);

	char output_filepath[256];
	sprintf(output_filepath, "%s.0000.raw", argv[1]);

	int fd = open(output_filepath, O_WRONLY|O_CREAT|O_DIRECT, 0644);
	if(fd < 1) {
		fprintf(stderr, "Could not write to '%s': %d\n\t", output_filepath, fd);
		perror("");
		return 1;
	}
	clock_t start;
	clock_t clocks_writing = 0;

	for(; block_idx < blocks; block_idx++) {
		for(int i = 0; i < block_bytesize/sizeof(int); i++)
			((int*)data)[i] = rand();
		
		guppiraw_header_put_integer(&header, "BLKIDX", block_idx);
		start = clock();
		guppiraw_write_block(fd, &header, data, block_bytesize, 1);
		clocks_writing += clock() - start;
	}
	close(fd);

	printf(
		"Wrote %d blocks of %ld in %f seconds: %f GB/s\n",
		blocks, block_bytesize,
		(double)clocks_writing/CLOCKS_PER_SEC,
		((double)blocks*block_bytesize)/((double)clocks_writing*1e9/CLOCKS_PER_SEC)
	);

	guppiraw_iterate_info_t gr_iterate = {0};
	int rv = guppiraw_iterate_open_stem(argv[1], &gr_iterate);
  if(rv) {
		fprintf(stderr, "Error opening: %s.%04d.raw: %d\n", gr_iterate.stempath, gr_iterate.fileenum, rv);
		return 1;
	}
	guppiraw_metadata_t* metadata = &gr_iterate.file_info.block_info.metadata;
	printf("\nRead datashape:\n");
	printf("\tblock_bytesize: %lu\n", metadata->datashape.block_size);
	printf("\tdirectio: %d\n", metadata->directio);
	printf("\tn_obschan: %u\n", metadata->datashape.n_obschan);
	printf("\tn_pol: %u\n", metadata->datashape.n_pol);
	printf("\tn_bit: %u\n", metadata->datashape.n_bit);
	printf("\tn_time: %lu\n", metadata->datashape.n_time);

	printf("Number of blocks in file: %d\n", gr_iterate.file_info.n_blocks);
	if(gr_iterate.file_info.n_blocks != blocks) {
		return 1;
	}

	for(int i = 0; i < gr_iterate.file_info.n_blocks; i++) {
		printf(
			"\tblock#%d: header @ %ld, data @ %ld\n",
			i,
			gr_iterate.file_info.file_header_pos[i],
			gr_iterate.file_info.file_data_pos[i]
		);
	}

	srand(seed);
	while(gr_iterate.fd > 0 && gr_iterate.block_index < gr_iterate.file_info.n_blocks) {
		printf(
			"block #%d[c=%lu,t=%lu] time=%lu, chan=%u...",
			gr_iterate.block_index,
			gr_iterate.chan_index, gr_iterate.time_index,
			metadata->datashape.n_time,
			metadata->datashape.n_obschan
		);

		int rv = guppiraw_iterate_read_block(&gr_iterate, data);
		if(rv != metadata->datashape.block_size) {
			fprintf(stderr,
				"Did not correctly read block #%d: %d (fd: %d)\n\t",
				gr_iterate.block_index-1,
				rv,
				gr_iterate.fd
			);
			perror("");
			break;
		}
		
		size_t bytes_wrong = 0;
		for(int i = 0; i < block_bytesize/sizeof(int); i++)
			bytes_wrong += ((int*)data)[i] != rand();
		
		if(bytes_wrong == 0){
			block_idx--;
			printf( "correct!\n");
		}
		else {
			fprintf(stderr, "wrong!\n");
		}
	}

	free(data);
	printf("Blocks incorrect: %d.\n", block_idx);
  return block_idx;
}