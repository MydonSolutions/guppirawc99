#ifndef GUPPI_RAW_C99_DIRECTIO_H_
#define GUPPI_RAW_C99_DIRECTIO_H_

#ifndef _GNU_SOURCE
#pragma message("_GNU_SOURCE not defined so DIRECTIO is disabled!") 
#define O_DIRECT 0
#endif

#endif // GUPPI_RAW_C99_DIRECTIO_H_