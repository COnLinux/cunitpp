#ifndef PROC_INFO_H_
#define PROC_INFO_H_

#include <sys/types.h>

// A opaque structure represents the elf information inside of the current
// program. Used to do function lookup and address location during runtime
// for executing test functions.
struct ProcInfo;

// Error code used to indicate error while doing proc information creation
#define PINFO_NO_ERROR 0
#define PINFO_CANNOT_OPEN_MAPS -1

// Create a ProcInfo structure w.r.t the pid_t indicated in function
int CreateProcInfo( pid_t , struct ProcInfo** );

// Find a function with the input name and return its address , if not
// found, then returns a NULL. The function name isn't marked as *weak*
// symbol
void* FindFunction( struct ProcInfo* , const char* , const char* );

// Destroy ProcInfo object
void DeleteProcInfo( struct ProcInfo* );

#endif // PROC_INFO_H_
