# Compiler and Flags
CXX=c++
CXXFLAGS=-std=c++26 -Wall -Wextra -I./
TARGET=yolo

# Main Application Files
SRCS=yolo_app.cpp
OBJS=$(SRCS:.cpp=.o)

# Test Application Files
TEST_SRCS=test_yolo_app.cpp
TEST_TARGET=run_unit_tests
RUNTIME_TEST_SCRIPT=test_runner.py

# Google Test Configuration
# IMPORTANT: Set this path to your Google Test installation directory.
GTEST_DIR ?= gtest/googletest
GTEST_INCLUDE=-isystem $(GTEST_DIR)/include -isystem gtest/googlemock/include
GTEST_LIB=-Lgtest/build_gtest/lib -lgtest -lgtest_main -pthread

# The default 'all' target now depends on the 'runtime-test' target.
# This ensures all tests pass before the build is considered successful.
all: clean runtime-test

# The main application target depends on its object files and the unit tests passing.
$(TARGET): test yolo_app.o 
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

yolo_app.o: yolo_app.cpp yolo_core.hpp
	$(CXX) $(CXXFLAGS) -c yolo_app.cpp

# Target to run the C++ unit tests. It depends on the test executable being built.
# The build will halt here if the test executable returns a non-zero exit code.
test: $(TEST_TARGET) 
	@echo "--- Running C++ Unit Tests ---"
	./$(TEST_TARGET) || (echo "Unit tests failed, cleaning up."; $(MAKE) clean; exit 1)

# Target to build the test executable.
$(TEST_TARGET): $(TEST_SRCS) yolo_app.cpp yolo_core.hpp
	$(CXX) $(CXXFLAGS) -DTESTING $(GTEST_INCLUDE) -o $@ $(TEST_SRCS) $(GTEST_LIB)

# Target to run the Python runtime tests. It depends on the main application being built.
# If the python script fails, it triggers a cleanup.
runtime-test: $(TARGET)
	@echo "--- Running Python Runtime Tests ---"
	python3 $(RUNTIME_TEST_SCRIPT) || (echo "Runtime tests failed, cleaning up."; $(MAKE) clean; exit 1)
	@echo "--- All tests passed. Build successful. ---"

.PHONY: all clean install test runtime-test
clean:
	@echo "--- Cleaning up build artifacts ---"
	rm -f *.o $(TARGET) $(TEST_TARGET) yolo.log test_log.txt

install:
	$(MAKE) all
	#cp $(TARGET) /bin 
	#cp $(TARGET) /usr/bin 
	#cp $(TARGET) c:\yolo 
	
