add_executable(tahoma_vision_test vision_test.cpp)
target_link_libraries(tahoma_vision_test PRIVATE TahomaVision::Vision)
target_compile_features(tahoma_vision_test PRIVATE cxx_std_23)

add_test(NAME tahoma_vision.contracts COMMAND tahoma_vision_test)

if(TAHOMA_VISION_PDF)
	add_executable(tahoma_vision_pdfium_test pdfium_test.cpp)
	target_link_libraries(tahoma_vision_pdfium_test PRIVATE PDFium::PDFium)
	add_test(NAME tahoma_vision.pdfium COMMAND tahoma_vision_pdfium_test)
endif()
