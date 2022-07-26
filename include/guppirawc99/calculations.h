#ifndef GUPPI_RAW_C99_CALCULATIONS_H_
#define GUPPI_RAW_C99_CALCULATIONS_H_

#include <stdlib.h>

off_t guppiraw_calc_directio_aligned(off_t value);

double guppiraw_calc_unix_date(
	const double tbin,
	const size_t sampleperblk,
	const size_t piperblk,
	const size_t synctime,
	const size_t pktidx
);

#endif // GUPPI_RAW_C99_CALCULATIONS_H_