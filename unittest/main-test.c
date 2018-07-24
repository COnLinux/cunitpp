#include "../src/cunitpp.h"
#include <stdio.h>
#include <unistd.h>

TEST(Lexer,Test1) {
  printf("coo\n");
  return 0;
}

TEST(Lexer,Test2) {
  printf("cooXX\n");
  return 0;
}

TEST(Lexer,Name) {
  printf("Hello World\n");
  return 0;
}

TEST(Parser,Name) {
  printf("XXX\n");
  return 0;
}

TEST(Executer,Namexxx) {
  printf("xxxx\n");
  return 0;
}
