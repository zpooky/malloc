# Makefile
#http://www.puxan.com/web/howto-write-generic-makefiles/
# Declaration of variables
CXX = g++
HEADER_DIRS = -Iexternal
# ovrrides makes it possible to externaly append extra flags
override CXXFLAGS += $(HEADER_DIRS) -enable-frame-pointers -std=c++14 -Wall -Wextra -Wpedantic -Wpointer-arith -ggdb -fno-strict-aliasing -Wconversion -Wshadow

LDFLAGS =
LDLIBS = -lpthread
PREFIX = /usr/local
BUILD = build

# File names
EXEC = main
LIB = lib$(EXEC)
SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp, $(BUILD)/%.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

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
-include $(DEPENDS)

$(BUILD)/%.o: %.cpp
	mkdir -p $(BUILD)
	# -c means to create an intermediary object file, rather than an executable
	# -MMD means to create *object*.d depend file with its depending cpp & h files
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -MMD -c $< -o $@
# }}}

# clean {{{
clean:
	rm -f $(OBJECTS)
	rm -f $(DEPENDS)
	rm -f $(EXEC) $(BUILD)/$(LIB).a $(BUILD)/$(LIB).so
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
	$(AR) rcs $(BUILD)/$(LIB).a $(OBJECTS)
# }}}

# bear {{{
# Creates compilation_database.json
bear: clean
	bear make
# }}}

