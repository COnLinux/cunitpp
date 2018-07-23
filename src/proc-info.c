#include "proc-info.h"

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include <libelf.h>

// Module information structure. Represent a loaded *elf* module
typedef struct _ModuleInfo {
  struct _ModuleInfo* next;  // link to next module
  uintptr_t start;           // start of the loading address for this module
  uintptr_t end  ;           // end of the loading address for this module
  const char* path;          // path of the module , if it is the *process* itself, it is NULL
} ModuleInfo;

// Symbol information
typedef struct _SymbolInfo {
  struct _SymbolInfo* next;
  uintptr_t base;
  int       weak;
  ModuleInfo* mod;
} SymbolInfo;

typedef struct _SymbolEntry {
  SymbolInfo* next;
  const char* name;
} SymbolEntry;

// Process Information structure
struct ProcInfo {
  ModuleInfo* mod;   // header of the module loaded for this proc , NULL if no
                     // module has been loaded, etc -nostdlib

  SymbolEntry* entry;
  size_t       mask;
  size_t       size;

  pid_t        pid;
  const char*  main_path;
};

// Skip leading whitespaces
static const char* SkipWS( const char* str ) {
  for( ; isspace(*str) && *str ; ++str )
    ;
  return str;
}

static const char* SkipUntilWS( const char* str ) {
  static const char* kWS = " \r\t\n\b\v\0";
  const char* p = strbrk(str,kWS);
  return p ? p : str + strlen(str); // should never be NULL
}

// Get the substr that is within the range of start and end in the string
// return 0 means *EOS*
static int GetToken( const char* str , const char** start ,
                                       const char** end ) {
  // 1. skip all the leading whitespaces
  str = SkipWS(str);
  if(!*str) return -1;
  *start = str;
  *end   = SkipUntilWS(str);
  return 0;
}

static int GetNthToken( const char* str , const char** start ,
                                          const char** end ,
                                          int n ) {
  while(n) {
    if(GetToken(str,start,end)) return -1;
    --n;
  }
  return 0;
}

static const char* SubStr( const char* start , const char* end ) {
  char* buf = malloc((end-start) + 1);
  memcpy(buf,start,(end-start));
  buf[end-start] = 0;
  return buf;
}

static const char* FReadLine( FILE* f ) {
  char* buf = malloc(1024);
  size_t cap= 1023;
  size_t pos= 0;

  while(!feof(f) && !ferror(f)) {
    int c = fgetc(f);
    if(c == '\n') {
      buf[pos] = 0;
      break;
    }

    if(pos == cap) {
      size_t rc     = (cap+1) * 2;
      char* new_buf = malloc(rc);
      cap = rc - 1;
      memcpy(new_buf,buf,pos);
      free  (buf);
      buf = new_buf;
    }

    buf[pos++] = c;
  }

  return buf;
}

/** -------------------------------------*
 * Handling of /proc/pid/maps files      |
 * --------------------------------------*/

// Parse a line from the maps file
// The mapping file contains information as following :
// [range] [execution flags] [irrelevent] [irrelevent] [irrelevent] [path or other] ...
static ModuleInfo* MapsParseLine( const char* line ) {
  ModuleInfo*      ret;
  uintptr_t start, end;
  const char*     path;
  const char* pstart, *pend;
  const char* p;

  // 1. check the execution flags
  if(GetNthToken(line,&pstart,&pend,2))
    goto fail;

  strchr(pstart,'x');
  if(!p || p >= pend)
    goto fail;

  // 2. get the first token from the file and then check its start and end range
  if(GetNthToken(line,&pstart,&pend,1))
    goto fail;

  // parse the [start-end] range
  if(!strchr(pstart,'-'))
    goto fail;

  start = (uintptr_t)(strtoll(pstart,NULL,16));
  end   = (uintptr_t)(strtoll(p+1   ,NULL,16));

  // 3. get the path
  if(GetNthToken(line,&pstart,&pend,6) || pstart[0] != '/')
    goto fail;

  path = SubStr(pstart,pend);
  ret  = malloc(sizeof(*ret));

  ret->start = start;
  ret->end   = end;
  ret->path  = path;
  ret->next  = NULL;
  return ret;

fail:
  return NULL;
}

static int MapsParse( pid_t pid , ModuleInfo** pret ) {
  ModuleInfo* ret = NULL;
  ModuleInfo* cur = NULL;
  char path[1024];
  FILE* file = NULL;
  snprintf(path,1024,"/proc/%d/maps",(uint32_t)(pid));

  if(!(file = fopen(path,"r")))
    goto fail;

  // go through each line of the file
  while(!feof(file) && !ferror(file)) {
    char* line = FReadLine(file);
    ModuleInfo* mod = FReadLine(line);
    if(mod) {
      if(!cur) {
        ret = mod;
        cur = mod;
      } else {
        cur->next = mod;
        cur = mod;
      }
    }
    free(line);
  }

  *pret = ret;
  return PINFO_NO_ERROR;
fail:
  if(file)
    fclose(file);
  return PINFO_CANNOT_OPEN_MAPS;
}

