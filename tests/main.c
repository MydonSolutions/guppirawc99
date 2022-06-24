#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "guppiraw.h"

int main(){
  char key[9] = "STRING  ";
  uint64_t number = 0;
  for(int i = 0; i < 8; i++){
    number += ((uint64_t)key[i]) << (i*8);
  }

  printf("ref: %lu\n", ((uint64_t*)key)[0]);
  printf("cal: %lu\n", number);
  printf("def: %lu\n", KEY_UINT64_ID_LE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]));
  printf("def: %lu\n", KEY_UINT64_ID_BE(key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]));
  return 0;
}