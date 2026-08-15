cmake_minimum_required(VERSION 3.20)
project(yolo VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release, etc.)" FORCE)
endif()

# Compiler options and warnings
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Include current directory (where yolo_core.hpp is)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Threading library is required by yolo_app and gtest
find_package(Threads REQUIRED)

# Standard C++ unit testing with Googletest
option(BUILD_TESTS "Build unit tests" ON)

# The main YOLO executable
add_executable(yolo yolo_app.cpp yolo_core.hpp)
target_link_libraries(yolo PRIVATE Threads::Threads)

# Force unit and runtime tests to run after building yolo
if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gtest")
    add_dependencies(yolo run_unit_tests)
    add_custom_command(TARGET yolo POST_BUILD
        COMMAND $<TARGET_FILE:run_unit_tests>
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR} python3 ${CMAKE_CURRENT_SOURCE_DIR}/test_runner.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running C++ unit tests and Python runtime tests..."
        VERBATIM)
endif()

if(BUILD_TESTS)
    # Check if we have the gtest subdirectory
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gtest")
        # Prevent Googletest from overriding MSVC runtime library settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        
        # Add Googletest subproject
        add_subdirectory(gtest EXCLUDE_FROM_ALL)
        
        # Test runner executable
        add_executable(run_unit_tests test_yolo_app.cpp)
        target_compile_definitions(run_unit_tests PRIVATE TESTING)
        target_include_directories(run_unit_tests PRIVATE 
            gtest/googletest/include 
            gtest/googlemock/include
        )
        target_link_libraries(run_unit_tests PRIVATE 
            gtest 
            gtest_main 
            Threads::Threads
        )
        
        # Support running ctest
        enable_testing()
        add_test(NAME unit_tests COMMAND run_unit_tests)
    else()
        message(WARNING "gtest subdirectory not found. Skipping unit tests.")
    endif()
endif()
