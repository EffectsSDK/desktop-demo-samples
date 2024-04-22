cmake_minimum_required(VERSION 3.16)

set(TARGET Sample)

project(${TARGET} CXX)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	set(OS_WINDOWS YES)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(OS_LINUX YES)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	set(OS_MACOS YES)
endif()

set(EFFECTS_SDK_PATH "" CACHE PATH "Path to Video Effects SDK") 

if(NOT EFFECTS_SDK_PATH)
	message(FATAL_ERROR  "EFFECTS_SDK_PATH is not set!")
endif()

if(NOT EXISTS ${EFFECTS_SDK_PATH})
	message(FATAL_ERROR "The path to Video Effects SDK '${EFFECTS_SDK_PATH}' does not exist!")
endif()

if(OS_WINDOWS)
	option(WINDEPLOYQT "Path to windeployqt.exe")

	if ((CMAKE_SIZEOF_VOID_P EQUAL 8) AND EXISTS "${EFFECTS_SDK_PATH}/bin_64")
		set(EFFECTS_SDK_BIN_PATH_DEFAULT "${EFFECTS_SDK_PATH}/bin_64")
	elseif((CMAKE_SIZEOF_VOID_P EQUAL 4) AND EXISTS "${EFFECTS_SDK_PATH}/bin_32")
		set(EFFECTS_SDK_BIN_PATH_DEFAULT "${EFFECTS_SDK_PATH}/bin_32")
	endif()
	set(EFFECTS_SDK_BIN_PATH ${EFFECTS_SDK_BIN_PATH_DEFAULT} CACHE PATH "Path to a dir with binaries of Video Effects SDK")
endif()

if (OS_LINUX)
	option(LINUXDEPLOY "Path to linuxdeploy bin for appdir generation")
endif()

set(SAMPLE_APP_NAME "Video Effects SDK")
if (DEFINED ENV{PRODUCT_VERSION})
	set(VERSION_STR $ENV{PRODUCT_VERSION})
else ()
	set(VERSION_STR "1.0.0.0")
endif ()

set(TSVB_BUNDLE_NAME "Video Effects SDK")
set(TSVB_COPYRIGHT "(c) 2021-2024 TomskSoft")
set(TSVB_COMPANY "TomskSoft")

if(OS_LINUX)
	string(REPLACE " " "" SAMPLE_APP_BIN_NAME "${SAMPLE_APP_NAME}")
else()
	set(SAMPLE_APP_BIN_NAME "${SAMPLE_APP_NAME}")
endif()

set(CMAKE_AUTOMOC ON)

if(OS_WINDOWS)
	set(QT_FRAMEWORK Qt6)
	set(QT_FRAMEWORK_DIR ${Qt6_DIR})
else()
	set(QT_FRAMEWORK Qt5)
	set(QT_FRAMEWORK_DIR ${Qt5_DIR})
endif()

list(APPEND CMAKE_PREFIX_PATH ${QT_FRAMEWORK_DIR})

find_package(${QT_FRAMEWORK} COMPONENTS Widgets Multimedia REQUIRED)
find_package(OpenCV REQUIRED)

if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
set(IS_X64_BUILD true)
elseif("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
set(IS_X86_BUILD true)
endif()

if(OS_WINDOWS)
	add_executable(${TARGET} WIN32)
elseif(OS_LINUX)
	add_executable(${TARGET})
elseif(OS_MACOS)
	add_executable(${TARGET} MACOSX_BUNDLE)
endif()

target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR} ${EFFECTS_SDK_PATH}/include/
	${CMAKE_CURRENT_BINARY_DIR})

set(H_SOURCES
	${H_SOURCES}
	${CMAKE_CURRENT_SOURCE_DIR}/frame_view.h
	${CMAKE_CURRENT_SOURCE_DIR}/media_utils.h
	${CMAKE_CURRENT_SOURCE_DIR}/metrics.h
	${CMAKE_CURRENT_SOURCE_DIR}/metrics_view.h
	${CMAKE_CURRENT_SOURCE_DIR}/pipeline.h
	${CMAKE_CURRENT_SOURCE_DIR}/sample.h
	${CMAKE_CURRENT_SOURCE_DIR}/sample_ui.h
	${CMAKE_CURRENT_SOURCE_DIR}/sdk_library_handler.h
	${CMAKE_CURRENT_SOURCE_DIR}/video_filter.h
)

set (CPP_SOURCES
	${CPP_SOURCES}
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/frame_view.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/media_utils.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/metrics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/metrics_view.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pipeline.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sample.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sample_ui.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/video_filter.cpp
)

if(OS_WINDOWS)
	set (CPP_SOURCES
		${CPP_SOURCES}
		${CMAKE_CURRENT_SOURCE_DIR}/sdk_library_handler_win.cpp
	)
elseif(OS_LINUX OR OS_MACOS)
	set (CPP_SOURCES
		${CPP_SOURCES}
		${CMAKE_CURRENT_SOURCE_DIR}/sdk_library_handler_unix.cpp
	)
endif()

if(OS_MACOS)
	set (H_SOURCES
		${H_SOURCES}
		${CMAKE_CURRENT_SOURCE_DIR}/camera_access_authorization.h
	)
	set (CPP_SOURCES
		${CPP_SOURCES}
		${CMAKE_CURRENT_SOURCE_DIR}/camera_access_authorization.mm
	)
endif()

target_sources(${TARGET}
	PRIVATE
	${CPP_SOURCES}
	${H_SOURCES}
)

target_link_libraries(${TARGET} PRIVATE
	${QT_FRAMEWORK}::Widgets
	${QT_FRAMEWORK}::Multimedia
)

if(TARGET opencv::opencv_videoio)
	target_link_libraries(${TARGET} PRIVATE
		opencv::opencv_videoio
	)
else()
	target_link_libraries(${TARGET} PRIVATE
		${OpenCV_LIBS}
	)	
	target_include_directories(${TARGET} PRIVATE
		${OpenCV_INCLUDE_DIRS}
	)
endif()

if(OS_LINUX)
	target_link_libraries(${TARGET} PRIVATE
		-pthread 
		-ldl
	)
endif()

if (OS_WINDOWS)
	add_subdirectory(win_version)
	target_link_libraries(${TARGET} PRIVATE ${TARGET}_VERSION)
endif()

set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "${SAMPLE_APP_BIN_NAME}")

target_compile_definitions(${TARGET} PRIVATE 
	TSVB_APP_NAME="${SAMPLE_APP_NAME}"
	TSVB_APP_BIN_NAME="${SAMPLE_APP_BIN_NAME}"
	TSVB_VERSION_STRING="${VERSION_STR}"
	TSVB_BUNDLE_NAME="${TSVB_BUNDLE_NAME}"
	TSVB_COPYRIGHT="${TSVB_COPYRIGHT}"
	TSVB_COMPANY="${TSVB_COMPANY}"
)

set(RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)

if(OS_MACOS)
	find_library(COCOA_LIBRARY Cocoa)
	target_link_libraries(${TARGET} PRIVATE ${COCOA_LIBRARY})
	set_target_properties(${TARGET} PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/BundleInfo.plist.in"
		MACOSX_BUNDLE_BUNDLE_NAME "${TSVB_BUNDLE_NAME}"
		MACOSX_BUNDLE_COPYRIGHT "${TSVB_COPYRIGHT}"
		VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_BUILD}"
	)
	set(bundle_dir $<TARGET_BUNDLE_CONTENT_DIR:${TARGET}>)
	if (NOT DEFINED EFFECTS_SDK_DYLIB_PATH)
		set(EFFECTS_SDK_DYLIB_PATH "${EFFECTS_SDK_PATH}/libtsvb.dylib")
	endif()
	add_custom_command(TARGET ${TARGET} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory 
			"${RESOURCES_DIR}/color_luts" 
			"${bundle_dir}/Resources/color_luts"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different 
			"${RESOURCES_DIR}/background.jpg" 
			"${bundle_dir}/Resources/background.jpg"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle_dir}/Frameworks"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different 
			"${EFFECTS_SDK_DYLIB_PATH}" 
			"${bundle_dir}/Frameworks/libtsvb.dylib"
		VERBATIM
	)
endif()

if (OS_WINDOWS)
	add_custom_command(TARGET ${TARGET} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory 
			"${RESOURCES_DIR}/color_luts" 
			"$<TARGET_FILE_DIR:${TARGET}>/color_luts"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different 
			"${RESOURCES_DIR}/background.jpg" 
			"$<TARGET_FILE_DIR:${TARGET}>/background.jpg"
		VERBATIM
	)
	if (EFFECTS_SDK_BIN_PATH)
		add_custom_command(TARGET ${TARGET} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory 
				"${EFFECTS_SDK_BIN_PATH}/" 
				"$<TARGET_FILE_DIR:${TARGET}>"
			VERBATIM
		)
	endif()
endif()

if(OS_WINDOWS AND WINDEPLOYQT)
	add_custom_target(DeployQt
		COMMAND "${WINDEPLOYQT}" 
			--no-translations 
			--no-quick-import  
			"$<TARGET_FILE:${TARGET}>"
		DEPENDS ${TARGET}
		VERBATIM
	)
endif()

if(OS_LINUX AND LINUXDEPLOY)
	set(SDK_LIB_NAME libtsvb.so)
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/sample.desktop.in
		${CMAKE_CURRENT_BINARY_DIR}/${SAMPLE_APP_BIN_NAME}.desktop
		@ONLY	
	)
	set(appdir ${CMAKE_CURRENT_BINARY_DIR}/appdir)
	add_custom_target(DeployAppDir 
		COMMAND ${LINUXDEPLOY} 
			--appdir ${appdir} 
			--desktop-file=${CMAKE_CURRENT_BINARY_DIR}/${SAMPLE_APP_BIN_NAME}.desktop 
			--executable=$<TARGET_FILE:${TARGET}>
			--library=${EFFECTS_SDK_PATH}/lib/${SDK_LIB_NAME}
			--icon-file=${RESOURCES_DIR}/icons/256x256/logo-icon.png
			--icon-file=${RESOURCES_DIR}/icons/256x256/logo-icon.svg
			--plugin qt
		COMMAND ${CMAKE_COMMAND} -E copy_directory 
			${RESOURCES_DIR}/color_luts 
			${appdir}/usr/share/color_luts
		COMMAND ${CMAKE_COMMAND} -E copy_if_different 
			${RESOURCES_DIR}/background.jpg
			${appdir}/usr/share/background.jpg
		DEPENDS ${TARGET}
		VERBATIM
	)
endif()
