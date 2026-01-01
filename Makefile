# Makefile for C++ protocol tester
# This tests the envelope.h and protocol.cpp files against real device packets

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -MMD -MP
INCLUDES = -I.

# Source files
PROTOCOL_SRC = components/cosori_kettle_ble/protocol.cpp
STATE_SRC = components/cosori_kettle_ble/cosori_kettle_state.cpp

TEST_CPP_SOURCES = tests/test_cpp.cpp $(PROTOCOL_SRC)
TEST_STATE_SOURCES = tests/test_state.cpp $(PROTOCOL_SRC) $(STATE_SRC)

TEST_CPP_OBJECTS = $(TEST_CPP_SOURCES:.cpp=.o)
TEST_STATE_OBJECTS = $(TEST_STATE_SOURCES:.cpp=.o)

DEPS = $(TEST_CPP_SOURCES:.cpp=.d) $(TEST_STATE_SOURCES:.cpp=.d)

.PHONY: all clean test run compile

all: tests/test_cpp tests/test_state

tests/test_cpp: $(TEST_CPP_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_CPP_OBJECTS)

tests/test_state: $(TEST_STATE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_STATE_OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Include dependency files
-include $(DEPS)

test: tests/test_cpp tests/test_state
	./tests/test_cpp
	./tests/test_state

run: test compile

compile:
	uv run esphome compile cosori-kettle.build.yaml

clean:
	rm -f tests/test_cpp tests/test_state $(TEST_CPP_OBJECTS) $(TEST_STATE_OBJECTS) $(DEPS)
