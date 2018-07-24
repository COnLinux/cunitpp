#ifndef UTIL_H_
#define UTIL_H_

#include <stdio.h>

// Return a heap allocated string marked by start and end pointer as input
const char* SubStr( const char* , const char* );

// A printer function to help print colorful text into the termainal/console.
void ColorFPrintf( FILE* , const char* format , const char* fg ,
                                                const char* bg ,
                                                const char*    ,
                                                ...             );

// Get the array's size
#define ARRAY_SIZE(V) (sizeof((V))/sizeof((V)[0]))

#endif // UTIL_H_
