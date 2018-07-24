#include "cunitpp.h"
#include "proc-info.h"
#include "util.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef int (*TestCase)();

// Function to parse the test case name
typedef struct _SymbolName {
  const char* module;
  const char* name  ;
} SymbolName;

static void FreeSymbolName( SymbolName* name ) {
  free((void*)(name->module));
  free((void*)(name->name  ));
}

static int ParseSymbolName( const char* name , const char* prefix ,
                                               const char* sep    ,
                                               SymbolName* output  ) {
  if(strstr(name,prefix) == name) {
    const char* sep_pos;
    name += strlen(prefix); // advance to skip the prefix
    if((sep_pos = strstr(name,sep))) {
      output->module = SubStr(name,sep_pos);
      sep_pos += strlen(sep);
      output->name   = strdup(sep_pos);
      return 0;
    }
  }
  return -1;
}

// Parse a comma separated string into a list of string
static const char** ParseCommStr( const char* str ) {
  size_t sz = 0;
  size_t cap= 8;
  const char** ret = malloc(sizeof(const char*)*cap);
  const char* start = str;

  do {
    char* p = strchr(start,',');
    if(sz == cap) {
      ret = realloc(ret,sizeof(const char*)*cap*2);
      cap *= 2;
    }

    if(p) {
      ret[sz++] = SubStr(start,p);
    } else {
      ret[sz++] = strdup(start);
      break;
    }
  } while(1);

  ret[sz] = NULL;
  return ret;
}

typedef struct _TestEntry {
  const char*   name;
  void*      address;
} TestEntry;

typedef struct _TestEntryArray {
  TestEntry* arr;
  size_t     cap;
  size_t    size;
} TestEntryArray;

typedef struct _ModuleEntry {
  const char* module;
  TestEntryArray arr;
} ModuleEntry;

typedef struct _TestPlan  {
  ModuleEntry *module;
  size_t         size;
  size_t          cap;
} TestPlan;

enum {
  ST_INIT,
  ST_SKIP,
  ST_FOUND
};

typedef struct _TestPlanGenerator {
  TestPlan*    plan;
  // context variables
  TestEntry*   cur_entry;
  int          status;
  const char*  prefix;
  const char*  separator;
  int          run_all;
} TestPlanGenerator;

static TestEntry* AddTestEntry( TestEntryArray* arr ) {
  TestEntry* te;
  if(arr->cap == arr->size) {
    size_t ncap = arr->cap == 0 ? 2 : arr->cap * 2;
    arr->arr = realloc(arr->arr,sizeof(TestEntry)*ncap);
    arr->cap = ncap;
  }
  te = arr->arr + arr->size++;
  te->address = NULL;
  return te;
}

static ModuleEntry* AddModuleEntry( TestPlan* tp ) {
  if(tp->cap == tp->size) {
    size_t ncap = tp->cap == 0 ? 2 : tp->cap * 2;
    tp->module = realloc(tp->module,sizeof(ModuleEntry) * ncap);
    // reset rest to be 0
    memset(tp->module + tp->size,0,sizeof(ModuleEntry)* (ncap - tp->size));
    tp->cap = ncap;
  }
  return tp->module + tp->size++;
}

static void DeleteTestPlan( TestPlan* p ) {
  size_t i;
  for( i = 0 ; i < p->size ; ++i ) {
    ModuleEntry* me = p->module + i;
    free((void*)me->module);
    for( size_t j = 0 ; j < me->arr.size ; ++j ) {
      TestEntry* te = me->arr.arr + j;
      free((void*)(te->name));
    }
    free(me->arr.arr);
  }
  free(p->module);
  p->module = 0;
  p->cap    = 0;
  p->size   = 0;
}

static int SymbolBegin( void* d , const char* name ) {
  TestPlanGenerator* gen = d;
  SymbolName sn;

  gen->status= ST_INIT;

  if(ParseSymbolName(name,gen->prefix,gen->separator,&sn)) {
    gen->status= ST_SKIP;
    return PINFO_FOREACH_BREAK;
  }

  // check it against module list
  for( size_t i = 0 ; i < gen->plan->size; ++i ) {
    ModuleEntry* me = gen->plan->module + i;
    if(strcmp(me->module,sn.module) == 0) {
      gen->cur_entry = AddTestEntry(&me->arr);
      gen->cur_entry->name = sn.name;
      free((void*)sn.module);
      return PINFO_FOREACH_CONTINUE;
    }
  }

  if(gen->run_all) {
    ModuleEntry* me = AddModuleEntry(gen->plan);
    me->module = sn.module;
    gen->cur_entry = AddTestEntry(&(me->arr));
    gen->cur_entry->name = sn.name;
    return PINFO_FOREACH_CONTINUE;
  } else {
    FreeSymbolName(&sn);
    gen->status = ST_SKIP;
    return PINFO_FOREACH_BREAK;
  }
}

static int OnSymbol  ( void* d , void* addr , int weak ) {
  TestPlanGenerator* gen = d;
  if(!weak) {
    gen->cur_entry->address = addr;
    gen->status = ST_FOUND;
    return PINFO_FOREACH_BREAK;
  }
  return PINFO_FOREACH_CONTINUE;
}

static void SymbolEnd( void* d ) {
  (void)d;
}

static void PrepareTestPlan( struct ProcInfo* pinfo , TestPlan* tp , const char** module_list ) {
  TestPlanGenerator gen;
  gen.plan        = tp;
  gen.prefix      = CUNIT_SYMBOL_PREFIX;
  gen.separator   = CUNIT_MODULE_SEPARATOR;
  // initialize the module entry
  if(module_list) {
    size_t sz = 0;
    const char** p = module_list;
    for( ; *p ; ++p ) ++sz;
    tp->cap    = sz;
    tp->size   = sz;
    tp->module = calloc(sz,sizeof(ModuleEntry));
    for( size_t i = 0 ; i < sz ; ++i ) {
      tp->module->module = module_list[i];
    }
    gen.run_all = 0;
  } else {
    tp->size    = 0;
    tp->cap     = 0;
    tp->module  = NULL;
    gen.run_all = 1;
  }

  ForeachSymbol(pinfo,SymbolBegin,OnSymbol,SymbolEnd,&gen);
}

static void RunTestPlan    ( const TestPlan* tp ) {
  size_t i;
  for( i = 0 ; i < tp->size ; ++i ) {
    ModuleEntry* me = tp->module + i;
    fprintf(stderr,"[ MODULE  ] %s\n",me->module);
    for( size_t j = 0 ; j < me->arr.size ; ++j ) {
      TestEntry* t  = me->arr.arr + j;
      fprintf(stderr,"[ RUN     ] %s.%s\n",me->module,t->name);
      TestCase tc   = (TestCase)(t->address);
      if(tc && tc()) {
        fprintf(stderr,"[    FAIL ] %s.%s \n",me->module,t->name);
        break;
      } else {
        fprintf(stderr,"[      OK ] %s.%s \n",me->module,t->name);
      }
    }
    fprintf(stderr,"\n");
  }
}

static void RunAllModuleTest() {
  struct ProcInfo* pinfo;
  TestPlan tp;
  int rcode = CreateProcInfo(getpid(),&pinfo,PINFO_SRCH_MAIN_ONLY);
  if(rcode) {
    fprintf(stderr,"Cannot create proc info with error code %d\n",rcode);
    return;
  }
  PrepareTestPlan(pinfo,&tp,NULL);
  RunTestPlan(&tp);
  DeleteTestPlan(&tp);
  DeleteProcInfo(pinfo);
}

int main( int argc , char* argv[] ) {
  (void)argc;
  (void)argv;
  RunAllModuleTest();
  return 0;
}
