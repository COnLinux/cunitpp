#ifndef CUNITPP_H_
#define CUNITPP_H_
#include <string.h>

/**
 * CUNITPP encode meta information into a symbol or function name when you
 * using the test suit's specific TEST macro. The basic schema is as follow:
 *
 * @{Prefix}@{TYPE}@{MODULE}@{SEP}@{NAME}
 *
 * In our case the Prefix is __CUnitPP_ , Module is defined by the user via
 * macro , SEP is ____ and the Name is also defined by the user in macro.
 */

// The cunitpp's test case symbol prefix
#define CUNIT_SYMBOL_PREFIX "__CUnitPP_"

// The cunitpp's test case meta information
#define CUNIT_SIMPLE_TEST      'T'

// The cunitpp's fixture test meta information
#define CUNIT_FIXTURE_TEST     'F'
#define CUNIT_FIXTURE_SETUP    'S'
#define CUNIT_FIXTURE_TEARDOWN 'D'

// The cunitpp's module separator name
#define CUNIT_MODULE_SEPARATOR "____"

// The cunitpp's symbol definition macro
#define CUNIT_TEST_DEFINE_SCHEMA(TT,MODULE,NAME) __CUnitPP_##TT##MODULE##____##NAME

// The cunitpp's exported test case macro, mimic google test's TEST macro
#define TEST(MODULE,NAME)       void CUNIT_TEST_DEFINE_SCHEMA(T,MODULE,NAME)(void)

// The cunitpp's fixture test meta function macro
#define TEST_F(MODULE,NAME,PAR)     void  CUNIT_TEST_DEFINE_SCHEMA(F,MODULE,NAME)(PAR)
#define TEST_F_SETUP(MODULE)        void* CUNIT_TEST_DEFINE_SCHEMA(S,MODULE,S)(void )
#define TEST_F_TEARDOWN(MODULE,PAR) void  CUNIT_TEST_DEFINE_SCHEMA(D,MODULE,D)(PAR)

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
