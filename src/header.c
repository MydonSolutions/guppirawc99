#include "guppirawc99/header.h"

static char _GUPPI_RAW_FTISHEADER_VALUEBUF_STR[] =
"                                                                                "
GUPPI_RAW_HEADER_END_STR;  

int _guppiraw_header_put_entry(guppiraw_header_llnode_t* head, const char* keyvalue) {
  while(1) {
    if(strncmp(head->keyvalue, keyvalue, 8) == 0) {
      memcpy(head->keyvalue, keyvalue, 80);
      return 0;
    }

    if(head->next == NULL) {
      break;
    }
    head = head->next;
  }
  head->next = malloc(sizeof(guppiraw_header_llnode_t));
  memcpy(head->next->keyvalue, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR, 80);
  head->next->next = NULL;
  return 1;
}

int _guppiraw_header_put_string(guppiraw_header_llnode_t* head, const char* key, const char* value) {
  memset(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, ' ', 80);
  hputs(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, key, value);
  return _guppiraw_header_put_entry(head, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR);
}

int _guppiraw_header_put_double(guppiraw_header_llnode_t* head, const char* key, const double value) {
  memset(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, ' ', 80);
  hputr8(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, key, value);
  return _guppiraw_header_put_entry(head, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR);
}

int _guppiraw_header_put_integer(guppiraw_header_llnode_t* head, const char* key, const int64_t value) {
  memset(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, ' ', 80);
  hputi8(_GUPPI_RAW_FTISHEADER_VALUEBUF_STR, key, value);
  return _guppiraw_header_put_entry(head, _GUPPI_RAW_FTISHEADER_VALUEBUF_STR);
}

static inline void _guppiraw_header_ensure_initialised(guppiraw_header_t* header, const char* key) {
  if(header->head == NULL || header->n_entries == 0) {
    header->head = malloc(sizeof(guppiraw_header_llnode_t));
    memset(header->head->keyvalue, ' ', 80);
    memcpy(header->head->keyvalue, key, strlen(key));
    header->head->next = NULL;
    header->n_entries = 1;
  }
}

int guppiraw_header_put_string(guppiraw_header_t* header, const char* key, const char* value) {
  _guppiraw_header_ensure_initialised(header, key);
  header->n_entries += _guppiraw_header_put_string(header->head, key, value);
  return 0;
}
int guppiraw_header_put_double(guppiraw_header_t* header, const char* key, const double value) {
  _guppiraw_header_ensure_initialised(header, key);
  header->n_entries += _guppiraw_header_put_double(header->head, key, value);
  return 0;
}
int guppiraw_header_put_integer(guppiraw_header_t* header, const char* key, const int64_t value) {
  _guppiraw_header_ensure_initialised(header, key);
  header->n_entries += _guppiraw_header_put_integer(header->head, key, value);
  return 0;
}

void _guppiraw_header_free(guppiraw_header_llnode_t* head) {
  if(head->next != NULL) {
    _guppiraw_header_free(head->next);
  }
  free(head->next);
}

void guppiraw_header_free(guppiraw_header_t* header) {
  _guppiraw_header_free(header->head);
  free(header->head);
  header->n_entries = 0;
}

const char _guppiraw_directio_padding_buffer[513] = 
"********************************************************************************************************************************"
"********************************************************************************************************************************"
"********************************************************************************************************************************"
"********************************************************************************************************************************";

char* guppiraw_header_malloc_string(const guppiraw_header_t* header, const char directio) {
  const int n_entries = header->n_entries;
  const size_t header_entries_len = (n_entries + 1) * 80;
  guppiraw_header_llnode_t* header_entry = header->head;
  char* header_string;
  if(directio) {
    const size_t header_entries_len_aligned = guppiraw_directio_align(header_entries_len);
    header_string = memalign(512, header_entries_len_aligned);
    memcpy(
      header_string + header_entries_len,
      _guppiraw_directio_padding_buffer,
      header_entries_len_aligned - header_entries_len
    );
  }
  else {
    header_string = malloc(header_entries_len);
  }

  for(int i = 0; i < n_entries; i++) {
    memcpy(header_string + i*80, header_entry->keyvalue, 80);
    header_entry = header_entry->next;
  }
  memcpy(header_string + n_entries*80, GUPPI_RAW_HEADER_END_STR, 80);
  return header_string;
}
