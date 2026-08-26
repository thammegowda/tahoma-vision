cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(TahomaVision VERSION 0.1.0 LANGUAGES CXX C)

include(CMakePackageConfigHelpers)
include(ExternalProject)
include(GNUInstallDirs)
include(cmake/ValidateNeutralSources.cmake)

option(TAHOMA_VISION_BUILD_TESTS "Build Tahoma Vision tests" ${PROJECT_IS_TOP_LEVEL})
option(TAHOMA_VISION_BUILD_BENCHMARKS "Build Tahoma Vision benchmarks" OFF)
option(TAHOMA_VISION_BUILD_FUZZERS "Build Tahoma Vision fuzzers" OFF)
option(TAHOMA_VISION_INSTALL "Install Tahoma Vision" ${PROJECT_IS_TOP_LEVEL})
option(TAHOMA_VISION_SVG "Enable SVG rasterization" OFF)
option(TAHOMA_VISION_PDF "Enable PDF rendering through PDFium" OFF)

function(tahoma_vision_require_submodule marker)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${marker}")
        message(FATAL_ERROR
            "Required submodule is not initialized: ${marker}\n"
            "Run: git submodule update --init --recursive")
    endif()
endfunction()

tahoma_vision_require_submodule(libs/pigzpp/CMakeLists.txt)
tahoma_vision_require_submodule(libs/libjpeg-turbo/CMakeLists.txt)

# Bootstrap only: M0 replaces these system packages with the pinned sources
# above. Pigzpp needs a core-only embedding path; libjpeg-turbo is integrated
# through ExternalProject because its build rejects add_subdirectory().
set(TAHOMA_VISION_JPEG_PREFIX
    "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-install")
set(TAHOMA_VISION_JPEG_LIBRARY
    "${TAHOMA_VISION_JPEG_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libjpeg.a")
file(MAKE_DIRECTORY "${TAHOMA_VISION_JPEG_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
ExternalProject_Add(tahoma_vision_libjpeg
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/libjpeg-turbo"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-build"
    INSTALL_DIR "${TAHOMA_VISION_JPEG_PREFIX}"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
        "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>"
        "-DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
        "-DENABLE_SHARED=OFF"
        "-DENABLE_STATIC=ON"
        "-DWITH_TURBOJPEG=OFF"
        "-DWITH_TOOLS=OFF"
        "-DWITH_TESTS=OFF"
    BUILD_BYPRODUCTS "${TAHOMA_VISION_JPEG_LIBRARY}")
add_library(TahomaVision::Jpeg STATIC IMPORTED GLOBAL)
set_target_properties(TahomaVision::Jpeg PROPERTIES
    IMPORTED_LOCATION "${TAHOMA_VISION_JPEG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${TAHOMA_VISION_JPEG_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
set(PIGZPP_USE_ZLIB_NG ON CACHE BOOL "" FORCE)
set(PIGZPP_USE_ISAL OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/pigzpp ${CMAKE_CURRENT_BINARY_DIR}/pigzpp EXCLUDE_FROM_ALL)

add_library(tahoma_vision
    tahoma/vision/codec.cpp
    tahoma/vision/format.cpp
    tahoma/vision/image.cpp
    tahoma/vision/io.cpp
    tahoma/vision/jpeg_codec.cpp
    tahoma/vision/png_codec.cpp)
add_library(TahomaVision::Vision ALIAS tahoma_vision)

set_target_properties(tahoma_vision PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME Vision
    OUTPUT_NAME tahoma_vision
    POSITION_INDEPENDENT_CODE ON)
target_compile_features(tahoma_vision PUBLIC cxx_std_23)
target_include_directories(tahoma_vision PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(tahoma_vision PRIVATE TahomaVision::Jpeg pigzpp_lib)
add_dependencies(tahoma_vision tahoma_vision_libjpeg)

tahoma_vision_validate_neutral_sources(
    tahoma/vision/codec.h
    tahoma/vision/document_conversion.h
    tahoma/vision/format.h
    tahoma/vision/image.h
    tahoma/vision/io.h
    tahoma/vision/codec.cpp
    tahoma/vision/format.cpp
    tahoma/vision/image.cpp
    tahoma/vision/io.cpp
    tahoma/vision/jpeg_codec.cpp
    tahoma/vision/png_codec.cpp)

if(TAHOMA_VISION_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TAHOMA_VISION_BUILD_BENCHMARKS)
    message(FATAL_ERROR "TAHOMA_VISION_BUILD_BENCHMARKS is not implemented yet")
endif()

if(TAHOMA_VISION_BUILD_FUZZERS)
    message(FATAL_ERROR "TAHOMA_VISION_BUILD_FUZZERS is not implemented yet")
endif()

if(TAHOMA_VISION_SVG)
    message(FATAL_ERROR "TAHOMA_VISION_SVG is not implemented yet")
endif()

if(TAHOMA_VISION_PDF)
    include(cmake/pdfium.cmake)
    tahoma_setup_pdfium()
    target_sources(tahoma_vision PRIVATE tahoma/vision/pdf.cpp)
    target_link_libraries(tahoma_vision PRIVATE PDFium::PDFium)
    target_compile_definitions(tahoma_vision PUBLIC TAHOMA_VISION_HAS_PDF=1)
    tahoma_vision_validate_neutral_sources(
        tahoma/vision/pdf.h
        tahoma/vision/pdf.cpp)
endif()

if(TAHOMA_VISION_INSTALL)
    install(TARGETS tahoma_vision pigzpp_lib zopfli zlib-ng
        EXPORT TahomaVisionTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${TAHOMA_VISION_JPEG_LIBRARY}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RENAME libtahoma_vision_jpeg.a)
    install(DIRECTORY tahoma/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tahoma
        FILES_MATCHING PATTERN "*.h")
    install(EXPORT TahomaVisionTargets
        FILE TahomaVisionTargets.cmake
        NAMESPACE TahomaVision::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TahomaVision)

    configure_package_config_file(
        cmake/TahomaVisionConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/TahomaVisionConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TahomaVision)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/TahomaVisionConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/TahomaVisionConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/TahomaVisionConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TahomaVision)
endif()
