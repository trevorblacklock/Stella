#--------------------------------------------------------------------------------#
# 1. GENERAL                                                                     #
#--------------------------------------------------------------------------------#

# 1.1 Find the OS of the system
# Currently only support Windows and Linux 64-bit
UNAME := $(shell uname -s)

# Linux based OS
ifeq ($(UNAME), Linux)
OS := $(shell uname -o)
endif

# Windows based OS
ifeq ($(OS), Windows_NT)
windows = yes
endif

# 1.2 File Naming w/ major and minor version
NAME = Stella
MAJOR = 1
MINOR = 0

# 1.3 Source code directory and files
ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SRCS := bitboard.cpp hashtable.cpp history.cpp main.cpp misc.cpp movegen.cpp \
				evaluate.cpp position.cpp pv.cpp search.cpp timing.cpp uci.cpp \
				perft.cpp
OBJS := $(SRCS:.cpp=.o)

# Default neural network naming
NET_FILE := $(ROOT)/Stella-00000000.nn

#--------------------------------------------------------------------------------#
# 2. CONFIGURATION                                                               #
#--------------------------------------------------------------------------------#
# Option            | Flag         | Description                                 #
#--------------------------------------------------------------------------------#
# debug = yes/no    | -DNDEBUG     | Enable/disable debugging mode               #
# optimize = yes/no | -03          | Enable compiler optimizations               #
# popcnt = yes/no   | -DUSE_POPCNT | Use popcnt asm-instruction                  #
# pext = yes/no     | -DUSE_PEXT   | Use pext asm-instruction                    #
# sse = yes/no      | -msse        | Use Intel Streaming SIMD extensions         #
# sse2 = yes/no     | -msse2       | Use Intel Streaming SIMD extensions 2       #
# sse3 = yes/no     | -msse3       | Use Intel Streaming SIMD extensions 3       #
# sse41 = yes/no    | -msse4.1     | Use Intel Streaming SIMD extensions 4.1     #
# sse42 = yes/no    | -msse4.2     | Use Intel Streaming SIMD extensions 4.2     #
# avx = yes/no      | -mavx        | Use Intel Advanced Vector Instsructions     #
# avx2 = yes/no     | -mavx2       | Use Intel Advanced Vector Instsructions 2   #
# avx512 = yes/no   | -mavx512bw   | Use Intel Advanced Vector Instsructions 512 #
#--------------------------------------------------------------------------------#

# 2.1 Compilation Default Options
debug = no
optimize = yes
popcnt = no
pext = no
sse = no
sse2 = no
sse3 = no
sse41 = no
sse42 = no
avx = no
avx2 = no
avx512 = no

# 2.2 Compiler Setup
# Currently only supporting g++
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wcast-qual -fno-exceptions -pedantic -Wextra -Wshadow -Wmissing-declarations \
					 -DMINOR_VERSION=$(MINOR) -DMAJOR_VERSION=$(MAJOR) -DNET_FILE=\"$(NET_FILE)\" \
					 -lpthread -static $(EXTRA_FLAGS)

# 2.3 Flags for compiling with PGO enabled
PRE_PGO_FLAGS = '-fprofile-generate -lgcov'
POST_PGO_FLAGS = '-fprofile-use -fno-peel-loops -fno-tracer -lgcov'

# 2.4.1 Arch specific flags
ifeq ($(ARCH),)
ARCH=native
CXXFLAGS += -march=native
endif

# 2.4.2 If arch is not defined then use Stockfish script to find native arch for the computer
ifeq ($(ARCH), native)
override ARCH := $(shell $(ROOT)/../scripts/get_native_properties.sh)
endif

# 2.4.3 Use the current ARCH variable to switch flags
ifeq ($(findstring x86-64, $(ARCH)), x86-64)
sse = yes
sse2 = yes
endif

ifeq ($(findstring -sse3, $(ARCH)), -sse3)
sse = yes
sse2 = yes
sse3 = yes
endif

ifeq ($(findstring -popcnt, $(ARCH)), -popcnt)
popcnt = yes
endif

ifeq ($(findstring -sse41, $(ARCH)), -sse41)
sse = yes
sse2 = yes
sse3 = yes
sse41 = yes
endif

ifeq ($(findstring -sse42, $(ARCH)), -sse42)
sse = yes
sse2 = yes
sse3 = yes
sse41 = yes
sse42 = yes
endif

ifeq ($(findstring -bmi2, $(ARCH)), -bmi2)
popcnt = yes
sse = yes
sse2 = yes
sse3 = yes
sse41 = yes
sse42 = yes
avx2 = yes
pext = yes
endif

ifeq ($(findstring -avx, $(ARCH)), -avx)
popcnt = yes
sse = yes
sse2 = yes
sse3 = yes
sse41 = yes
sse42 = yes
avx = yes
endif

ifeq ($(findstring -avx2, $(ARCH)), -avx2)
popcnt = yes
sse = yes
sse2 = yes
sse3 = yes
sse41 = yes
sse42 = yes
avx = yes
avx2 = yes
endif

ifeq ($(findstring -avx512, $(ARCH)), -avx512)
popcnt = yes
sse = yes
sse2 = yes
sse3 = yes
sse41 = yes
sse42 = yes
avx = yes
avx2 = yes
pext = yes
avx512 = yes
endif

#--------------------------------------------------------------------------------#
# 3. Low-Level Configuration                                                     #
#--------------------------------------------------------------------------------#

# 3.1 Debugging flags
ifeq ($(debug), yes)
CXXFLAGS += -g
else
CXXFLAGS += -DNDEBUG
endif

# 3.2 Optimization flags
ifeq ($(optimize), yes)
CXXFLAGS += -O3 -funroll-loops
endif

# 3.3 SIMD and vector instruction flags
ifeq ($(popcnt), yes)
CXXFLAGS += -mpopcnt -DUSE_POPCNT
endif

ifeq ($(pext), yes)
CXXFLAGS += -mbmi2 -DUSE_PEXT
endif

ifeq ($(avx512), yes)
CXXFLAGS += -mavx512f -mavx512bw -mavx512dq
endif

ifeq ($(avx2), yes)
CXXFLAGS += -mavx2 -mbmi
endif

ifeq ($(avx), yes)
CXXFLAGS += -mavx
endif

ifeq ($(sse42), yes)
CXXFLAGS += -msse4.2
endif

ifeq ($(sse41), yes)
CXXFLAGS += -msse4.1
endif

ifeq ($(sse3), yes)
CXXFLAGS += -msse3
endif

ifeq ($(sse2), yes)
CXXFLAGS += -msse2
endif

ifeq ($(sse), yes)
CXXFLAGS += -msse
endif

# 3.4 Link time optimiztion
ifeq ($(optimize), yes)
ifeq ($(debug), no)
CXXFLAGS += -flto -flto-partition=one
endif
endif

# 3.5 Executable naming
PREFIX = linux
SUFFIX =
ifeq ($(windows), yes)
override PREFIX = windows
override SUFFIX = .exe
endif

EXE_DIR := $(ROOT)/../bin
EXE := $(EXE_DIR)/$(NAME)-$(MAJOR).$(MINOR)-$(PREFIX)-$(ARCH)$(SUFFIX)
EXE_PGO := $(abspath $(EXE))

#--------------------------------------------------------------------------------#
# 4. Build Targets                                                               #
#--------------------------------------------------------------------------------#

help:
	@echo ""
	@echo "To compile Stella, type:"
	@echo ""
	@echo "make -j target [ARCH=arch]"
	@echo ""
	@echo "Supported targets:"
	@echo "help                         > Displays information on build targets"
	@echo "pgo                          > Creates an executable with profile guided optimization"
	@echo "build                        > Standard build"
	@echo "net                          > Requires an internet connection; downloads the latest neural net"
	@echo "clean                        > Cleans up the directory of build files"
	@echo ""
	@echo "Supported arch's:"
	@echo "native                       > Used by default and will auto select the best arch"
	@echo "x86-64-avx512                > 64-bit with avx512 support"
	@echo "x86-64-avx2                  > 64-bit with avx2 support"
	@echo "x86-64-avx                   > 64-bit with avx support"
	@echo "x86-64-bmi2                  > 64-bit with bmi2 support"
	@echo "x86-64-sse42-popcnt          > 64-bit with sse4.2 and popcnt support"
	@echo "x86-64-sse41-popcnt          > 64-bit with sse4.1 and popcnt support"
	@echo "x86-64-sse3-popcnt           > 64-bit with sse3 and popcnt support"
	@echo "x86-64-sse3                  > 64-bit with sse3 support"
	@echo "x86-64                       > 64-bit with sse2 support"
	@echo ""
	@echo "Examples:"
	@echo "make -j pgo                  > Creates the fastest executable for your system"
	@echo "make -j pgo ARCH=x86-64-avx2 > Creates an executable with avx2"

build: net
	@mkdir -p $(EXE_DIR)
	$(MAKE) ARCH=$(ARCH) all

pgo : net obj-clean pgo-clean
	@echo ""
	@echo "Step 1/4: Building initial executable.."
	@mkdir -p $(EXE_DIR)
	$(MAKE) ARCH=$(ARCH) EXTRA_FLAGS=$(PRE_PGO_FLAGS) all
	@echo ""
	@echo "Step 2/4: Running benchmark.."
	$(EXE_PGO) "go perft 6" "exit" > bench.out 2>&1
	tail -n 1 bench.out
	@echo  ""
	@echo "Step 3/4: Building optimized executable.."
	$(MAKE) ARCH=$(ARCH) obj-clean
	$(MAKE) ARCH=$(ARCH) EXTRA_FLAGS=$(POST_PGO_FLAGS) all
	@echo ""
	@echo "Step 4/4: Cleaning up.."
	$(MAKE) ARCH=$(ARCH) pgo-clean

.PHONY: help build pgo clean obj-clean pgo-clean FORCE

obj-clean:
	@rm -f *.o nn/*.o

pgo-clean:
	@rm -f *.gcda *.gcno nn/*.gcda nn/*.gcno bench.out

clean: obj-clean pgo-clean
	@echo "Cleaning directory.."

default:
	help

all: $(OBJS) $(EXE)

$(EXE): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(CXXFLAGS) -flto

$(OBJS): %.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

net:

misc.o: FORCE
FORCE:
