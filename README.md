C unittest framework
===========================================================================

# Introduction

Mimic google test framework in C , user only needs to use MACRO to write instrumented
test function and the framework will figure out which function needs to be invoked as
unittest function. The most striking difference between cunitpp and other C test framework
is that user doesn't need to register function into framework but just a declarative
way to write unittest.

````
  TEST(Module1,Test1) {
    ASSERT_EQ(1,1);
    ASSERT_TRUE(MyCoolFunction());
  }

  int main( int argc , char* argv[] ) {
    return RunAllTests(argc,argv);
  }

````

Save the above code in a file called test.cc and compile it and link against libcunitpp
then run the output binary you will find your test function is executed. You don't need
to write main function or what's so ever, just need to use TEST macro to define your
function. The TEST macro has same meaning as google test framwork. The first argument is
used to indicate the test module , logically mutiple tests grouped together. The second
argument is used to specify the test name. So a full test name is `Module1.Test1` in the
above case. The function is automatically executed and test cases that is in the same
module will be groupped together to run and it shows a google test framework similar ouput.


# Internal

The project uses elf file export symbol to do runtime symbol detection. The TEST macro
will expand to a specific function symbol name that later on the framework can detect.
The framework will load its own binary via libelf along with /proc/pid/maps file to locate
all the recognized symbol along with its function address during the runtime. And then
when the symbol name matches certain test function name pattern it will be picked up for
execution.

The assertion internally is implemented via setjmp/longjmp to achieve C style exception


# Missing Feature

1. Mock
2. Benchmark
3. Signal Handling
