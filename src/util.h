#ifndef UTIL_H_
#define UTIL_H_

#include <stdio.h>

// Return a heap allocated string marked by start and end pointer as input
const char* SubStr( const char* , const char* );

// A printer function to help print colorful text into the termainal/console.
// User can use special marker like @red { ... }  to mark a section of the text
// to be certain color.
void ColorFPrintf( FILE* , const char* , ... );

#endif // UTIL_H_
