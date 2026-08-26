add_executable(tahoma_vision_test vision_test.cpp)
target_link_libraries(
	tahoma_vision_test PRIVATE TahomaVision::Vision TahomaVision::Jpeg)
target_compile_features(tahoma_vision_test PRIVATE cxx_std_23)
target_compile_definitions(tahoma_vision_test PRIVATE
	TAHOMA_VISION_TEST_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_test(NAME tahoma_vision.contracts COMMAND tahoma_vision_test)

if(TAHOMA_VISION_PDF)
	add_executable(tahoma_vision_pdfium_test pdfium_test.cpp)
	target_link_libraries(
		tahoma_vision_pdfium_test PRIVATE TahomaVision::Vision)
	target_compile_definitions(tahoma_vision_pdfium_test PRIVATE
		TAHOMA_VISION_TEST_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
	add_test(NAME tahoma_vision.pdfium COMMAND tahoma_vision_pdfium_test)

	add_executable(tahoma_vision_resource_generator EXCLUDE_FROM_ALL
		resources/generate.cpp)
	target_link_libraries(
		tahoma_vision_resource_generator PRIVATE TahomaVision::Vision)
	add_custom_target(tahoma_vision_generate_test_resources
		COMMAND $<TARGET_FILE:tahoma_vision_resource_generator>
			"${CMAKE_CURRENT_SOURCE_DIR}/resources"
		DEPENDS tahoma_vision_resource_generator
		VERBATIM)
endif()
