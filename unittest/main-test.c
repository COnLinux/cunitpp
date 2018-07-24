#include "../src/cunitpp.h"
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
