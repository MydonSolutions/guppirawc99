#ifndef GUPPI_RAW_C99_HEADER_H_
#define GUPPI_RAW_C99_HEADER_H_

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fitsheader.h"
#include "guppirawc99/header_key.h"
#include "guppirawc99/directio.h"

typedef struct guppiraw_header_llnode {
  char keyvalue[81];
  struct guppiraw_header_llnode* next;
} guppiraw_header_llnode_t;

typedef struct {
  guppiraw_header_llnode_t* head;
  uint32_t n_entries;
} guppiraw_header_t;

int guppiraw_header_put_string(guppiraw_header_t* header, const char* key, const char* value); 
int guppiraw_header_put_double(guppiraw_header_t* header, const char* key, const double value); 
int guppiraw_header_put_integer(guppiraw_header_t* header, const char* key, const int64_t value); 

void guppiraw_header_free(guppiraw_header_t* header);

char* guppiraw_header_malloc_string(const guppiraw_header_t* header, const char directio);

#endif // GUPPI_RAW_C99_HEADER_H_