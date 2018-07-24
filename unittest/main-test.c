#include "../src/cunitpp.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

TEST(Lexer,Test1) {
  ASSERT_EQ  (2,2);
}

TEST(Lexer,Test2) {
  ASSERT_EQ  (1,1);
}

TEST(Lexer,Name) {
  ASSERT_TRUE(1==1);
  ASSERT_STREQ("a","a");
}

TEST(Parser,Name) {
  ASSERT_TRUE(1 < 2);
}

TEST(Executer,Namexxx) {
  ASSERT_FALSE(2 > 3);
}

// ------------------------------------
// Test Fixture
// ------------------------------------

int g = 10;

TEST_F_SETUP(Fix) {
  return &g;
}

TEST_F_TEARDOWN(Fix,int* ctx) {
  assert(&g == ctx);
}

TEST_F(Fix,T1,int* ctx) {
  *ctx = 20;
  ASSERT_EQ(g,20);
  ASSERT_EQ(*ctx,20);
}
