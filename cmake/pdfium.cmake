include_guard(GLOBAL)

include(FetchContent)

function(tahoma_setup_pdfium)
    if(TARGET PDFium::PDFium)
        return()
    endif()

    set(TAHOMA_PDFIUM_VERSION "154.0.8021.0" CACHE STRING
        "Pinned PDFium binary package version")
    set(TAHOMA_PDFIUM_REVISION "8021" CACHE STRING
        "Pinned bblanchon/pdfium-binaries Chromium revision")
    set(TAHOMA_PDFIUM_ROOT "" CACHE PATH
        "Existing extracted PDFium package; skips download")
    set(TAHOMA_PDFIUM_URL "" CACHE STRING
        "Override PDFium binary archive URL")
    set(TAHOMA_PDFIUM_SHA256 "" CACHE STRING
        "Override PDFium binary archive SHA-256")
    option(TAHOMA_PDFIUM_ALLOW_DOWNLOAD
        "Allow CMake to download the pinned PDFium package" ON)

    if(TARGET pdfium)
        add_library(PDFium::PDFium ALIAS pdfium)
        return()
    endif()

    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _pdfium_processor)

    if(TAHOMA_PDFIUM_ROOT)
        set(_pdfium_source_dir "${TAHOMA_PDFIUM_ROOT}")
    else()
        if(NOT TAHOMA_PDFIUM_ALLOW_DOWNLOAD)
            message(FATAL_ERROR
                "PDFium download is disabled and TAHOMA_PDFIUM_ROOT is empty")
        endif()

        if(TAHOMA_PDFIUM_URL)
            if(NOT TAHOMA_PDFIUM_SHA256)
                message(FATAL_ERROR
                    "TAHOMA_PDFIUM_URL requires TAHOMA_PDFIUM_SHA256")
            endif()
            set(_pdfium_url "${TAHOMA_PDFIUM_URL}")
            set(_pdfium_sha256 "${TAHOMA_PDFIUM_SHA256}")
        else()
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                if(_pdfium_processor MATCHES "^(x86_64|amd64)$")
                    set(_pdfium_asset "pdfium-linux-x64.tgz")
                    set(_pdfium_sha256 "685f7930cd184ea22cd77afe707c1cf53b173d18118b6e16cb213c9277d7cdc3")
                elseif(_pdfium_processor MATCHES "^(aarch64|arm64)$")
                    set(_pdfium_asset "pdfium-linux-arm64.tgz")
                    set(_pdfium_sha256 "8da7615b210986a5b7187e3246ef0c315995583c68af6765b070d5dbb156ede6")
                endif()
            elseif(WIN32)
                if(_pdfium_processor MATCHES "^(x86_64|amd64)$")
                    set(_pdfium_asset "pdfium-win-x64.tgz")
                    set(_pdfium_sha256 "adac8ce034015427b5daa81f8eeddfcc8e84bc2a9f036f007890ff18bd4388c4")
                elseif(_pdfium_processor MATCHES "^(aarch64|arm64)$")
                    set(_pdfium_asset "pdfium-win-arm64.tgz")
                    set(_pdfium_sha256 "0783b32874953ef62799b784a5f3b163c97e745729cb981d242fa25099feca3e")
                endif()
            elseif(APPLE)
                if(_pdfium_processor MATCHES "^(x86_64|amd64)$")
                    set(_pdfium_asset "pdfium-mac-x64.tgz")
                    set(_pdfium_sha256 "0e770fda56c6726a08fab84c6306ad91eceb10589020ce3a407fad3ebcbe7bb2")
                elseif(_pdfium_processor MATCHES "^(aarch64|arm64)$")
                    set(_pdfium_asset "pdfium-mac-arm64.tgz")
                    set(_pdfium_sha256 "994600fa28974ce09a1c51c35039e808a6bc8ea3839050322c101ab229ad5c96")
                endif()
            endif()

            if(NOT _pdfium_asset)
                message(FATAL_ERROR
                    "Tahoma Vision has no pinned PDFium binary for "
                    "${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}; set "
                    "TAHOMA_PDFIUM_URL and TAHOMA_PDFIUM_SHA256 explicitly")
            endif()
            if(TAHOMA_PDFIUM_SHA256)
                set(_pdfium_sha256 "${TAHOMA_PDFIUM_SHA256}")
            endif()
            set(_pdfium_url
                "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium/${TAHOMA_PDFIUM_REVISION}/${_pdfium_asset}")
        endif()

        FetchContent_Declare(tahoma_pdfium
            URL "${_pdfium_url}"
            URL_HASH "SHA256=${_pdfium_sha256}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SOURCE_SUBDIR _no_cmake_project)
        FetchContent_MakeAvailable(tahoma_pdfium)
        set(_pdfium_source_dir "${tahoma_pdfium_SOURCE_DIR}")
    endif()

    unset(PDFium_INCLUDE_DIR CACHE)
    unset(PDFium_LIBRARY CACHE)
    unset(PDFium_IMPLIB CACHE)
    unset(PDFium_DIR CACHE)
    set(PDFium_DIR "${_pdfium_source_dir}" CACHE PATH
        "Tahoma Vision-managed PDFium package" FORCE)
    find_package(PDFium CONFIG REQUIRED
        PATHS "${_pdfium_source_dir}"
        NO_DEFAULT_PATH)
    if(NOT PDFium_VERSION VERSION_EQUAL TAHOMA_PDFIUM_VERSION)
        message(FATAL_ERROR
            "Tahoma Vision expected PDFium ${TAHOMA_PDFIUM_VERSION}, "
            "found ${PDFium_VERSION}")
    endif()
    if(NOT TARGET pdfium)
        message(FATAL_ERROR "PDFium package did not define target 'pdfium'")
    endif()
    set_property(TARGET pdfium PROPERTY IMPORTED_GLOBAL TRUE)
    add_library(PDFium::PDFium ALIAS pdfium)

    set(PDFium_VERSION "${PDFium_VERSION}" PARENT_SCOPE)
    message(STATUS
        "Tahoma Vision PDFium: ${PDFium_VERSION} from ${_pdfium_source_dir}")
endfunction()