#include "proc-info.h"
#include "util.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

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
  uintptr_t  base;
  int        weak;
  ModuleInfo* mod;
} SymbolInfo;

typedef struct _SymbolEntry {
  struct _SymbolEntry* chain;
  SymbolInfo* info;
  const char* name;
  uint64_t    hash;
} SymbolEntry;

// Process Information structure
struct ProcInfo {
  ModuleInfo* mod;   // header of the module loaded for this proc , NULL if no
                     // module has been loaded, etc -nostdlib

  // hash table for symbol entry using open addressing
  SymbolEntry* entry;
  size_t       mask;
  size_t       size;
};

// Skip leading whitespaces
static const char* SkipWhitespace( const char* str ) {
  for( ; isspace(*str) && *str ; ++str )
    ;
  return str;
}

static const char* SkipUntilWhitespace( const char* str ) {
  static const char* kWhitespace = " \r\t\n\b\v\0";
  const char* p = strpbrk(str,kWhitespace);
  return p ? p : str + strlen(str); // should never be NULL
}

// Get the substr that is within the range of start and end in the string
// return 0 means *EOS*
static int GetToken( const char* str , const char** start ,
                                       const char** end ) {
  // 1. skip all the leading whitespaces
  str = SkipWhitespace(str);
  if(!*str) return -1;
  *start = str;
  *end   = SkipUntilWhitespace(str);
  return 0;
}

static int GetNthToken( const char* str , const char** start ,
                                          const char** end ,
                                          int n ) {
  const char* s = str;
  while(n) {
    if(GetToken(s,start,end)) return -1;
    s = *end;
    --n;
  }
  return 0;
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

  if(ferror(f)) {
    free(buf);
    buf = NULL;
  } else if(feof(f)) {
    buf[pos] = 0;
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

  p = strchr(pstart,'x');
  if(!p || p >= pend)
    goto fail;

  // 2. get the first token from the file and then check its start and end range
  if(GetNthToken(line,&pstart,&pend,1))
    goto fail;

  // parse the [start-end] range
  if(!(p=strchr(pstart,'-')))
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

static int MapsParse( pid_t pid , ModuleInfo** pret , int opt ) {
  ModuleInfo* ret = NULL;
  ModuleInfo* cur = NULL;
  char path[1024];
  FILE* file = NULL;
  snprintf(path,1024,"/proc/%d/maps",(uint32_t)(pid));

  if(!(file = fopen(path,"r")))
    goto fail;

  // go through each line of the file
  while(!feof(file) && !ferror(file)) {
    const char* line = FReadLine(file);
    if(line) {
      ModuleInfo* mod = MapsParseLine(line);
      if(mod) {
        if(!cur) {
          ret = mod;
          cur = mod;
          // check if we can just bailout here if we only need to search the main
          // app instead of the full dynamic linked library
          if(opt == PINFO_SRCH_MAIN_ONLY) {
            free((void*)line);
            break;
          }
        } else {
          cur->next = mod;
          cur = mod;
        }
      }
    }
    free((void*)line);
  }

  *pret = ret;
  return PINFO_NO_ERROR;
fail:
  if(file)
    fclose(file);
  return PINFO_CANNOT_OPEN_MAPS;
}

/* ---------------------------------------------
 * Symbol Table Hash                           |
 * --------------------------------------------*/
static uint64_t StrHash( const char* str , size_t len ) {
  uint64_t ret = 17771;
  size_t     i = 0;
  for( ; i < len ; ++i ) {
    ret = ret ^ ((ret<<5) + (ret>>2) + str[i]);
  }
  return ret;
}

#define OPT_INSERT 0
#define OPT_QUERY  1

static SymbolEntry* _SymbolFindEntry( struct ProcInfo* pinfo ,
                                      const char* name       ,
                                      uint64_t    hash       ,
                                      int          opt        ) {
  SymbolEntry* prev  = NULL;
  SymbolEntry* entry = pinfo->entry + (pinfo->mask & hash);
  if(!entry->name) {
    // main position of the table is not in used
    if(opt == OPT_INSERT)
      return entry;
    else
      return NULL;
  }

  // do a chain resolution
  do {
    if(entry->hash == hash && strcmp(entry->name,name) == 0) {
      return entry;
    }
    prev = entry;
    entry= entry->chain;
  } while(entry);

  // when we reach here it means we cannot find a entry with the
  // same name and hash value
  if(opt == OPT_QUERY) return NULL;

  // do a linear probing to find an empty slot to do the insertion
  do {
    entry = pinfo->entry + (++hash & pinfo->mask);
  } while(entry->name);

  prev->chain = entry;
  return entry;
}

static void _SymbolRehash( struct ProcInfo* pinfo ) {
  struct ProcInfo temp;
  size_t i;
  SymbolEntry* e;
  SymbolEntry* new_e;
  size_t newcap = pinfo->size * 2;

  assert( pinfo->mask + 1 == pinfo->size );

  temp.mask = newcap - 1;
  temp.entry= calloc(newcap,sizeof(SymbolEntry));

  for( i = 0 ; i < pinfo->size ; ++i ) {
    e = pinfo->entry + i;
    assert(e->name);
    new_e = _SymbolFindEntry(&temp,e->name,e->hash,OPT_INSERT);
    assert(new_e);
    new_e->name = e->name;
    new_e->hash = e->hash;
    new_e->info = e->info;
  }

  free(pinfo->entry);

  pinfo->mask = temp.mask;
  pinfo->entry= temp.entry;
}

static SymbolInfo* SymbolInsert( struct ProcInfo* pinfo , const char* name ) {
  SymbolInfo* ret;
  SymbolEntry* e;
  uint64_t  hash = StrHash(name,strlen(name));

  if(pinfo->mask+1 == pinfo->size)
    _SymbolRehash(pinfo);

  // get the symbol from the hash table
  e = _SymbolFindEntry(pinfo,name,hash,OPT_INSERT);
  assert(e);
  if(!e->name) {
    ++(pinfo->size);
    // now insert a new symbol info entry into the chain
    e->hash = hash;
    e->name = strdup(name);
  }

  ret = malloc(sizeof(SymbolInfo));
  ret->next = e->info;
  e->info   = ret;

  return ret;
}

static SymbolEntry* SymbolFind( struct ProcInfo* pinfo, const char* name ) {
  return _SymbolFindEntry(pinfo,name,StrHash(name,strlen(name)),OPT_QUERY);
}

static void SymbolDelete( struct ProcInfo* pinfo ) {
  size_t i;
  for( i = 0 ; i < pinfo->mask + 1; ++i ) {
    SymbolEntry* e = pinfo->entry + i;
    if(e->name) {
      SymbolInfo*  t = e->info;
      free((void*)e->name);
      do {
        SymbolInfo* temp = t->next;
        free(t);
        t = temp;
      } while(t);
    }
  }
  free(pinfo->entry);

  pinfo->entry = NULL;
  pinfo->mask  = 0;
  pinfo->size  = 0;
}

// Load a elf section from a openned elf stream
static void LoadElfSection( Elf* elf , struct ProcInfo* pinfo , ModuleInfo* mod ,
                                                                int (*predicate)(int) ,
                                                                int bmain ) {
  Elf_Scn*     elf_section = NULL;
  Elf64_Shdr*  elf_shdr;
  uintptr_t    offset = bmain ? 0 : mod->start;

  while((elf_section = elf_nextscn(elf,elf_section)) != NULL) {
    if((elf_shdr = elf64_getshdr(elf_section)) != NULL) {
      if(predicate(elf_shdr->sh_type)) {
        Elf_Data*  elf_data = elf_getdata(elf_section,NULL);
        Elf64_Sym* elf_sym;
        Elf64_Sym* elf_end;

        if(elf_data == NULL || elf_data->d_size == 0) {
          break;
        }

        elf_sym = (Elf64_Sym*)(elf_data->d_buf);
        elf_end = (Elf64_Sym*)((char*)elf_data->d_buf + elf_data->d_size);

        for( ; elf_sym != elf_end ; elf_sym++ ) {
          const char* name;
          SymbolInfo* si;

          if(elf_sym->st_value == 0 ||
             (ELF64_ST_BIND(elf_sym->st_info) == STB_NUM) ||
             (ELF64_ST_TYPE(elf_sym->st_info) != STT_FUNC)) {
            continue; // none function type
          }

#ifndef CONFIG_ALLOW_WEAK_FUNCTION
          if(ELF64_ST_BIND(elf_sym->st_info) == STB_WEAK) continue;
#endif // CONFIG_ALLOW_WEAK_FUNCTION

          // now we have a function symbol here
          name = elf_strptr(elf,elf_shdr->sh_link,(size_t)(elf_sym->st_name));
          si   = SymbolInsert(pinfo,name);

#ifdef CONFIG_ALLOW_WEAK_FUNCTION
          si->weak = ELF64_ST_BIND(elf_sym->st_info) == STB_WEAK;
#else
          si->weak = 0;
#endif // CONFIG_ALLOW_WEAK_FUNCTION

          si->mod  = mod;
          si->base = elf_sym->st_value + offset;
        }
      }
    }
  }
}

static int _MainProgramElfPredicate( int type ) {
  return type == SHT_SYMTAB || type == SHT_DYNSYM;
}

static int _DynProgramElfPredicate ( int type ) {
  return type == SHT_DYNSYM;
}

static int LoadElf( struct ProcInfo* pinfo , ModuleInfo* mod , int main ) {
  int fd = open(mod->path,O_RDONLY);
  Elf* elf;
  if(!fd) {
    return PINFO_CANNOT_OPEN_ELF;
  }

  elf = elf_begin(fd,ELF_C_READ,NULL);
  if(!elf) {
    goto fail;
  }
  if(main) {
    LoadElfSection(elf,pinfo,mod,_MainProgramElfPredicate,main);
  } else {
    LoadElfSection(elf,pinfo,mod,_DynProgramElfPredicate ,main);
  }

  elf_end(elf);
  close(fd);
  return PINFO_NO_ERROR;

fail:
  if(elf) elf_end(elf);
  close(fd);
  return PINFO_ELF_ERROR;
}

int CreateProcInfo( pid_t pid , struct ProcInfo** ret , int opt ) {
  int rcode;
  struct ProcInfo* pinfo = malloc(sizeof(*pinfo));

  elf_version(EV_CURRENT);

  // initialize the symbol table
  {
    size_t cap = 1024;
    pinfo->mask  = cap - 1;
    pinfo->size  = 0;
    pinfo->entry = calloc(cap,sizeof(SymbolEntry));
  }

  // 1. parse the maps file
  if((rcode=MapsParse(pid,&(pinfo->mod),opt))) return rcode;

  // 2. go through each of the modules and do the elf parsing
  {
    int bmain = 1;
    ModuleInfo* m = pinfo->mod;
    for( ; m ; m = m->next ) {
      if((rcode = LoadElf(pinfo,m,bmain))) goto fail;
      bmain = 0;
    }
  }

  *ret = pinfo;
  return PINFO_NO_ERROR;

fail:
  DeleteProcInfo(pinfo);
  return rcode;
}

void DumpProcInfo( const struct ProcInfo* pinfo , FILE* output ) {
  // 1. dump the module list
  {
    ModuleInfo* mod = pinfo->mod;
    fprintf(output,"------------------------ module list ----------------------------------\n");
    for( ; mod ; mod = mod->next ) {
      fprintf(output,"Module:%s,Start:%p,End:%p\n",mod->path,(void*)mod->start,(void*)mod->end);
    }
    fprintf(output,"-----------------------------------------------------------------------\n");
  }

  // 2. dump all the loaded symbol
  {
    size_t i = 0;
    fprintf(output,"------------------------ symbol list ----------------------------------\n");
    for( ; i < pinfo->mask + 1 ; ++i ) {
      SymbolEntry* se = pinfo->entry + i;
      if(se->name) {
        SymbolInfo* si = se->info;
        for( ; si ; si = si->next ) {
          fprintf(output,"Symbol:%s,Addr:%p,Module:%s,Weak:%d\n",se->name,(void*)si->base,
                                                                          si->mod->path,
                                                                          si->weak);
        }
      }
    }
    fprintf(output,"-----------------------------------------------------------------------\n");
  }
}

void DeleteProcInfo( struct ProcInfo* pinfo ) {
  // 1. delete the module list
  {
    ModuleInfo* mod = pinfo->mod;
    while(mod) {
      ModuleInfo* temp = mod->next;
      free((void*)mod->path);
      free(mod);
      mod = temp;
    }
  }

  // 2. delete hash table
  SymbolDelete(pinfo);

  // 3. free itself
  free(pinfo);
}

void* FindStrongSymbol( struct ProcInfo* pinfo , const char* name ) {
  SymbolEntry* e = SymbolFind(pinfo,name);
  if(e) {
    // walk through the list to find a strong symbol
    for( SymbolInfo* info = e->info ; info ; info = info->next ) {
      if(!info->weak) {
        return (void*)(info->base);
      }
    }
  }
  return NULL;
}

void ForeachSymbol( struct ProcInfo* pinfo , BeginSymbolCallback ncb ,
                                             OnSymbolCallback     cb ,
                                             EndSymbolCallback   ecb ,
                                             void*              data ) {
  size_t i = 0;
  for( ; i < pinfo->mask + 1 ; ++i ) {
    SymbolEntry* e = pinfo->entry + i;
    if(e->name) {
      switch(ncb(data,e->name)) {
        case PINFO_FOREACH_BREAK:            goto repeat;
        case PINFO_FOREACH_STOP : ecb(data); goto done  ;
        default: break;
      }

      for( SymbolInfo* info = e->info; info ; info = info->next ) {
        switch(cb(data,(void*)(info->base),info->weak)) {
          case PINFO_FOREACH_BREAK:            goto repeat;
          case PINFO_FOREACH_STOP : ecb(data); goto done  ;
          default: break;
        }
      }
    }

repeat:
    // invoke end of callback functions
    ecb(data);
  }

done:
  return;
}
