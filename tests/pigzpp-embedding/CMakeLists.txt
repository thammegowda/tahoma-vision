cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(PigzppEmbeddingProbe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Parent shared-library policy" FORCE)
set(ZLIB_COMPAT OFF CACHE BOOL "Parent zlib compatibility policy" FORCE)
set(WITH_BENCHMARKS ON CACHE BOOL "Parent benchmark policy" FORCE)

set(PIGZPP_USE_ZLIB_NG ON CACHE BOOL "" FORCE)
set(PIGZPP_USE_ISAL OFF CACHE BOOL "" FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../libs/pigzpp"
    "${CMAKE_CURRENT_BINARY_DIR}/pigzpp"
    EXCLUDE_FROM_ALL)

if(TARGET pigzpp_exe)
    message(FATAL_ERROR "embedded pigzpp unexpectedly created its CLI target")
endif()
if(PIGZPP_BUILD_CLI OR PIGZPP_BUILD_PYTHON OR PIGZPP_BUILD_TESTS OR
   PIGZPP_BUILD_BENCHMARKS OR PIGZPP_INSTALL OR PIGZPP_LTO OR PIGZPP_STATIC)
    message(FATAL_ERROR "embedded pigzpp enabled a product-only option")
endif()
if(NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR "embedded pigzpp changed BUILD_SHARED_LIBS")
endif()
get_property(parent_zlib_compat CACHE ZLIB_COMPAT PROPERTY VALUE)
get_property(parent_benchmarks CACHE WITH_BENCHMARKS PROPERTY VALUE)
if(parent_zlib_compat OR NOT parent_benchmarks)
    message(FATAL_ERROR "embedded pigzpp changed parent zlib-ng cache options")
endif()
if(NOT CMAKE_CXX_STANDARD EQUAL 20)
    message(FATAL_ERROR "embedded pigzpp changed the parent C++ standard")
endif()
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    message(FATAL_ERROR "embedded pigzpp enabled parent-project IPO")
endif()

add_executable(pigzpp_embedding_probe main.cpp)
target_link_libraries(pigzpp_embedding_probe PRIVATE pigzpp_lib)
