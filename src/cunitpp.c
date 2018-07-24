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

// Simple Test function prototype
typedef void (*SimpleTest)();

// Fixture Test function prototype
typedef void* (*FixtureSetup   )();
typedef void  (*FixtureTest    )(void*);
typedef void  (*FixtureTearDown)(void*);

// Use to simulate exception in C
static jmp_buf kTestEnv;

// Define test type that supported by the framework
#define TT_UNKNOWN (0)
#define TT_SIMPLE  (1)
#define TT_FIXTURE (2)

// Internal used symbol name type
#define ST_UNKNOWN         (-1)
#define ST_SIMPLE_TEST      (0)
#define ST_FIXTURE_SETUP    (1)
#define ST_FIXTURE_TEARDOWN (2)
#define ST_FIXTURE_TEST     (3)

enum {
  ST_INIT,
  ST_SKIP,
  ST_FOUND
};

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
  const char*    module;
  TestEntryArray arr;
  int tt;
  // only used for fixture test
  FixtureSetup    setup;
  FixtureTearDown tear_down;
} ModuleEntry;

typedef struct _TestPlan  {
  ModuleEntry *module;
  size_t         size;
  size_t          cap;
} TestPlan;

typedef struct _TestPlanGenerator {
  TestPlan*    plan;
  // context variables
  int          tt;
  union {
    TestEntry*      entry;
    ModuleEntry*    module;
  } cur;

  int          run_all;
} TestPlanGenerator;

static const char* GetTTName( int tt ) {
  switch(tt) {
    case TT_SIMPLE:  return "T";
    case TT_FIXTURE: return "F";
    default:         return NULL;
  }
}

static uint64_t TimeGetNow() {
  struct timespec res;
  clock_gettime(CLOCK_MONOTONIC,&res);
  return res.tv_sec * 1000 + res.tv_nsec /1000000;
}

static void FreeSymbolName( SymbolName* name ) {
  free((void*)(name->module));
  free((void*)(name->name  ));
}

// resolve the symbol to check whether it is a unknown symbol to our framework
static int ParseSymbolName( const char* name , SymbolName* output ) {
  int tt;
  const char* p;

  // check whether the symbol has our designed the prefix, if not then
  // just return it is not known to the framework
  if(strstr(name,CUNIT_SYMBOL_PREFIX) == name) {
    name += strlen(CUNIT_SYMBOL_PREFIX); // advance and look for the meta character
    switch(*name) {
      case CUNIT_SIMPLE_TEST     : tt = ST_SIMPLE_TEST;      break;
      case CUNIT_FIXTURE_TEST    : tt = ST_FIXTURE_TEST;     break;
      case CUNIT_FIXTURE_SETUP   : tt = ST_FIXTURE_SETUP;    break;
      case CUNIT_FIXTURE_TEARDOWN: tt = ST_FIXTURE_TEARDOWN; break;
      default: goto unknown;
    }

    name ++; // skip the meta character
    p = strstr(name,CUNIT_MODULE_SEPARATOR);
    if(p) {
      output->module = SubStr(name,p);
      output->name   = strdup(p+strlen(CUNIT_MODULE_SEPARATOR));
      return tt;
    }
  }

unknown:
  return ST_UNKNOWN;
}

static int ExplodeSymbolName( const char* name , int           tt ,
                                                 char*        mod ,
                                                 char*        sym ,
                                                 char*        buf ,
                                                 size_t       len ) {

  const char* pend = strchr(name,'.');
  if(pend) {
    size_t sz = strlen(pend+1);
    char   mt;
    memcpy(mod,name,pend-name);
    mod[pend-name] = 0;
    memcpy(sym,pend+1,sz);
    sym[sz] = 0;

    switch(tt) {
      case ST_SIMPLE_TEST     :  mt = CUNIT_SIMPLE_TEST;      break;
      case ST_FIXTURE_TEST    :  mt = CUNIT_FIXTURE_TEST;     break;
      case ST_FIXTURE_SETUP   :  mt = CUNIT_FIXTURE_SETUP;    break;
      case ST_FIXTURE_TEARDOWN:  mt = CUNIT_FIXTURE_TEARDOWN; break;
      default: return -1;
    }
    snprintf(buf,len,"%s%c%s%s%s",CUNIT_SYMBOL_PREFIX,mt,mod,CUNIT_MODULE_SEPARATOR,sym);
    return 0;
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

static ModuleEntry* FindOrAddModule( TestPlanGenerator* gen , const char* module, int tt ) {
  size_t i;
  for( i = 0 ; i < gen->plan->size ; ++i ) {
    ModuleEntry* me = gen->plan->module + i;
    if(strcmp(me->module,module) == 0) {
      if(me->tt != TT_UNKNOWN) {
        if(me->tt != tt) {
          return NULL;
        }
      } else {
        me->tt = tt;
      }
      return me;
    }
  }

  if(gen->run_all) {
    ModuleEntry* me = AddModuleEntry(gen->plan);
    me->tt = tt;
    return me;
  }

  return NULL;
}

static int SymbolBegin( void* d , const char* name ) {
  TestPlanGenerator* gen = d;
  SymbolName sn;

  gen->tt = ParseSymbolName(name,&sn);

  switch(gen->tt) {
    case ST_UNKNOWN    :
      goto brk;
    case ST_SIMPLE_TEST:
    case ST_FIXTURE_TEST:
      {
        ModuleEntry* me = FindOrAddModule(gen,
                                          sn.module,
                                          gen->tt == ST_SIMPLE_TEST ? TT_SIMPLE : TT_FIXTURE);
        if(me) {
          if(!me->module)
            me->module = sn.module;
          else
            free((void*)sn.module);

          gen->cur.entry = AddTestEntry(&me->arr);
          gen->cur.entry->name = sn.name;
          goto cont;
        } else {
          FreeSymbolName(&sn);
          goto brk;
        }
      }
      break;

    case ST_FIXTURE_SETUP:
    case ST_FIXTURE_TEARDOWN:
      {
        ModuleEntry* me = FindOrAddModule(gen,sn.module,TT_FIXTURE);
        if(me) {
          if(me) {
            if(!me->module) {
              me->module = sn.module;
              free((void*)sn.name);
            } else {
              FreeSymbolName(&sn);
            }
            gen->cur.module = me;
            goto cont;
          }
        } else {
          FreeSymbolName(&sn);
          goto brk;
        }
      }
      break;
    default:
      break;
  }

cont:
  return PINFO_FOREACH_CONTINUE;

brk:
  return PINFO_FOREACH_BREAK;
}

static int OnSymbol  ( void* d , void* addr , int weak ) {
  TestPlanGenerator* gen = d;
  if(!weak) {
    switch(gen->tt) {
      case ST_SIMPLE_TEST:
      case ST_FIXTURE_TEST:
        gen->cur.entry->address    = addr;
        break;
      case ST_FIXTURE_SETUP:
        gen->cur.module->setup     = addr;
        break;
      case ST_FIXTURE_TEARDOWN:
        gen->cur.module->tear_down = addr;
        break;
      default:
        break;
    }
    return PINFO_FOREACH_BREAK;
  }
  return PINFO_FOREACH_CONTINUE;
}

static void SymbolEnd( void* d ) {
  (void)d;
}

static void ShowSeparator() {
  ColorFPrintf(stderr,"Bold","Megenta",NULL,"[---------]\n");
}

static void PrepareTestPlan( struct ProcInfo* pinfo , TestPlan* tp ,
                                                      const char** module_list ) {
  TestPlanGenerator gen;
  gen.plan        = tp;

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

static void ShowError( const char* fmt , ... ) {
  char buf[1024];
  va_list vl;
  va_start(vl,fmt);
  vsnprintf(buf,1024,fmt,vl);
  ColorFPrintf(stderr,"Bold","Red",NULL,"[ ERROR   ] %s\n",buf);
}

static int RunTest( void* address , FILE* file , const char* module ,
                                                 const char* name   ,
                                                 int            tt  ,
                                                 void*          ctx  ) {
  ColorFPrintf(stderr,NULL,"Blue",NULL,"[ RUN     ] ");
  fprintf     (stderr,"%s.%s\n",module,name);

  if(setjmp(kTestEnv) == 0) {
    uint64_t start,end;

    start = TimeGetNow();
    switch(tt) {
      case TT_SIMPLE:
        {
          SimpleTest tc = (SimpleTest)(address);
          tc();
        }
        break;
      case TT_FIXTURE:
        {
          FixtureTest ft = (FixtureTest)(address);
          ft(ctx);
        }
        break;
      default:
        break;
    }
    end   = TimeGetNow();

    ColorFPrintf(stderr,NULL,"Green",NULL,"[      OK ] ");
    fprintf     (stderr,"%s.%s (%lldms)\n",module,name,(long long int)(end-start));
    return 0;
  } else {
    ColorFPrintf(stderr,NULL,"Red",NULL,"[    FAIL ] ");
    fprintf     (stderr,"%s.%s\n",module,name);
    return -1;
  }
}

static int RunTestPlan( const TestPlan* tp ) {
  size_t i;
  int rcode = 0;
  for( i = 0 ; i < tp->size ; ++i ) {
    ModuleEntry* me = tp->module + i;
    ColorFPrintf(stderr,NULL,"Blue",NULL,"[ SUITE(%s)] ",GetTTName(me->tt));
    fprintf     (stderr,"%s\n",me->module);

    switch(me->tt) {
      case TT_SIMPLE:
        for( size_t j = 0 ; j < me->arr.size ; ++j ) {
          TestEntry* t  = me->arr.arr + j;
          if(t->address) {
            rcode = RunTest(t->address,stderr,me->module,t->name,TT_SIMPLE,NULL);
          }
        }
        break;
      case TT_FIXTURE:
        {
          void* ctx = NULL;

          if(me->setup) {
            ColorFPrintf(stderr,NULL,"Blue",NULL,"[ SETUP   ]\n");
            ctx = me->setup();
          }

          for( size_t j = 0 ; j < me->arr.size ; ++j ) {
            TestEntry* t = me->arr.arr + j;
            if(t->address) {
              rcode = RunTest(t->address,stderr,me->module,t->name,TT_FIXTURE,ctx);
            }
          }

          if(me->setup && me->tear_down) {
            ColorFPrintf(stderr,NULL,"Blue",NULL,"[ TEARDOWN]\n");
            me->tear_down(ctx);
          }
        }
        break;
      default:
        break;
    }
    ShowSeparator();
  }
  return rcode;
}

static int RunModuleTest( const char** module_list , int opt ) {
  TestPlan tp;
  struct ProcInfo* pinfo;
  int rcode;

  if((rcode = CreateProcInfo(getpid(),&pinfo,opt))) {
    ShowError("Cannot create ProcInfo object because of error code %d\n",rcode);
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
    ShowError("Cannot create ProcInfo object because of error code %d\n",rcode);
    return -1;
  }

  for( ; *test_list ; ++test_list ) {
    void* address;
    if(ExplodeSymbolName(*test_list,ST_SIMPLE_TEST,mod,sym,buf,1024)) {
      ShowError("Test %s is not a valid name\n",*test_list);
      rcode = -1;
    } else {
      address = FindStrongSymbol(pinfo,buf);
      if(!address) {
        ShowError("Test %s is not found\n",*test_list);
        rcode = -1;
      } else {
        RunTest(address,stderr,mod,sym,TT_SIMPLE,NULL);
      }
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
    ShowError("Cannot create ProcInfo object because of error code %d\n",rcode);
    return -1;
  }

  PrepareTestPlan(pinfo,&tp,NULL);

  for( i = 0 ; i < tp.size ; ++i ) {
    ModuleEntry* me = tp.module + i;
    ColorFPrintf(stderr,NULL,"Green",NULL,"[ SUITE(%s)] ",GetTTName(me->tt));
    fprintf     (stderr,"%s\n",me->module);

    switch(me->tt) {
      case TT_FIXTURE:
        {
          if(me->setup) {
            ColorFPrintf(stderr,NULL,"Green",NULL,"[ SETUP   ]\n");
          }
          if(me->tear_down) {
            ColorFPrintf(stderr,NULL,"Green",NULL,"[ TEARDOWN]\n");
          }
        }
        // fallthrough
      case TT_SIMPLE:
        {
          for( size_t j = 0 ; j < me->arr.size ; ++j ) {
            TestEntry* t = me->arr.arr + j;
            if(t->address) {
              ColorFPrintf(stderr,NULL,"Green",NULL,"[ TEST    ] ");
              fprintf     (stderr,"%s.%s\n",me->module,t->name);
            }
          }
        }
        break;
    }
    ShowSeparator();
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

static void ShowHelp( const char* fmt , ... ) {
  static const char* kError =
    "  --list-tests:\n"
    "    Show all tests in different module\n"
    "\n"
    "  --module-list:\n"
    "    Specify a comma separated list to filter out specific tests to run\n"
    "\n"
    "  --test-list:\n"
    "    Specify a comma separated list to filter out specific tests to run\n"
    "\n"
    "  --option:   \n"
    "    Specify the searching option for test cases, the value can be *Main*\n"
    "    or *All*.*Main* means only search this executable program and *All* \n"
    "    means search all the shared object and the executable program\n";

  char buf[1024];
  va_list vl;
  va_start(vl,fmt);
  vsnprintf(buf,1024,fmt,vl);

  ColorFPrintf(stderr,NULL,"Red","Green",buf);
  fprintf(stderr,"\n\n");
  ColorFPrintf(stderr,NULL,"Blue",NULL ,kError);
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
      ShowHelp("cunitpp help:");
      goto fail;
    } else if(strcmp(argv[i],"--list-test") == 0) {
      if(opt->list != -1) {
        ShowHelp("--list-test duplicated");
        goto fail;
      }
      opt->list = 1;
    } else if(strcmp(argv[i],"--module-list") == 0) {
      if(opt->module_list != NULL) {
        ShowHelp("--module-list duplicated");
        goto fail;
      }
      if(i+1 == argc) {
        ShowHelp("expect a argument after --module-list");
        goto fail;
      }
      opt->module_list = ParseCommaList(argv[++i]);
    } else if(strcmp(argv[i],"--test-list") == 0) {
      if(opt->test_list != NULL) {
        ShowHelp("--test-list duplicated");
        goto fail;
      }
      if(i+1 == argc) {
        ShowHelp("expect a argument after --test-list");
        goto fail;
      }
      opt->test_list = ParseCommaList(argv[++i]);
    } else if(strcmp(argv[i],"--option") == 0) {
      if(i+1 == argc) {
        ShowHelp("expect a argument after --option");
        goto fail;
      }
      if(strcmp(argv[++i],"All") == 0) {
        opt->opt = PINFO_SRCH_ALL;
      } else {
        opt->opt = PINFO_SRCH_MAIN_ONLY;
      }
    } else {
      ShowHelp("unknown option %s",argv[i]);
      goto fail;
    }
  }

  if(opt->list == -1) opt->list = 0;
  return 0;
fail:
  DeleteCmdOption(opt);
  return -1;
}

void _CUnitAssert( const char* file , int line , const char* format , ... ) {
  char buf[1024];
  va_list vl;
  size_t nret;
  va_start(vl,format);
  nret = snprintf(buf,1024,"Assertion failed around %d:%s => ",line,file);
  fwrite  (buf,nret,1,stderr);
  vfprintf(stderr,format,vl);
  longjmp (kTestEnv,1);
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
