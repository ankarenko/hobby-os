#include <string.h>
#include <stdio.h>

void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
  unsigned char* dst = (unsigned char*)dstptr;
  const unsigned char* src = (const unsigned char*)srcptr;
  for (size_t i = 0; i < size; i++) {
    dst[i] = src[i];
    /*
    printf("\n%d != %d", dst[i], src[i]);
    if (dst[i] != src[i]) {
      uint16_t a = 1;
    }
    */
  }
  return dstptr;
}