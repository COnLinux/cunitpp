cunitpp/cunit++ , a C unittest framework that works like google test
===========================================================================

# Introduction

This project provides an extreamly simple C unittest framework which looks almost
like google test framework. Most of the C unittest framework needs some sort of
test function registration . Basically you write a function to do your test but
later on in certain function you have to provide the function pointer to the test
framework to let it know which function is unittest function. In google test framework,
user just needs to use macro TEST to write function test and it is automatically
registered globally via C++'s static object's constructor function. But in C it is
nearly impossible to achieve. The reason why this is important is if you have many
unittest functions, the code that does the function pointer registeration becomes
really tedious to maintain and easy to forget to register certain function for testing.
It is nice to have a declarative way to write unittest function like google test.

This project provides you with a extreamly simple way to write C unittest function same
as google test. Example :

````
  TEST(Module1,Test1) {
    ASSERT_EQ(1,1);
    ASSERT_TRUE(MyCoolFunction());
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
4. Portable

In the future, all the above features will be implemented
