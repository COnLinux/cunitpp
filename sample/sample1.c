#include "../src/cunitpp.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

/** --------------------------------------*
 * Simple Test                            |
 * ---------------------------------------*/

TEST(Suite1,TestTrueFalse) {
  ASSERT_TRUE(1);
  ASSERT_FALSE(0);
  ASSERT_FALSE(1 != 1);
  ASSERT_TRUE (1 == 1);
}

TEST(Suite1,TestCompare) {
  ASSERT_EQ(1,1);
  ASSERT_NE(1,2);
  ASSERT_LT(1,10);
  ASSERT_LE(1,1);
  ASSERT_GT(10,1);
  ASSERT_GE(1,1);
}

TEST(Suite1,TestStrCompare) {
  ASSERT_STREQ("a","a");
  ASSERT_STRNE("a","b");
  ASSERT_STRLT("a","b");
  ASSERT_STRLE("a","a");
  ASSERT_STRGT("b","a");
  ASSERT_STRGE("b","b");
}

TEST(NegativeSuite1,T1) {
  ASSERT_TRUE(0);
}

TEST(NegativeSuite1,T2) {
  ASSERT_FALSE(1);
}

TEST(NegativeSuite1,T3) {
  ASSERT_EQ(1,0);
}

TEST(NegativeSuite1,T4) {
  ASSERT_NE(1,1);
}

TEST(NegativeSuite1,T5) {
  ASSERT_STRNE("a","a");
}
