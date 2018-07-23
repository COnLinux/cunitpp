#ifndef CUNITPP_H_
#define CUNITPP_H_

// A symbol name that has *module's* module name and also the function name
// itself. It follows certain symbol rules to make sure we can read it from
// the elf table and execute them automatically.

// The cunitpp's test case symbol prefix
#define CUNIT_SYMBOL_PREFIX "__CUnitPP_"

// The cunitpp's module separator name
#define CUNIT_MODULE_SEPARATOR "_SS_"

// The cunitpp's single test case symbol name structure
#define CUNIT_TEST_NAME(MODULE,NAME) __CUnitPP_##MODULE##_SS_##NAME

// The cunitpp's exported test case macro, mimic google test's TEST macro
#define TEST(MODULE,NAME) int CUNIT_TEST_NAME(MODULE,NAME)(void)

// The assertion function to spew out error information into the output stream
// This function will not abort the program
void _CUnitAssert( const char* , int line , const char* , ... );

#define ASSERT_TRUE(COND)                                                                       \
  do {                                                                                          \
    if(!(COND)) { _CUnitAssert(__FILE__,__LINE__,"Expect %s to be True",#COND); return -1; }    \
  } while(0)

#define ASSERT_FALSE(COND)                                                                      \
  do {                                                                                          \
    if((COND))  { _CUnitAssert(__FILE__,__LINE__,"Expect %s to be False",#COND); return -1; }   \
  } while(0)

// Run all the tests that is registered based on symbol name
int RunAllTests( int , char** argv );

#endif // CUNITPP_H_
