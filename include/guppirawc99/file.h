#ifndef GUPPI_RAW_C99_FILE_H_
#define GUPPI_RAW_C99_FILE_H_

typedef struct {
  int fd;
  guppiraw_metadata_t metadata;
  off_t bytesize_file;

  int n_block;
  int block_index;

  // Block-position fields
  off_t *file_header_pos;
  off_t *file_data_pos;
} guppiraw_file_info_t;

#define guppiraw_file_ntime_remaining(gr_file)\
  ((gr_file->n_block - gr_file->block_index) * gr_file->metadata.datashape.n_time)

#define guppiraw_file_data_pos(gr_file, block_index) gr_file->file_data_pos[block_index]
#define guppiraw_file_data_pos_offset(gr_file, block_offset) guppiraw_file_data_pos(gr_file, gr_file->block_index + block_offset)
#define guppiraw_file_data_pos_current(gr_file) guppiraw_file_data_pos_offset(gr_file, 0)

#endif // GUPPI_RAW_C99_FILE_H_