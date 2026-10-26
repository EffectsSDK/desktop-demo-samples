cmake_minimum_required(VERSION 3.26)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Qt 6.8 runs on macOS 12 and later")

project(SampleQml VERSION 1.0 LANGUAGES CXX)

if(APPLE)
	enable_language(OBJCXX)
	set(CMAKE_OBJCXX_STANDARD 17)
	set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EFFECTS_SDK_PATH "" CACHE PATH "Video Effects SDK: the directory holding include/vb_sdk")
if(NOT EXISTS "${EFFECTS_SDK_PATH}/include/vb_sdk")
	message(FATAL_ERROR "EFFECTS_SDK_PATH has to point at Video Effects SDK, the directory holding include/vb_sdk: '${EFFECTS_SDK_PATH}'")
endif()

if(WIN32)
	# The layout of the SDK package: bin_64, lib_64, bin_32, lib_32 next to include.
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(sdk_bits 64)
	else()
		set(sdk_bits 32)
	endif()
	if(EXISTS "${EFFECTS_SDK_PATH}/bin_${sdk_bits}")
		set(EFFECTS_SDK_BIN_PATH_DEFAULT "${EFFECTS_SDK_PATH}/bin_${sdk_bits}")
	endif()
	set(EFFECTS_SDK_BIN_PATH "${EFFECTS_SDK_BIN_PATH_DEFAULT}" CACHE PATH "The SDK binaries, copied next to the executable")

	option(EFFECTS_SDK_LINK "Link tsvb.lib instead of loading tsvb.dll at run time" OFF)
	set(EFFECTS_SDK_IMPLIB_PATH "${EFFECTS_SDK_PATH}/lib_${sdk_bits}/tsvb.lib" CACHE FILEPATH "The import library of tsvb.dll, for EFFECTS_SDK_LINK")
endif()

if(DEFINED ENV{PRODUCT_VERSION})
	set(SAMPLE_VERSION "$ENV{PRODUCT_VERSION}")
else()
	set(SAMPLE_VERSION "1.0.0.0")
endif()
set(SAMPLE_COPYRIGHT "(c) 2025-2026 Damiko Inc.")

set(SAMPLE_RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

find_package(Qt6 6.8.4 REQUIRED COMPONENTS Multimedia Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(SampleQml WIN32 MACOSX_BUNDLE
	main.cpp
)

file(GLOB COLOR_FILTER_PREVIEWS CONFIGURE_DEPENDS
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/resources/color_filters/*.jpg"
)

qt_add_qml_module(SampleQml
	URI SampleQml
	VERSION 1.0
	QML_FILES
		qml/Main.qml
		qml/FeatureCard.qml
		qml/LabeledSlider.qml
		qml/ColorFilterGrid.qml
	RESOURCES
		${COLOR_FILTER_PREVIEWS}
		resources/icons/reset.svg
	SOURCES
		camera_controller.cpp
		camera_controller.h
		capture_loop.cpp
		capture_loop.h
		controller_settings.cpp
		controller_settings.h
		sample_controller.cpp
		sample_controller.h
		sdk_library_handler.h
		video_filter.cpp
		video_filter.h
)

if(APPLE OR EFFECTS_SDK_LINK)
	target_sources(SampleQml PRIVATE sdk_library_handler_linked.cpp)
	if(NOT TARGET tsvb)
		add_library(tsvb SHARED IMPORTED)
		if(APPLE)
			set(EFFECTS_SDK_DYLIB_PATH "${EFFECTS_SDK_PATH}/libtsvb.dylib" CACHE FILEPATH "The SDK library, linked and copied into the bundle")
			set_target_properties(tsvb PROPERTIES
				IMPORTED_LOCATION "${EFFECTS_SDK_DYLIB_PATH}"
				IMPORTED_SONAME "@rpath/libtsvb.dylib"
			)
		else()
			set_target_properties(tsvb PROPERTIES IMPORTED_IMPLIB "${EFFECTS_SDK_IMPLIB_PATH}")
		endif()
	endif()
	target_link_libraries(SampleQml PRIVATE tsvb)
elseif(WIN32)
	target_sources(SampleQml PRIVATE sdk_library_handler_win.cpp)
else()
	target_sources(SampleQml PRIVATE sdk_library_handler_unix.cpp)
	target_link_libraries(SampleQml PRIVATE ${CMAKE_DL_LIBS})
endif()

target_include_directories(SampleQml PRIVATE "${EFFECTS_SDK_PATH}/include")

target_compile_definitions(SampleQml PRIVATE
	SAMPLE_VERSION="${SAMPLE_VERSION}"
	SAMPLE_COPYRIGHT="${SAMPLE_COPYRIGHT}"
)

target_link_libraries(SampleQml PRIVATE
	Qt6::Multimedia
	Qt6::Quick
	Qt6::QuickControls2
)

if(APPLE)
	if(NOT TARGET Qt6::MultimediaPrivate OR NOT TARGET Qt6::GuiPrivate)
		message(FATAL_ERROR "The macOS Metal bridge requires Qt Multimedia and Gui private development headers matching the installed Qt libraries.")
	endif()
	target_link_libraries(SampleQml PRIVATE Qt6::MultimediaPrivate Qt6::GuiPrivate)
	target_sources(SampleQml PRIVATE
		capture_loop_mac.cpp
		camera/camera_capturer.h
		camera/camera_capturer_mac.mm
		camera/frame_mailbox.h
		mac/pixel_buffer.h
		mac/pixel_buffer_video_frame.h
		mac/pixel_buffer_video_frame.mm
	)
	set_source_files_properties(camera/camera_capturer_mac.mm mac/pixel_buffer_video_frame.mm PROPERTIES
		COMPILE_OPTIONS "-fobjc-arc"
	)
	foreach(framework AVFoundation CoreMedia CoreVideo Foundation Metal)
		find_library(${framework}_FRAMEWORK ${framework} REQUIRED)
		target_link_libraries(SampleQml PRIVATE "${${framework}_FRAMEWORK}")
	endforeach()
endif()

if(WIN32)
	string(REPLACE "." "," SAMPLE_VERSION_COMMAS "${SAMPLE_VERSION}")
	configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)
	target_sources(SampleQml PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version.rc")

	target_sources(SampleQml PRIVATE
		capture_loop_win.cpp
		camera/camera_capturer.h
		camera/camera_capturer_win.cpp
		camera/frame_mailbox.h
	)
	target_link_libraries(SampleQml PRIVATE mf mfplat mfreadwrite mfuuid)
endif()

set_target_properties(SampleQml PROPERTIES
	OUTPUT_NAME "Video Effects SDK"
)

if(APPLE)
	set_property(TARGET SampleQml APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")

	string(REGEX MATCH "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?" SAMPLE_SHORT_VERSION "${SAMPLE_VERSION}")
	if(SAMPLE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.([0-9]+)")
		set(SAMPLE_BUILD_NUMBER "${CMAKE_MATCH_1}")
	else()
		set(SAMPLE_BUILD_NUMBER "${SAMPLE_SHORT_VERSION}")
	endif()

	set_target_properties(SampleQml PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.effectssdk.SDK.Sample"
		MACOSX_BUNDLE_BUNDLE_NAME "Video Effects SDK"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${SAMPLE_SHORT_VERSION}"
		MACOSX_BUNDLE_BUNDLE_VERSION "${SAMPLE_BUILD_NUMBER}"
		MACOSX_BUNDLE_COPYRIGHT "${SAMPLE_COPYRIGHT}"
	)

	set(bundle_dir "$<TARGET_BUNDLE_CONTENT_DIR:SampleQml>")
	add_custom_command(TARGET SampleQml POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
			"${SAMPLE_RESOURCES_DIR}/color_luts"
			"${bundle_dir}/Resources/color_luts"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${SAMPLE_RESOURCES_DIR}/background.jpg"
			"${bundle_dir}/Resources/background.jpg"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle_dir}/Frameworks"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${EFFECTS_SDK_DYLIB_PATH}"
			"${bundle_dir}/Frameworks/libtsvb.dylib"
		VERBATIM
	)
endif()

if(WIN32)
	add_custom_command(TARGET SampleQml POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
			"${SAMPLE_RESOURCES_DIR}/color_luts"
			"$<TARGET_FILE_DIR:SampleQml>/color_luts"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${SAMPLE_RESOURCES_DIR}/background.jpg"
			"$<TARGET_FILE_DIR:SampleQml>/background.jpg"
		VERBATIM
	)

	if(EFFECTS_SDK_BIN_PATH)
		add_custom_command(TARGET SampleQml POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
				"${EFFECTS_SDK_BIN_PATH}"
				"$<TARGET_FILE_DIR:SampleQml>"
			VERBATIM
		)
	endif()
endif()

if(WIN32 AND TARGET Qt6::windeployqt)
	add_custom_target(DeployQt
		COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
			--no-translations
			--qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
			"$<TARGET_FILE:SampleQml>"
		DEPENDS SampleQml
		VERBATIM
	)
endif()
