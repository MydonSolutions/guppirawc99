#include "guppiraw.h"
#include <time.h>

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

int main(int argc, char const *argv[])
{
	char do_not_validate = 0;
	if(argc != 2) {
		if(!(argc == 3 && strncmp("-V", argv[1], 2) == 0)) {
			printf(
				"Usage: %s [options] output_stempath.\n"\
				"options:\n"\
				"\t-V\t\tDo not validate output.\n",
				argv[0]
			);
			return 1;
		}
		do_not_validate = 1;
	}
	else if(strlen(argv[argc-1]) > 245) {
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
	const int blocks = 36;
	const int blocks_per_file = 36;

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

	int file_enum = 0;
	char output_filepath[256];
	sprintf(output_filepath, "%s.%04d.raw", argv[argc-1], file_enum++);
	printf("Opening '%s'\n", output_filepath);

	int fd = open(output_filepath, O_WRONLY|O_CREAT|O_DIRECT, 0644);
	if(fd < 1) {
		fprintf(stderr, "Could not write to '%s': %d\n\t", output_filepath, fd);
		perror("");
		return 1;
	}
	
  struct timespec start, stop;
	uint64_t writing_ns = 0;

	for(; block_idx < blocks; block_idx++) {
		if(block_idx > 0 && block_idx % blocks_per_file == 0) {
			printf("Closing '%s' at block #%02d...", output_filepath, block_idx);
			close(fd);
			sprintf(output_filepath, "%s.%04d.raw", argv[argc-1], file_enum++);
			printf("Opening '%s'.\n", output_filepath);
			fd = open(output_filepath, O_WRONLY|O_CREAT|O_DIRECT, 0644);
			if(fd < 1) {
				fprintf(stderr, "Could not write to '%s': %d\n\t", output_filepath, fd);
				perror("");
				return 1;
			}
		}
		if(!do_not_validate || block_idx == 0)
			for(int i = 0; i < block_bytesize/sizeof(int); i++)
				((int*)data)[i] = rand();
		
		guppiraw_header_put_integer(&header, "BLKIDX", block_idx);
		
		clock_gettime(CLOCK_MONOTONIC, &start);
			guppiraw_write_block(fd, &header, data, block_bytesize, 1);
		clock_gettime(CLOCK_MONOTONIC, &stop);
		writing_ns += ELAPSED_NS(start, stop);
	}
	printf("Closing '%s'\n", output_filepath);
	close(fd);

	printf(
		"Wrote %d blocks of %ld in %f seconds: %f GB/s.\n",
		blocks, block_bytesize,
		(double)writing_ns/1e9,
		((double)blocks*block_bytesize)/(double)writing_ns
	);

	if(do_not_validate == 1) {
		printf("Not validating output: `%s`\n", output_filepath);
		return 0;
	}

	guppiraw_iterate_info_t gr_iterate = {0};
	int rv = guppiraw_iterate_open_stem(argv[argc-1], &gr_iterate);
  if(rv) {
		fprintf(stderr, "Error opening: %s.%04d.raw: %d\n", gr_iterate.stempath, gr_iterate.fileenum, rv);
		return 1;
	}
	guppiraw_metadata_t* metadata = &gr_iterate.file_info.metadata;
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
	printf(
		"%s output: `%s`\n",
		block_idx == 0 ? "Valid" : "Invalid",
		output_filepath
	);
  return block_idx == 0;
}