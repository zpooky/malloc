# Makefile
# Declaration of variables
CXX = g++
# using -std=gnu++17 because gtest uses functions only presisent in the gun dialect of c++
CXXFLAGS = -enable-frame-pointers -std=gnu++17 -Wall -Wextra -ggdb -fno-strict-aliasing
PROJECT_ROOT = ..
GTEST_ROOT = $(PROJECT_ROOT)/external/googletest/googletest
HEADER_DIRS = -I$(GTEST_ROOT)/include -I$(PROJECT_ROOT) -I$(PROJECT_ROOT)/external
LDLIBS = -L$(GTEST_ROOT)/make -lgtest_main -lpthread -L$(PROJECT_ROOT) -lsut

# File names
EXEC = thetest
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: test all clean gtest sut

# all {{{
all: sut $(EXEC)
# }}}
#
# $(EXEC) {{{
$(EXEC): gtest $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(EXEC) $(LDLIBS)
# }}}

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(HEADER_DIRS) $< -o $@
# }}}

# clean {{{
clean:
	rm -f $(EXEC) $(OBJECTS)
	rm -f $(PROJECT_ROOT)/libsut.a
# }}}

# gtest {{{
LIBGTEST_ROOT = $(GTEST_ROOT)/make
gtest:
	$(MAKE) -C $(GTEST_ROOT)/make all
	cp $(LIBGTEST_ROOT)/gtest_main.a $(LIBGTEST_ROOT)/libgtest_main.a
# }}}

# sut {{{
sut:
	$(MAKE) LIB=libsut CXXFLAGS+=-DSP_TEST -C $(PROJECT_ROOT) staticlib
# }}}

# test {{{
test: all
	./$(EXEC)
# }}}
