DIR := ${CURDIR}

ifndef FF_ROOT 
FF_ROOT		= ${DIR}/fastflow
endif

CXX			= g++ -std=c++17 
INCLUDES	= -I $(FF_ROOT) 
CXXFLAGS  	= -g # -DBLOCKING_MODE -DFF_BOUNDED_BUFFER

LDFLAGS 	= -pthread
OPTFLAGS	= -O3 -finline-functions -w -DNDEBUG

PRELDFLAGS 	= LD_PRELOAD=${DIR}/jemalloc/lib/libjemalloc.so.2

TARGETS 	=	build/boruvka_sequential \
				build/boruvka_parallel_thread \
				build/boruvka_fastflow


.PHONY: all clean test_par test_seq test_ff

.SUFFIXES: .cpp 

all: $(TARGETS)

build/boruvka_sequential: boruvka_sequential.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^ $(LDFLAGS)

build/boruvka_parallel_thread: boruvka_parallel_thread.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^ $(LDFLAGS)

build/boruvka_fastflow: boruvka_parallel_fastflow.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf build/*

test_seq:
	$(PRELDFLAGS) ./build/boruvka_sequential 10 20 "V1000_E10000.txt"

test_par:
	$(PRELDFLAGS) ./build/boruvka_parallel_thread 4 10 20 "V1000_E10000.txt"

test_ff:
	$(PRELDFLAGS) ./build/boruvka_fastflow 4 10 20 "V1000_E10000.txt"