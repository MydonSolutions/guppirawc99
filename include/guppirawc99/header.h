#ifndef GUPPI_RAW_C99_HEADER_H_
#define GUPPI_RAW_C99_HEADER_H_

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fitsheaderc99.h"
#include "guppirawc99/structs.h"
#include "guppirawc99/header_key.h"
#include "guppirawc99/directio.h"
#include "guppirawc99/calculations.h"


void guppiraw_header_parse_entry(const char* entry, guppiraw_metadata_t* metadata);
char guppiraw_header_entry_is_END(const uint64_t* entry_uint64);
// Negative `header_length` indicates no hard limit on `header_string` length
void guppiraw_header_string_parse_metadata(guppiraw_metadata_t* metadata, char* header_string, int64_t header_string_length);

typedef struct guppiraw_header_llnode {
  char keyvalue[81];
  struct guppiraw_header_llnode* next;
} guppiraw_header_llnode_t;

typedef struct {
  guppiraw_header_llnode_t* head;
  guppiraw_metadata_t metadata;
  uint32_t n_entries;
} guppiraw_header_t;

int guppiraw_header_put_string(guppiraw_header_t* header, const char* key, const char* value); 
int guppiraw_header_put_double(guppiraw_header_t* header, const char* key, const double value); 
int guppiraw_header_put_integer(guppiraw_header_t* header, const char* key, const int64_t value); 
int guppiraw_header_put_metadata(guppiraw_header_t* header);

// Negative `header_length` indicates no hard limit on `header_string` length
void guppiraw_header_parse(guppiraw_header_t* header, char* header_string, int64_t header_string_length);
void guppiraw_header_free(guppiraw_header_t* header);

char* guppiraw_header_malloc_string(const guppiraw_header_t* header);

#endif // GUPPI_RAW_C99_HEADER_H_