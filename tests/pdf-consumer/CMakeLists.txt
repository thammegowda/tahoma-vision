cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(TahomaVisionPdfConsumer LANGUAGES CXX)

find_package(TahomaVision 0.1 REQUIRED CONFIG)

add_executable(tahoma_vision_pdf_consumer main.cpp)
target_compile_features(tahoma_vision_pdf_consumer PRIVATE cxx_std_23)
target_link_libraries(tahoma_vision_pdf_consumer PRIVATE TahomaVision::Vision)