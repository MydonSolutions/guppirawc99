#include "guppirawc99/calculations.h"

inline off_t guppiraw_calc_directio_aligned(off_t value) {
  return (value + 511) & ~((off_t)511);
}

inline double guppiraw_calc_unix_date(
	const double tbin,
	const size_t sampleperblk,
	const size_t piperblk,
	const size_t synctime,
	const size_t pktidx
) {
	 return (double)synctime + ((sampleperblk * tbin) / piperblk) * pktidx;
}