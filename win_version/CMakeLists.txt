cmake_minimum_required(VERSION 3.12)

if (DEFINED ENV{PRODUCT_VERSION})
    set(VERSION_STR $ENV{PRODUCT_VERSION})
else ()
    set(VERSION_STR "1.0.0.0")
endif ()

string(REPLACE "." ";" VERSION_LIST ${VERSION_STR})
list(GET VERSION_LIST 0 VERSION_MAJOR)
list(GET VERSION_LIST 1 VERSION_MINOR)
list(GET VERSION_LIST 2 VERSION_PATCH)
list(GET VERSION_LIST 3 VERSION_BUILD)
set(VERSION_API "${VERSION_MAJOR}.${VERSION_MINOR}")

include(generate_version_info.cmake)
include(version_config.cmake)

generate_version_info(
	versionFiles
	NAME ${SAMPLE_APP_NAME}
	BUNDLE ${version_conf_bundle}
	COMPANY_NAME ${version_conf_company_name}
	COMPANY_COPYRIGHT ${version_conf_company_copyright}
	FILE_DESCRIPTION ${version_conf_file_description}
	FILE_TYPE SHARED_LIBRARY
	COMMENTS ""
	MAJOR_VERSION ${VERSION_MAJOR}
	MINOR_VERSION ${VERSION_MINOR}
	PATCH_VERSION ${VERSION_PATCH}
	BUILD_NUMBER ${VERSION_BUILD}
)

add_library(${TARGET}_VERSION INTERFACE)
target_sources(${TARGET}_VERSION INTERFACE ${versionFiles})