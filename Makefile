PWD               =$(shell pwd)
SOURCE            =$(shell find src/ -type f -name "*.c")
INCLUDE           =$(shell find src/ -type f -name "*.h")
OBJECT            =${SOURCE:.c=.o}
ASM               =$(shell find src/ -type f -name "*.S")
ASMOBJECT         =${ASM:.S=.o}
TEST              =$(shell find unittest/ -type f -name "*-test.c")
TESTOBJECT        =${TEST:.c=.t}
CC                = gcc
NASM              = nasm
#SANITIZER         =-fsanitize=address,undefined
RUNTIME_DEBUG     =-D_FORTIFY_SOURCE=2 -D_GLIBCC_ASSERTIONS
PROGNAME          =cunitpp
LIBNAME           =lib$(PROGNAME).a

CCFLAGS           =
LDFLAGS           = -lelf

# -------------------------------------------------------------------------------
#
# Flags for different types of build
#
# -------------------------------------------------------------------------------
RELEASE_FLAGS = -I$(PWD) -O3
RELEASE_LIBS  =


TEST_FLAGS        =-O0 -g3 -I./
TEST_LIBS         =

# -------------------------------------------------------------------------------
#
# Objects
#
# -------------------------------------------------------------------------------
src/%.o : src/%.c src/%.h
	$(CC) $(CCFLAGS) -c -o $@ $< $(LDFLAGS)

src/%.o : src/%.S
	$(NASM) -f elf64 -o $@ $<

# -------------------------------------------------------------------------------
#
# Test
#
# -------------------------------------------------------------------------------
unittest/%.t : unittest/%.c  $(ASMOBJECT) $(OBJECT) $(INCLUDE) $(SOURCE)
	$(CC) $(CCFLAGS) $(OBJECT) $(ASMOBJECT) -o $@ $< $(LDFLAGS)

test: CCFLAGS += $(TEST_FLAGS)
test: LDFLAGS += $(TEST_LIBS)

test: $(TESTOBJECT)

# -------------------------------------------------------------------------------
#
#  Release
#
# -------------------------------------------------------------------------------
release: CCFLAGS  += $(RELEASE_FLAGS)
release: LDFLAGS  += $(RELEASE_LIBS)

release: $(OBJECT) $(ASMOBJECT)
	ar rcs $(LIBNAME) $(OBJECT) $(ASMOBJECT)

all : release

clean:
	rm -rf $(OBJECT)
	rm -rf $(ASMOBJECT)
	rm -rf $(TESTOBJECT)
	rm -rf $(LIBNAME)
