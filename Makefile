# Makefile
#http://www.puxan.com/web/howto-write-generic-makefiles/
# Declaration of variables
CXX = g++
CXXFLAGS = -enable-frame-pointers -std=c++17 -Wall -Wextra -Wpedantic
CXXFLAGS_DEBUG = $(CXXFLAGS) -ggdb
HEADER_DIRS = -Iexternal
LDLIBS = -lpthread
PREFIX = /usr/local

# File names
EXEC = main
LIB = lib$(EXEC)
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

#TODO dynamic link lib
#TODO header dependant change rebuild

# PHONY targets is not file backed targets
.PHONY: test all clean install uninstall bear

# all {{{
# The "all" target. runs by default since it the first target
all: ${EXEC}
# }}}

# $(EXEC) {{{
# depends on the targets for all the object files
$(EXEC): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(EXEC) $(LDLIBS)
# }}}

# object {{{
# The "object" file target
# An implicit conversion from a cpp file to a object file?
%.o: %.cpp
	# -c means to create an intermediary object file, rather than an executable
	$(CXX) -c $(CXXFLAGS) $(HEADER_DIRS) $< -o $@
# }}}

# clean {{{
clean:
	rm -f $(EXEC) $(OBJECTS) $(LIB).a $(LIB).so
	$(MAKE) -C test clean
# }}}

# test {{{
test:
	$(MAKE) -C test test
# }}}

# staticlib {{{
staticlib: $(OBJECTS)
	# 'r' means to insert with replacement
	# 'c' means to create a new archive
	# 's' means to write an index
	$(AR) rcs $(LIB).a $(OBJECTS)
# }}}

# install {{{
install: $(EXEC) staticlib
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	# mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp -f $(EXEC) $(DESTDIR)$(PREFIX)/bin
	cp -f $(LIB).a $(DESTDIR)$(PREFIX)/lib
	# gzip < $(EXEC).1 > $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1.gz
# }}}

# uninstall {{{
uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(EXEC)
	rm $(DESTDIR)$(PREFIX)/bin/$(lib).a
	# rm $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1.gz
# }}}

# bear {{{
# Creates compilation_database.json
bear: clean
	bear make
# }}}
