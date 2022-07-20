#ifndef GUPPI_RAW_C99_STRUCTS_H_
#define GUPPI_RAW_C99_STRUCTS_H_

typedef struct {
  // Header populated fields
  uint32_t n_obschan;
  uint32_t n_ant;
  uint32_t n_beam;
  uint32_t n_pol;
  uint32_t n_bit;
  uint64_t block_size;

  // Header inferred fields
  size_t n_time;
  uint32_t n_aspect;
  uint32_t n_aspectchan;

  size_t bytestride_polarization;
  size_t bytestride_time;
  size_t bytestride_channel;
  size_t bytestride_aspect;
} guppiraw_datashape_t;

typedef void (*guppiraw_header_entry_parser)(const char* entry, void* user_data);

typedef struct {
  // Header populated fields
  int directio;

  guppiraw_datashape_t datashape;

  // User data
  guppiraw_header_entry_parser user_callback;
  void* user_data;
} guppiraw_metadata_t;

typedef struct {
  guppiraw_metadata_t metadata;

  // File position fields
  off_t file_header_pos;
  off_t file_data_pos;
} guppiraw_block_info_t;

#endif // GUPPI_RAW_C99_STRUCTS_H_