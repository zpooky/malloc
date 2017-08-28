# Makefile
#http://www.puxan.com/web/howto-write-generic-makefiles/
# Declaration of variables
CC = g++
CC_FLAGS = -enable-frame-pointers -std=c++17 -ggdb -Wall -Wextra -Wpedantic
HEADER_DIRS = -Iexternal
LIBS = -lpthread
# File names
EXEC = main
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

# The "main" target. runs by default since it the first target
$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LIBS)

# To obtain object files
%.o: %.cpp
	$(CC) -c $(CC_FLAGS) $(HEADER_DIRS) $< -o $@

# The clean target
clean:
	rm -f $(EXEC) $(OBJECTS)

# The test target
# {{{
test:
	cd test && $(MAKE)
# }}}
