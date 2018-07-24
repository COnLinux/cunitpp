#include "cunitpp.h"
#include "proc-info.h"
#include "util.h"

#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef void (*TestCase)();

// Use to simulate exception in C
static jmp_buf kTestEnv;

typedef struct _SymbolName {
  const char* module;
  const char* name  ;
} SymbolName;

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

/* ---------------------------------------------------------
 * Assertion Helper                                        |
 * --------------------------------------------------------*/
void _CUnitAssert( const char* file , int line , const char* format , ... ) {
  char buf[1024];
  va_list vl;
  size_t nret;
  va_start(vl,format);
  nret = snprintf(buf,1024,"Assertion failed around %d:%s => ",line,file);
  fwrite  (buf,nret,1,stderr);
  vfprintf(stderr,format,vl);
  longjmp(kTestEnv,1);
}

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

static int ExplodeSymbolName( const char* name , const char*  prefix,
                                                 const char*  sep ,
                                                 char*        mod ,
                                                 char*        sym ,
                                                 char*        buf ,
                                                 size_t       len ) {
  const char* pend = strchr(name,'.');
  if(pend) {
    size_t sz = strlen(pend+1);
    memcpy(mod,name,pend-name);
    mod[pend-name] = 0;
    memcpy(sym,pend+1,sz);
    sym[sz] = 0;
    snprintf(buf,len,"%s%s%s%s",prefix,mod,sep,sym);
    return 0;
  }
  return 1;
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

static void PrepareTestPlan( struct ProcInfo* pinfo , TestPlan* tp ,
                                                      const char** module_list ) {
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
      tp->module[i].module = module_list[i];
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

static uint64_t TimeGetNow() {
  struct timespec res;
  clock_gettime(CLOCK_MONOTONIC,&res);
  return res.tv_sec * 1000 + res.tv_nsec /1000000;
}

static int RunTest( void* address , FILE* file , const char* module ,
                                                 const char* name   ) {
  TestCase tc = (TestCase)(address);
  ColorFPrintf(stderr,"Bold","Blue",NULL,"[ RUN     ] ");
  fprintf     (stderr,"%s.%s\n",module,name);

  if(setjmp(kTestEnv) == 0) {
    uint64_t start,end;

    start = TimeGetNow();
    tc();
    end   = TimeGetNow();

    ColorFPrintf(stderr,"Bold","Green",NULL,"[      OK ] ");
    fprintf     (stderr,"%s.%s (%lldms)\n",module,name,(long long int)(end-start));
    return 0;
  } else {
    ColorFPrintf(stderr,"Bold","Red",NULL,"[    FAIL ] ");
    fprintf     (stderr,"%s.%s\n",module,name);
    return -1;
  }
}

static int RunTestPlan( const TestPlan* tp ) {
  size_t i;
  int rcode = 0;
  for( i = 0 ; i < tp->size ; ++i ) {
    ModuleEntry* me = tp->module + i;
    ColorFPrintf(stderr,"Bold","Blue",NULL,"[ MODULE  ] ",me->module);
    fprintf     (stderr,"%s\n",me->module);
    for( size_t j = 0 ; j < me->arr.size ; ++j ) {
      TestEntry* t  = me->arr.arr + j;
      if(t->address) {
        rcode = RunTest(t->address,stderr,me->module,t->name);
      }
    }
    fprintf(stderr,"\n");
  }
  return rcode;
}

static int RunModuleTest( const char** module_list , int opt ) {
  TestPlan tp;
  struct ProcInfo* pinfo;
  int rcode;

  if((rcode = CreateProcInfo(getpid(),&pinfo,opt))) {
    fprintf(stderr,"Cannot create ProcInfo object because of error code %d\n",rcode);
    return -1;
  }

  PrepareTestPlan(pinfo,&tp,module_list);
  RunTestPlan(&tp);

  DeleteTestPlan(&tp);
  DeleteProcInfo(pinfo);
  return rcode;
}

static int RunTestList( const char** test_list , int opt ) {
  char buf[1024];
  char mod[1024];
  char sym[1024];

  struct ProcInfo* pinfo;
  int rcode = CreateProcInfo(getpid(),&pinfo,opt);
  if(rcode) {
    fprintf(stderr,"Cannot create ProcInfo object beacuse of error code %d\n",rcode);
    return -1;
  }

  for( ; *test_list ; ++test_list ) {
    void* address;
    if(ExplodeSymbolName(*test_list,CUNIT_SYMBOL_PREFIX,
                                    CUNIT_MODULE_SEPARATOR,
                                    mod,
                                    sym,
                                    buf,
                                    1024)) {
      fprintf(stderr,"Test %s is not a valid name\n",*test_list);
      rcode = -1;
    } else {
      address = FindStrongSymbol(pinfo,buf);
      if(!address) {
        fprintf(stderr,"Test %s is not found\n",*test_list);
        rcode = -1;
      }
      RunTest(address,stderr,mod,sym);
    }
  }

  DeleteProcInfo(pinfo);
  return rcode;
}

static int ListAllTest( int opt ) {
  size_t i;
  TestPlan tp;
  struct ProcInfo* pinfo;
  int rcode = CreateProcInfo(getpid(),&pinfo,opt);
  if(rcode) {
    fprintf(stderr,"Cannot create ProcInfo because of error code %d\n",rcode);
    return -1;
  }

  PrepareTestPlan(pinfo,&tp,NULL);
  for( i = 0 ; i < tp.size ; ++i ) {
    ModuleEntry* me = tp.module + i;
    ColorFPrintf(stderr,"Bold","Green",NULL,"[ MODULE  ] ");
    fprintf     (stderr,"%s\n",me->module);
    for( size_t j = 0 ; j < me->arr.size ; ++j ) {
      TestEntry* t = me->arr.arr + j;
      if(t->address) {
        ColorFPrintf(stderr,"Bold","Green",NULL,"[ TEST    ] ");
        fprintf     (stderr,"%s.%s\n",me->module,t->name);
      }
    }
    fprintf(stderr,"\n");
  }

  DeleteProcInfo(pinfo);
  return 0;
}

/* --------------------------------------------
 * Command Line Parser                        |
 * -------------------------------------------*/
typedef struct _CmdOption {
  int opt ;
  int list;
  const char** module_list;
  const char** test_list;
} CmdOption;

static void FreeStrList( const char** slist ) {
  void* m = (void*)slist;
  for( ; *slist ; ++slist ) {
    free((void*)*slist);
  }
  free(m);
}

static void DeleteCmdOption( CmdOption* opt ) {
  if(opt->module_list) FreeStrList(opt->module_list);
  if(opt->test_list  ) FreeStrList(opt->test_list  );
}

static void ShowError( const char* fmt , ... ) {
  static const char* kError =
    "--list-tests:\n"
    "  Show all tests in different module\n"
    "--module-list:\n"
    "  Specify a comma separated list to filter out specific tests to run\n"
    "--test-list:\n"
    "  Specify a comma separated list to filter out specific tests to run\n"
    "--option:   \n"
    "  Specify the searching option for test cases, the value can be *Main* or *All*.\n"
    "  *Main* means only search this executable program ,*All* means search all the  \n"
    "  shared object as well\n";

  char buf[1024];
  va_list vl;
  va_start(vl,fmt);
  vsnprintf(buf,1024,fmt,vl);

  fprintf(stderr,"%s\n",buf);
  fprintf(stderr,"%s"  ,kError);
}

static const char** ParseCommaList( const char* str ) {
  size_t sz = 0;
  const char** ret;

  {
    const char* s = str;
    do {
      const char* p = strpbrk(s,",;");
      if(p) {
        ++sz;
        s = p+1;
      } else {
        ++sz;
        break;
      }
    } while(1);
  }

  ret = malloc(sizeof(const char*)*sz + 1);
  sz  = 0;
  {
    do {
      const char* p =strpbrk(str,",;");
      if(p) {
        ret[sz++] = SubStr(str,p);
      } else {
        ret[sz++] = strdup(str);
        break;
      }
      str = p + 1;
    } while(1);
  }

  ret[sz] = NULL;

  return ret;
}

static int ParseCommandLine( int argc , char** argv , CmdOption* opt ) {
  int i = 1;
  opt->opt         = PINFO_SRCH_MAIN_ONLY;
  opt->list        = -1;
  opt->module_list = NULL;
  opt->test_list   = NULL;

  for( ; i < argc ; ++i ) {
    if(strcmp(argv[i],"--help") == 0) {
      ShowError("help");
      goto fail;
    } else if(strcmp(argv[i],"--list-tests") == 0) {
      if(opt->list != -1) {
        ShowError("--list-tests duplicated");
        goto fail;
      }
      opt->list = 1;
    } else if(strcmp(argv[i],"--module-list") == 0) {
      if(opt->module_list != NULL) {
        ShowError("--module-list duplicated");
        goto fail;
      }
      opt->module_list = ParseCommaList(argv[++i]);
    } else if(strcmp(argv[i],"--test-list") == 0) {
      if(opt->test_list != NULL) {
        ShowError("--test-list duplicated");
        goto fail;
      }
      opt->test_list = ParseCommaList(argv[++i]);
    } else if(strcmp(argv[i],"--option") == 0) {
      if(strcmp(argv[++i],"All") == 0) {
        opt->opt = PINFO_SRCH_ALL;
      } else {
        opt->opt = PINFO_SRCH_MAIN_ONLY;
      }
    } else {
      ShowError("unknown option %s",argv[i]);
      goto fail;
    }
  }

  if(opt->list == -1) opt->list = 0;
  return 0;
fail:
  DeleteCmdOption(opt);
  return -1;
}

int main( int argc , char* argv[] ) {
  CmdOption opt;
  int rcode;

  if(ParseCommandLine(argc,argv,&opt)) {
    return -1;
  }

  if(opt.list) {
    rcode = ListAllTest(opt.opt);
  } else if(opt.test_list) {
    rcode = RunTestList(opt.test_list,opt.opt);
  } else {
    rcode = RunModuleTest(opt.module_list,opt.opt);
  }

  DeleteCmdOption(&opt);
  return rcode;
}
