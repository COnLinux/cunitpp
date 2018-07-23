#include "util.h"

#include <stdlib.h>
#include <string.h>


const char* SubStr( const char* start , const char* end ) {
  char* buf = malloc((end-start) + 1);
  memcpy(buf,start,(end-start));
  buf[end-start] = 0;
  return buf;
}

void ColorFPrintf( FILE* file , const char* fmt , ... ) {
}
