CPPFLAGS 	= -std=c++17 -pthread -Wall -Wzero-as-null-pointer-constant -O3 -g
LDFLAGS 	=
LDLIBS		=
AR			= ar
ARFLAGS		= rvs

TARGETS 	=	build/boruvka_parallel_thread 

INCLUDES	=	lib/dset.hpp \
				lib/utils.hpp \
				lib/components.hpp \
				lib/graph.hpp \
				lib/queue.hpp \
				lib/utimer.hpp 
					

.PHONY: all clean directories

# .SUFFIXES: .cpp .hpp

build/%: %.cpp 
	g++ $(CPPFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(LDLIBS)

# build/%: src/%.cpp 
# 	g++ $(CPPFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $< $(LDLIBS)

# build/%.o: %.cpp 
# 	g++ $(CFLAGS) $(INCLUDES) $(LDFLAGS) -c -o $@ $< $(LDLIBS) 

# build/%.o: src/%.cpp 
# 	g++ $(CFLAGS) $(INCLUDES) $(LDFLAGS) -c -o $@ $< $(LDLIBS) 


all: $(TARGETS)

build/boruvka_parallel_thread: boruvka_parallel_thread.cpp
	g++ $(CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)


clean:
	rm -rf build/*

directories:
	mkdir -p build