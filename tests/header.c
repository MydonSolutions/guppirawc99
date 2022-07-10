#include <stdlib.h>
#include "guppiraw.h"

int main(int argc, char const *argv[])
{
	guppiraw_header_t header = {0};
	guppiraw_header_put_string(&header, "OBSID", "False Observation");
	guppiraw_header_put_string(&header, "OBSID", "Faux Observation");
	guppiraw_header_put_integer(&header, "OBSNCHAN", -1024);
	guppiraw_header_put_integer(&header, "OBSNCHAN", 1024);
	guppiraw_header_put_double(&header, "CHAN_BW", 0.1024);
	guppiraw_header_put_double(&header, "CHAN_BW", 0.5);
  
	char* header_string = guppiraw_header_malloc_string(&header);
	printf("%s\n", header_string);

	int rv = 0;

	char obsid[72] = {0};
	int obsnchan = -1;
	double chan_bw = -1.0;

	hgets(header_string, "OBSID", 72, obsid);
	hgeti4(header_string, "OBSNCHAN", &obsnchan);
	hgetr8(header_string, "CHAN_BW", &chan_bw);

	if(strncmp("Faux Observation", obsid, 16) != 0) {
		fprintf(stderr, "OBSID != 'Faux Observation': '%s'\n", obsid);
		rv = 1;
	}
	else if(obsnchan != 1024) {
		fprintf(stderr, "OBSNCHAN != 1024: %d\n", obsnchan);
		rv = 1;
	}
	else if(chan_bw != 0.5) {
		fprintf(stderr, "CHAN_BW != 0.5: %f\n", chan_bw);
		rv = 1;
	}

	free(header_string);
	guppiraw_header_free(&header);

  return rv;
}