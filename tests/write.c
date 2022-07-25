#include "guppiraw.h"
#include <time.h>

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

typedef struct {
	char obsid[72];
	double chan_bw;
} guppiraw_block_meta_t;

void guppiraw_write_block_meta(guppiraw_header_t* header) {
	guppiraw_block_meta_t* user_metadata = (guppiraw_block_meta_t*) header->metadata.user_data;
	guppiraw_header_put_string(header, "OBSID", user_metadata->obsid);
	guppiraw_header_put_double(header, "CHAN_BW", user_metadata->chan_bw);
}

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

	const int64_t n_bits = 4;
	const int64_t n_pols = 512; // ensure each time index is 512 bytes, directio aligned
	const int64_t n_time = 3*5*7;
	const int64_t n_chan_perant = 3*5*7;
	const int64_t n_ant = 2*3;

	const int64_t block_bytesize = (n_ant*n_chan_perant*n_time*n_pols*2*n_bits)/8;
	int block_idx = 0;
	const int blocks = 36;
	const int blocks_per_file = 11;

	const int n_aspect_batch = 3;
	const int n_batched_aspect = n_ant/n_aspect_batch;
	const int n_chan_batch = 5;
	const int64_t batch_aspect_block_bytesize = block_bytesize / (n_aspect_batch * n_chan_batch * n_batched_aspect);

	guppiraw_header_t header = {0};
	header.metadata.datashape.n_bit = n_bits;
	header.metadata.datashape.n_pol = n_pols;
	header.metadata.datashape.n_ant = n_ant;
	header.metadata.datashape.n_time = n_time;
	header.metadata.datashape.n_aspectchan = n_chan_perant;
	header.metadata.datashape.block_size = block_bytesize;
	header.metadata.directio = 1;
	header.metadata.user_data = malloc(sizeof(guppiraw_block_meta_t));
	
	snprintf(((guppiraw_block_meta_t*)header.metadata.user_data)->obsid, 72, "Synth Observation");
	((guppiraw_block_meta_t*)header.metadata.user_data)->chan_bw = 3.14159265;
	guppiraw_header_put_metadata(&header);
	guppiraw_write_block_meta(&header);

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
		if(!do_not_validate || block_idx == 0) {
			srand(seed + block_idx);
			guppiraw_header_put_integer(&header, "BLOCSEED", seed + block_idx);
			for(int aspect_batch_i = 0; aspect_batch_i < n_aspect_batch; aspect_batch_i++)
				for(int batch_aspect_i = 0; batch_aspect_i < n_batched_aspect; batch_aspect_i++)
					for(int chan_batch_i = 0; chan_batch_i < n_chan_batch; chan_batch_i++)
						for(int i = 0; i < batch_aspect_block_bytesize/sizeof(int); i++)
							((int*)(
								data + ((aspect_batch_i * n_chan_batch + chan_batch_i) * n_batched_aspect + batch_aspect_i) * batch_aspect_block_bytesize
							))[i] = rand();
		}
		
		guppiraw_header_put_integer(&header, "BLOCIDX", block_idx);
		
		clock_gettime(CLOCK_MONOTONIC, &start);
			guppiraw_write_block_batched(fd, &header, data, n_aspect_batch, n_chan_batch);
		clock_gettime(CLOCK_MONOTONIC, &stop);
		writing_ns += ELAPSED_NS(start, stop);
	}
	printf("Closing '%s' at block #%02d.\n", output_filepath, block_idx);
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
	int rv = guppiraw_iterate_open(&gr_iterate, argv[argc-1]);
  if(rv) {
		fprintf(stderr, "Error opening: %s.%04d.raw: %d, fileblock %d\n", gr_iterate.stempath, gr_iterate.n_file, rv, guppiraw_iterate_file_info(&gr_iterate, gr_iterate.n_file)->n_block);
		return 1;
	}

	guppiraw_metadata_t* metadata = guppiraw_iterate_metadata(&gr_iterate);
	printf("\nRead datashape:\n");
	printf("\tblock_bytesize: %lu\n", metadata->datashape.block_size);
	printf("\tdirectio: %d\n", metadata->directio);
	printf("\tn_obschan: %u\n", metadata->datashape.n_obschan);
	printf("\tn_pol: %u\n", metadata->datashape.n_pol);
	printf("\tn_bit: %u\n", metadata->datashape.n_bit);
	printf("\tn_time: %lu\n", metadata->datashape.n_time);

	printf("Number of blocks in stem: %d (over %d files)\n", gr_iterate.n_block, gr_iterate.n_file);

	for(int i = 0; i < gr_iterate.n_block; i++) {
		int block_index = i;
		int file_index = guppiraw_iterate_file_index_of_block(&gr_iterate, &block_index);
		printf(
			"\tblock#%d: file #%d(block #%d, header @ %ld, data @ %ld)\n",
			i, file_index, block_index,
			gr_iterate.file_info[file_index].file_header_pos[block_index],
			gr_iterate.file_info[file_index].file_data_pos[block_index]
		);
	}
	if(gr_iterate.n_block != blocks) {
		return 1;
	}

	while(gr_iterate.block_index < gr_iterate.n_block) {
		printf(
			"block #%d[f=%d,a=%lu,c=%lu,t=%lu] time=%lu, chan=%u, aspect=%u...",
			gr_iterate.block_index,
			gr_iterate.file_index,gr_iterate.aspect_index,gr_iterate.chan_index, gr_iterate.time_index,
			metadata->datashape.n_time,
			metadata->datashape.n_aspectchan,
			metadata->datashape.n_aspect
		);

		srand(seed + gr_iterate.block_index);
		int rv = guppiraw_iterate_read_block(&gr_iterate, data);
		if(rv != metadata->datashape.block_size) {
			fprintf(stderr,
				"Did not correctly read block #%d: %d\n\t",
				gr_iterate.block_index-1,
				rv
			);
			perror("");
			break;
		}
		
		size_t bytes_wrong = 0;
		for(int i = 0; i < block_bytesize/sizeof(int); i++)
			bytes_wrong += ((int*)data)[i] != rand();
		
		if(bytes_wrong == 0){
			block_idx--;
			printf("correct!\n");
		}
		else {
			printf("wrong!\n");
		}
	}

	guppiraw_iterate_close(&gr_iterate);
	free(data);

	printf("Blocks incorrect: %d.\n", block_idx);
	printf(
		"%s output: `%s`\n",
		block_idx == 0 ? "Valid" : "Invalid",
		output_filepath
	);
  return block_idx != 0;
}