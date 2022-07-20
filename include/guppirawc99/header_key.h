#ifndef GUPPI_RAW_C99_HEADER_KEY_H_
#define GUPPI_RAW_C99_HEADER_KEY_H_

#define GUPPI_RAW_HEADER_MAX_ENTRIES 2048
#define GUPPI_RAW_HEADER_DIGEST_BYTES 5*4096 // LCM(80, 4096)
#define GUPPI_RAW_HEADER_DIGEST_ENTRIES (GUPPI_RAW_HEADER_DIGEST_BYTES/80)
#define GUPPI_RAW_HEADER_END_STR \
"END                                                                             "

#define GUPPI_RAW_KEY_UINT64_ID_BE(c0,c1,c2,c3,c4,c5,c6,c7) \
  (\
  (((uint64_t)c0)<<56) + \
  (((uint64_t)c1)<<48) + \
  (((uint64_t)c2)<<40) + \
  (((uint64_t)c3)<<32) + \
  (((uint64_t)c4)<<24) + \
  (((uint64_t)c5)<<16) + \
  (((uint64_t)c6)<<8) + \
  ((uint64_t)c7)\
  )

#define GUPPI_RAW_KEY_UINT64_ID_LE(c0,c1,c2,c3,c4,c5,c6,c7) \
  (uint64_t)(\
  ((uint64_t)c0) + \
  (((uint64_t)c1)<<8) + \
  (((uint64_t)c2)<<16) + \
  (((uint64_t)c3)<<24) + \
  (((uint64_t)c4)<<32) + \
  (((uint64_t)c5)<<40) + \
  (((uint64_t)c6)<<48) + \
  (((uint64_t)c7)<<56)\
  )

#define GUPPI_RAW_KEYSTR_UINT64_ID_BE(key) \
  GUPPI_RAW_KEY_UINT64_ID_BE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7])

#define GUPPI_RAW_KEYSTR_UINT64_ID_LE(key) \
  GUPPI_RAW_KEY_UINT64_ID_LE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7])

#endif // GUPPI_RAW_C99_HEADER_KEY_H_