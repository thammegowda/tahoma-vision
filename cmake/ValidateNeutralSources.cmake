function(tahoma_vision_validate_neutral_sources)
    set(_forbidden_include
        "#[ \t]*include[ \t]*[<\"](torch|ATen|c10|cuda|hip|onnxruntime|xnnpack|tahoma/(tensor|infer))")
    foreach(_source IN LISTS ARGN)
        if(NOT IS_ABSOLUTE "${_source}")
            set(_source "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
        endif()
        file(STRINGS "${_source}" _forbidden_lines REGEX "${_forbidden_include}")
        if(_forbidden_lines)
            message(FATAL_ERROR
                "Neutral Tahoma Vision source has a forbidden dependency in ${_source}:\n"
                "${_forbidden_lines}")
        endif()
    endforeach()
endfunction()
