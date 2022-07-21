#include <stdlib.h>
#include "guppirawc99/header.h"

typedef struct {
	char obsid[72];
	double chan_bw;
} guppiraw_block_meta_t;

const uint64_t KEY_UINT64_OBSID    = GUPPI_RAW_KEY_UINT64_ID_LE('O','B','S','I','D',' ',' ',' ');
const uint64_t KEY_UINT64_CHAN_BW  = GUPPI_RAW_KEY_UINT64_ID_LE('C','H','A','N','_','B','W',' ');

void guppiraw_parse_block_meta(const char* entry, void* block_meta) {
  if(((uint64_t*)entry)[0] == KEY_UINT64_OBSID)
    hgets(entry, "OBSID", 72, ((guppiraw_block_meta_t*)block_meta)->obsid);
	else if(((uint64_t*)entry)[0] == KEY_UINT64_CHAN_BW)
    hgetr8(entry, "CHAN_BW", &((guppiraw_block_meta_t*)block_meta)->chan_bw);
}

int main(int argc, char const *argv[])
{
	guppiraw_header_t header = {0};
	guppiraw_header_put_string(&header, "OBSID", "False Observation");
	guppiraw_header_put_string(&header, "OBSID", "Faux Observation");
	guppiraw_header_put_integer(&header, "OBSNCHAN", -1024);
	guppiraw_header_put_integer(&header, "OBSNCHAN", 1024);
	guppiraw_header_put_double(&header, "CHAN_BW", 0.1024);
	guppiraw_header_put_double(&header, "CHAN_BW", 0.5);
  
	char* header_string = guppiraw_header_malloc_string(&header, 0);
	printf("%s\n", header_string);

	int rv = 0;

	guppiraw_metadata_t metadata = {0};
	metadata.user_data = malloc(sizeof(guppiraw_block_meta_t));
	metadata.user_callback = guppiraw_parse_block_meta;
	guppiraw_header_parse_string(&metadata, header_string, -1);
	const guppiraw_block_meta_t* user_metadata = (guppiraw_block_meta_t*)metadata.user_data;

	if(strncmp("Faux Observation", user_metadata->obsid, 16) != 0) {
		fprintf(stderr, "OBSID != 'Faux Observation': '%s'\n", user_metadata->obsid);
		rv = 1;
	}
	else if(metadata.datashape.n_obschan != 1024) {
		fprintf(stderr, "OBSNCHAN != 1024: %d\n", metadata.datashape.n_obschan);
		rv = 1;
	}
	else if(user_metadata->chan_bw != 0.5) {
		fprintf(stderr, "CHAN_BW != 0.5: %f\n", user_metadata->chan_bw);
		rv = 1;
	}

	free(header_string);
	guppiraw_header_free(&header);

  return rv;
}