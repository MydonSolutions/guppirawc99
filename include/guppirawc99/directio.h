#ifndef GUPPI_RAW_C99_DIRECTIO_H_
#define GUPPI_RAW_C99_DIRECTIO_H_

static inline off_t guppiraw_directio_align(off_t value) {
  return (value + 511) & ~((off_t)511);
}

#endif // GUPPI_RAW_C99_DIRECTIO_H_