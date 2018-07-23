#ifndef PROC_INFO_H_
#define PROC_INFO_H_

#include <stdio.h>
#include <sys/types.h>

// A opaque structure represents the elf information inside of the current
// program. Used to do function lookup and address location during runtime
// for executing test functions.
struct ProcInfo;

// Error code used to indicate error while doing proc information creation
#define PINFO_NO_ERROR          0
#define PINFO_CANNOT_OPEN_MAPS -1
#define PINFO_CANNOT_OPEN_ELF  -2
#define PINFO_ELF_ERROR        -3

// Option for searching the symbol
enum {
  // only search all the symbol in the executable program
  PINFO_SRCH_MAIN_ONLY,
  // search all loaded so object for symbol name as well as executable program
  PINFO_SRCH_ALL
};

// Create a ProcInfo structure w.r.t the pid_t indicated in function
int CreateProcInfo( pid_t , struct ProcInfo** , int );

// Dump the proc information into the stream
void DumpProcInfo ( const struct ProcInfo* , FILE* );

// Find a strong symbol function , should be only one
void* FindStrongSymbol( struct ProcInfo* , const char* );

// Callback function that is invoked by the foreach routine.
// Return status of SymbolCallback is as following
enum {
  PINFO_FOREACH_CONTINUE = 0,
  PINFO_FOREACH_BREAK,
  PINFO_FOREACH_STOP
};

// SymbolCallback function to be invoked by each symbol
typedef int (*BeginSymbolCallback) ( void* , const char* );
typedef int (*OnSymbolCallback   ) ( void* , void* , int );
typedef void(*EndSymbolCallback  ) ( void* );

// Foreach all the symbol in the symbol table for testing purpose
void  ForeachSymbol( struct ProcInfo* , BeginSymbolCallback , OnSymbolCallback  ,
                                                              EndSymbolCallback ,
                                                              void*             );

// Destroy ProcInfo object
void DeleteProcInfo( struct ProcInfo* );

#endif // PROC_INFO_H_
