#include "util.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


const char* SubStr( const char* start , const char* end ) {
  char* buf = malloc((end-start) + 1);
  memcpy(buf,start,(end-start));
  buf[end-start] = 0;
  return buf;
}

struct Mapping {
  const char* key;
  const char* val;
};

static const char* GetFormatCode( const char* format ) {
  static const struct Mapping kAttr[] = {
    {"Default" , "0"},
    {"Bold"    , "1"},
    {"Dim"     , "2"},
    {"Underlined", "3"},
    {"Blink"   , "5" },
    {"Reverse" , "7" },
    {"Hidden"  , "8" }
  };
  if(format) {
    size_t i;
    for( i = 0 ; i < ARRAY_SIZE(kAttr); ++i ) {
      if(strcmp(kAttr[i].key,format) == 0)
        return kAttr[i].val;
    }
  }
  return "0";
}

static const char* GetFgColor( const char* fg ) {
  static const struct Mapping kFg[] = {
    {"Default" , "39"},
    {"Black"   , "30"},
    {"Red"     , "31"},
    {"Green"   , "32"},
    {"Yellow"  , "33"},
    {"Blue"    , "34"},
    {"Magenta" , "35"},
    {"Cyan"    , "36"},
    {"Light Gray" , "37" },
    {"White"   , "97" }
  };
  if(fg) {
    size_t i;
    for( i = 0 ; i < ARRAY_SIZE(kFg); ++i ) {
      if(strcmp(kFg[i].key,fg) == 0)
        return kFg[i].val;
    }
  }
  return "39";
}

static const char* GetBgColor( const char* bg ) {
  static const struct Mapping kBg[] = {
    {"Default" , "49"},
    {"Black"   , "40"},
    {"Red"     , "41"},
    {"Green"   , "42"},
    {"Yellow"  , "43"},
    {"Blue"    , "44"},
    {"Megenta" , "45"},
    {"Cyan"    , "46"},
    {"White"   , "107"}
  };
  if(bg) {
    size_t i;
    for( i = 0 ; i < ARRAY_SIZE(kBg); ++i ) {
      if(strcmp(kBg[i].key,bg) == 0)
        return kBg[i].val;
    }
  }
  return "49";
}

static const char* kReset = "\033[0m";

void ColorFPrintf( FILE* file , const char* format , const char* fg  ,
                                                     const char* bg  ,
                                                     const char* fmt , ... ) {
  char buf[1024];
  va_list vl;
  size_t sz;
  va_start(vl,fmt);
  sz = vsnprintf(buf,1024,fmt,vl);
  fprintf(file,"\033[%s;%s;%sm",GetFormatCode(format),GetFgColor(fg),GetBgColor(bg));
  fwrite (buf ,sz,1,file);
  fwrite (kReset,strlen(kReset),1,file);
}
