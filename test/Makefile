# Makefile
# Declaration of variables
CXX = g++
PROJECT_ROOT = ..
SUT_DIR = build_test
GTEST_ROOT = $(PROJECT_ROOT)/external/googletest/googletest
HEADER_DIRS = -I$(GTEST_ROOT)/include -I$(PROJECT_ROOT) -I$(PROJECT_ROOT)/external -I$(PROJECT_ROOT)/external/sputil/include
LDLIBS = -L$(GTEST_ROOT)/make -lgtest_main -lpthread -L$(PROJECT_ROOT)/$(SUT_DIR) -lsut -L$(PROJECT_ROOT)/external/sputil/build/malloc -lsputil
override CXXFLAGS += $(HEADER_DIRS) -enable-frame-pointers -std=gnu++14 -Wall -Wextra -ggdb -fno-strict-aliasing -Wpointer-arith -DSP_TEST
BUILD = build

# File names
EXEC = thetest
SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp, $(BUILD)/%.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

.PHONY: test all clean gtest sut

# all {{{
all: sut $(EXEC)
# }}}

# $(EXEC) {{{
$(EXEC): gtest $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(EXEC) $(LDLIBS)
# }}}

# To obtain object files
-include $(DEPENDS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -MMD -c $< -o $@
# }}}

# clean {{{
clean:
# clean test binaries
	rm -f $(EXEC) $(OBJECTS) $(DEPENDS)
# Clean sut binaries
	rm -f $(PROJECT_ROOT)/$(SUT_DIR)/*.a
	rm -f $(PROJECT_ROOT)/$(SUT_DIR)/*.d
	rm -f $(PROJECT_ROOT)/$(SUT_DIR)/*.o
# }}}

# gtest {{{
LIBGTEST_ROOT = $(GTEST_ROOT)/make
gtest:
	$(MAKE) -C $(GTEST_ROOT)/make all
	cp $(LIBGTEST_ROOT)/gtest_main.a $(LIBGTEST_ROOT)/libgtest_main.a
# }}}

# sut {{{
sut:
# Build sut binaries and put result in $(SUT_DIR) directory to differentiate the build for test binaries with regular binaries.
# Define the SP_TEST preprocessor macro
# The resulting static library will be called libsut
	$(MAKE) BUILD=$(SUT_DIR) LIB=libsut CXXFLAGS+=-DSP_TEST -C $(PROJECT_ROOT)
# }}}

# test {{{
test: all
	./$(EXEC)
# }}}
