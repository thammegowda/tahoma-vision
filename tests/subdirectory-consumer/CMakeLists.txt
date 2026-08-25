cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(TahomaVisionSubdirectoryConsumer LANGUAGES CXX)

set(TAHOMA_VISION_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TAHOMA_VISION_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    "${CMAKE_CURRENT_BINARY_DIR}/tahoma-vision"
    EXCLUDE_FROM_ALL)

add_executable(tahoma_vision_subdirectory_consumer main.cpp)
target_compile_features(tahoma_vision_subdirectory_consumer PRIVATE cxx_std_23)
target_link_libraries(
    tahoma_vision_subdirectory_consumer PRIVATE TahomaVision::Vision)
