if(NOT DEFINED OUTPUT_HEADER)
    message(FATAL_ERROR "OUTPUT_HEADER not set")
endif()

if(NOT DEFINED BUILD_NUMBER_FILE)
    message(FATAL_ERROR "BUILD_NUMBER_FILE not set")
endif()

if(NOT DEFINED VERSION_MAJOR OR NOT DEFINED VERSION_MINOR OR NOT DEFINED VERSION_PATCH)
    message(FATAL_ERROR "VERSION_MAJOR/VERSION_MINOR/VERSION_PATCH not set")
endif()

get_filename_component(header_dir "${OUTPUT_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${header_dir}")

set(build_number 0)
if(EXISTS "${BUILD_NUMBER_FILE}")
    file(READ "${BUILD_NUMBER_FILE}" build_number_raw)
    string(STRIP "${build_number_raw}" build_number_raw)
    if(build_number_raw MATCHES "^[0-9]+$")
        set(build_number "${build_number_raw}")
    endif()
endif()

math(EXPR build_number "${build_number} + 1")

file(WRITE "${BUILD_NUMBER_FILE}" "${build_number}")

set(version_string "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${build_number}")
string(TIMESTAMP build_timestamp "%Y-%m-%d %H:%M:%S")

file(WRITE "${OUTPUT_HEADER}"
    "#pragma once\n"
    "#define PERSONALITIES_VERSION_MAJOR ${VERSION_MAJOR}\n"
    "#define PERSONALITIES_VERSION_MINOR ${VERSION_MINOR}\n"
    "#define PERSONALITIES_VERSION_PATCH ${VERSION_PATCH}\n"
    "#define PERSONALITIES_BUILD_NUMBER ${build_number}\n"
    "#define PERSONALITIES_VERSION_STRING \"${version_string}\"\n"
    "#define PERSONALITIES_BUILD_TIMESTAMP \"${build_timestamp}\"\n")
