DIR := ${CURDIR}

ifndef FF_ROOT 
# FF_ROOT		= ${DIR}
FF_ROOT		= ${HOME}/fastflow
endif

CXX			= g++ -std=c++17 
INCLUDES	= -I $(FF_ROOT) 
CXXFLAGS  	= -g # -DBLOCKING_MODE -DFF_BOUNDED_BUFFER

LDFLAGS 	= -pthread
OPTFLAGS	= -O3 -finline-functions -w -DNDEBUG

PRELDFLAGS 	= LD_PRELOAD=${DIR}/jemalloc/lib/libjemalloc.so.2

TARGETS 	=	build/boruvka_sequential \
				build/boruvka_thread \
				build/boruvka_ff 


.PHONY: all clean

.SUFFIXES: .cpp 

all: $(TARGETS)

build/boruvka_sequential: boruvka_sequential.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^ $(LDFLAGS)

build/boruvka_thread: boruvka_thread.cpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^ $(LDFLAGS)

build/boruvka_ff: boruvka_parallel_ff.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf build/*
