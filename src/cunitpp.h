#ifndef CUNITPP_H_
#define CUNITPP_H_
#include <string.h>

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
#define TEST(MODULE,NAME) void CUNIT_TEST_NAME(MODULE,NAME)(void)

// The assertion function to spew out error information into the output stream
// This function will not abort the program
void _CUnitAssert( const char* , int line , const char* , ... );

#define _ASSERT_BINARY(LHS,RHS,OP)                         \
  ( ((LHS) OP (RHS)) ? (void)(0) : _CUnitAssert(__FILE__ , \
                                                __LINE__ , \
    "Comparison `%s %s %s` failed\n", #LHS , #OP , #RHS) )

#define _ASSERT_STR_BINARY(LHS,RHS,OP)                         \
  ((strcmp((LHS),(RHS)) OP 0) ? (void)(0) : _CUnitAssert(    \
    __FILE__,__LINE__,"String comparison `%s %s %s` failed\n", \
    (LHS),#OP,(RHS)))

#define ASSERT_EQ(LHS,RHS) _ASSERT_BINARY(LHS,RHS,==)
#define ASSERT_NE(LHS,RHS) _ASSERT_BINARY(LHS,RHS,!=)
#define ASSERT_LT(LHS,RHS) _ASSERT_BINARY(LHS,RHS,< )
#define ASSERT_LE(LHS,RHS) _ASSERT_BINARY(LHS,RHS,<=)
#define ASSERT_GT(LHS,RHS) _ASSERT_BINARY(LHS,RHS,> )
#define ASSERT_GE(LHS,RHS) _ASSERT_BINARY(LHS,RHS,>=)

#define ASSERT_STREQ(LHS,RHS) _ASSERT_STR_BINARY(LHS,RHS,==)
#define ASSERT_STRNE(LHS,RHS) _ASSERT_STR_BINARY(LHS,RHS,!=)
#define ASSERT_STRLT(LHS,RHS) _ASSERT_STR_BINARY(LHS,RHS,< )
#define ASSERT_STRLE(LHS,RHS) _ASSERT_STR_BINARY(LHS,RHS,<=)
#define ASSERT_STRGT(LHS,RHS) _ASSERT_STR_BINARY(LHS,RHS,> )
#define ASSERT_STRGE(LHS,RHS) _ASSERT_STR_BINARY(LHS,RHS,>=)

#define ASSERT_TRUE(COND)                                  \
  ((COND) ? (void)(0) : _CUnitAssert(__FILE__,__LINE__,    \
    "Expression `%s` expected to be True\n", #COND))

#define ASSERT_FALSE(COND) \
  ((!(COND)) ? (void)(0) : _CUnitAssert(__FILE__,__LINE__, \
    "Expression `%s` expected to be False\n",#COND))

// Run all the tests that is registered based on symbol name
int RunAllTests( int , char** argv );

#endif // CUNITPP_H_
