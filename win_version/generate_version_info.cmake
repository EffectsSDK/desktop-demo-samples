cmake_minimum_required(VERSION 3.12)

# Function's arguments:
# NAME 
# BUNDLE - PRODUCT NAME
# COMPANY_NAME
# COMPANY_COPYRIGHT
# COMMENTS
# FILE_DESCRIPTION
# FILE_TYPE (EXECUTABLE|SHARED_LIBRARY)
# MAJOR_VERSION 
# MINOR_VERSION
# PATCH_VERSION
# BUILD_NUMBER

function(generate_version_info outputFiles)

set(options)
set(oneValueArgs
	NAME
	BUNDLE 
	COMPANY_NAME
	COMPANY_COPYRIGHT
	COMMENTS
	FILE_DESCRIPTION
	FILE_TYPE
	MAJOR_VERSION
	MINOR_VERSION
	PATCH_VERSION
	BUILD_NUMBER
)
set(multiValueArgs)
cmake_parse_arguments(VI "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

if ("${VI_FILE_TYPE}" STREQUAL "EXECUTABLE")
	set(fileExt exe)
	set(VI_FILE_TYPE VFT_APP)
elseif("${VI_FILE_TYPE}" STREQUAL "SHARED_LIBRARY")
	set(fileExt dll)
	set(VI_FILE_TYPE VFT_DLL)
endif()

set(VI_INTERNAL_NAME ${VI_NAME})
set(VI_ORIGINAL_FILENAME ${VI_NAME}.${fileExt})

set(rcFileName VersionResource.rc)
set(outputVersionFile ${CMAKE_BINARY_DIR}/${VI_NAME}/${rcFileName})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${rcFileName}.in ${outputVersionFile})
set(${outputFiles} ${outputVersionFile} PARENT_SCOPE)

endfunction()