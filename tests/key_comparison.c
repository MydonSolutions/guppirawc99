#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "guppiraw.h"

int main(){

  const char key_a[9] = "A KEY   ";
  const char key_b[9] = "B KEY   ";
  const char key_c[9] = "A KEY  2";
  const char key_d[9] = "B KEY  2";
  const uint64_t key_a_uint64 = GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_a);
  const uint64_t key_b_uint64 = GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_b);
  const uint64_t key_c_uint64 = GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_c);
  const uint64_t key_d_uint64 = GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_d);

  const uint64_t reiterations = 1000000000;
  clock_t start, stop;

  int flag = -1;

  start = clock();
  for(uint64_t i = 0; i < reiterations; i++) {
    if (strncmp(key_a, key_a, 5) == 0) {
      flag = 1;
    }
    else if (strncmp(key_a, key_b, 5) == 0) {
      flag = 2;
    }
    else if (strncmp(key_a, key_c, 8) == 0) {
      flag = 3;
    }
    else if (strncmp(key_a, key_d, 8) == 0) {
      flag = 4;
    }
    if (strncmp(key_b, key_a, 5) == 0) {
      flag = 1;
    }
    else if (strncmp(key_b, key_b, 5) == 0) {
      flag = 2;
    }
    else if (strncmp(key_b, key_c, 8) == 0) {
      flag = 3;
    }
    else if (strncmp(key_b, key_d, 8) == 0) {
      flag = 4;
    }
    if (strncmp(key_c, key_a, 8) == 0) {
      flag = 1;
    }
    else if (strncmp(key_c, key_b, 8) == 0) {
      flag = 2;
    }
    else if (strncmp(key_c, key_c, 8) == 0) {
      flag = 3;
    }
    else if (strncmp(key_c, key_d, 8) == 0) {
      flag = 4;
    }
    if (strncmp(key_d, key_a, 8) == 0) {
      flag = 1;
    }
    else if (strncmp(key_d, key_b, 8) == 0) {
      flag = 2;
    }
    else if (strncmp(key_d, key_c, 8) == 0) {
      flag = 3;
    }
    else if (strncmp(key_d, key_d, 8) == 0) {
      flag = 4;
    }
  }
  stop = clock();
  printf("strncmp(...8): %f s\n", ((double)(stop-start))/CLOCKS_PER_SEC);

  start = clock();
  for(uint64_t i = 0; i < reiterations; i++) {
    if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_a) == key_a_uint64) {
      flag = 1;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_a) == key_b_uint64) {
      flag = 2;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_a) == key_c_uint64) {
      flag = 3;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_a) == key_d_uint64) {
      flag = 4;
    }
    if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_b) == key_a_uint64) {
      flag = 1;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_b) == key_b_uint64) {
      flag = 2;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_b) == key_c_uint64) {
      flag = 3;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_b) == key_d_uint64) {
      flag = 4;
    }
    if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_c) == key_a_uint64) {
      flag = 1;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_c) == key_b_uint64) {
      flag = 2;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_c) == key_c_uint64) {
      flag = 3;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_c) == key_d_uint64) {
      flag = 4;
    }
    if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_d) == key_a_uint64) {
      flag = 1;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_d) == key_b_uint64) {
      flag = 2;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_d) == key_c_uint64) {
      flag = 3;
    }
    else if (GUPPI_RAW_KEYSTR_UINT64_ID_LE(key_d) == key_d_uint64) {
      flag = 4;
    }
  }
  stop = clock();
  printf("KEYSTR_ == const uint64_t: %f s\n", ((double)(stop-start))/CLOCKS_PER_SEC);

  start = clock();
  for(uint64_t i = 0; i < reiterations; i++) {
    if (((uint64_t*)key_a)[0] == key_a_uint64) {
      flag = 1;
    }
    else if (((uint64_t*)key_a)[0] == key_b_uint64) {
      flag = 2;
    }
    else if (((uint64_t*)key_a)[0] == key_c_uint64) {
      flag = 3;
    }
    else if (((uint64_t*)key_a)[0] == key_d_uint64) {
      flag = 4;
    }
    if (((uint64_t*)key_b)[0] == key_a_uint64) {
      flag = 1;
    }
    else if (((uint64_t*)key_b)[0] == key_b_uint64) {
      flag = 2;
    }
    else if (((uint64_t*)key_b)[0] == key_c_uint64) {
      flag = 3;
    }
    else if (((uint64_t*)key_b)[0] == key_d_uint64) {
      flag = 4;
    }
    if (((uint64_t*)key_c)[0] == key_a_uint64) {
      flag = 1;
    }
    else if (((uint64_t*)key_c)[0] == key_b_uint64) {
      flag = 2;
    }
    else if (((uint64_t*)key_c)[0] == key_c_uint64) {
      flag = 3;
    }
    else if (((uint64_t*)key_c)[0] == key_d_uint64) {
      flag = 4;
    }
    if (((uint64_t*)key_d)[0] == key_a_uint64) {
      flag = 1;
    }
    else if (((uint64_t*)key_d)[0] == key_b_uint64) {
      flag = 2;
    }
    else if (((uint64_t*)key_d)[0] == key_c_uint64) {
      flag = 3;
    }
    else if (((uint64_t*)key_d)[0] == key_d_uint64) {
      flag = 4;
    }
  }
  stop = clock();
  printf("(uint64_t*)[0] == const uint64_t: %f s\n", ((double)(stop-start))/CLOCKS_PER_SEC);

  uint64_t key_uint64;
  start = clock();
  for(uint64_t i = 0; i < reiterations; i++) {
    key_uint64 = ((uint64_t*)key_a)[0];
    if (key_uint64 == key_a_uint64) {
      flag = 1;
    }
    else if (key_uint64 == key_b_uint64) {
      flag = 2;
    }
    else if (key_uint64 == key_c_uint64) {
      flag = 3;
    }
    else if (key_uint64 == key_d_uint64) {
      flag = 4;
    }
    key_uint64 = ((uint64_t*)key_b)[0];
    if (key_uint64 == key_a_uint64) {
      flag = 1;
    }
    else if (key_uint64 == key_b_uint64) {
      flag = 2;
    }
    else if (key_uint64 == key_c_uint64) {
      flag = 3;
    }
    else if (key_uint64 == key_d_uint64) {
      flag = 4;
    }
    key_uint64 = ((uint64_t*)key_c)[0];
    if (key_uint64 == key_a_uint64) {
      flag = 1;
    }
    else if (key_uint64 == key_b_uint64) {
      flag = 2;
    }
    else if (key_uint64 == key_c_uint64) {
      flag = 3;
    }
    else if (key_uint64 == key_d_uint64) {
      flag = 4;
    }
    key_uint64 = ((uint64_t*)key_d)[0];
    if (key_uint64 == key_a_uint64) {
      flag = 1;
    }
    else if (key_uint64 == key_b_uint64) {
      flag = 2;
    }
    else if (key_uint64 == key_c_uint64) {
      flag = 3;
    }
    else if (key_uint64 == key_d_uint64) {
      flag = 4;
    }
  }
  stop = clock();
  printf("(uint64_t)key == const uint64_t: %f s\n", ((double)(stop-start))/CLOCKS_PER_SEC);

  start = clock();
  for(uint64_t i = 0; i < reiterations; i++) {
    switch (((uint64_t*)key_a)[0]) {
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ',' '):
        flag = 1;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ',' '):
        flag = 2;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ','2'):
        flag = 3;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ','2'):
        flag = 4;
        break;
    }
    switch (((uint64_t*)key_b)[0]) {
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ',' '):
        flag = 1;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ',' '):
        flag = 2;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ','2'):
        flag = 3;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ','2'):
        flag = 4;
        break;
    }
    switch (((uint64_t*)key_c)[0]) {
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ',' '):
        flag = 1;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ',' '):
        flag = 2;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ','2'):
        flag = 3;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ','2'):
        flag = 4;
        break;
    }
    switch (((uint64_t*)key_d)[0]) {
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ',' '):
        flag = 1;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ',' '):
        flag = 2;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('A',' ','K','E','Y',' ',' ','2'):
        flag = 3;
        break;
      case GUPPI_RAW_KEY_UINT64_ID_LE('B',' ','K','E','Y',' ',' ','2'):
        flag = 4;
        break;
    }
  }
  stop = clock();
  printf("switch((uint64_t*)[0]): %f s\n", ((double)(stop-start))/CLOCKS_PER_SEC);

  return 0;
}