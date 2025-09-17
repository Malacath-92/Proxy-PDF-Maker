cmake_minimum_required(VERSION 3.21.1)

# --------------------------------------------------
# Set global properties
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/publish")
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})
set(CMAKE_AUTORCC ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --------------------------------------------------
# Define options
option(PPP_BUILD_TESTS "Build test suite" OFF)
option(PPP_FORMAT_TARGETS "Including clang-format targets" ON)

# --------------------------------------------------
# Run conan, needs to happen before project definition
include(cmake/conan.cmake)
set(CONAN_INSTALL_ARGS --build=missing)
run_conan(
    HOST_PROFILE default auto-cmake
    BUILD_PROFILE default
    INSTALL_ARGS ${CONAN_INSTALL_ARGS}
)

# --------------------------------------------------
# Define Project
project(ProxyPDF CXX C)

# --------------------------------------------------
# Include utility
include(cmake/group_files.cmake)

# --------------------------------------------------
# Add submodules
add_subdirectory("submodules/dla")

# --------------------------------------------------
# Add formatting target
if (PPP_FORMAT_TARGETS)
	include(cmake/clang-format.cmake)
	setup_format_target(format_proxy_pdf)
endif()

# --------------------------------------------------
# Get version tag
find_package(Git)
if(NOT GIT_FOUND)
	SET(GIT_EXECUTABLE git)
endif()

execute_process(
	COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty=-modified
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	OUTPUT_VARIABLE PROXY_PDF_GIT_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ECHO_ERROR_VARIABLE
	COMMAND_ERROR_IS_FATAL ANY)

# --------------------------------------------------
# Timestamp
string(TIMESTAMP PROXY_PDF_NOW "%d-%m-%Y @ %H:%M")

# --------------------------------------------------
# Find packages
find_package(fmt REQUIRED)
find_package(Qt6 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(libharu REQUIRED)
find_package(podofo REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(efsw REQUIRED)
find_package(magic_enum REQUIRED)

# --------------------------------------------------
# Workaround for conan not exposing this plugin
if (NOT TARGET Qt6::TlsBackendOpenSSLPlugin)
	if (WIN32)
		set(QT_TLS_BACKEND_OPENSSL_PLUGIN_DEBUG_SUFFIX "d")
	elseif(APPLE)
		set(QT_TLS_BACKEND_OPENSSL_PLUGIN_DEBUG_SUFFIX "_debug")
	else()
		set(QT_TLS_BACKEND_OPENSSL_PLUGIN_DEBUG_SUFFIX "")
	endif()

	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_DIR_DEBUG ${qt_PACKAGE_FOLDER_DEBUG}/plugins/tls)
	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_DIR_RELEASE ${qt_PACKAGE_FOLDER_RELEASE}/plugins/tls)

	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_BASE_NAME ${CMAKE_STATIC_LIBRARY_PREFIX}qopensslbackend)
	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_NAME_DEBUG
		${QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_BASE_NAME}${QT_TLS_BACKEND_OPENSSL_PLUGIN_DEBUG_SUFFIX})
	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_NAME_RELEASE
		${QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_BASE_NAME})

	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_DEBUG
		${QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_NAME_DEBUG}${CMAKE_STATIC_LIBRARY_SUFFIX})
	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_RELEASE
		${QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_NAME_RELEASE}${CMAKE_STATIC_LIBRARY_SUFFIX})

	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_DEBUG
		${QT_TLS_BACKEND_OPENSSL_PLUGIN_DIR_DEBUG}/${QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_DEBUG})
	set(QT_TLS_BACKEND_OPENSSL_PLUGIN_RELEASE
		${QT_TLS_BACKEND_OPENSSL_PLUGIN_DIR_RELEASE}/${QT_TLS_BACKEND_OPENSSL_PLUGIN_STATIC_LIB_RELEASE})

	add_library(qt_tls_backend_open_ssl_plugin STATIC IMPORTED GLOBAL)

	if (DEFINED CMAKE_CONFIGURATION_TYPES)
		set(BUILD_TYPES_UPPER "")
		foreach(BUILD_TYPE ${CMAKE_CONFIGURATION_TYPES})
			string(TOUPPER "${BUILD_TYPE}" BUILD_TYPE_UPPER)
			list(APPEND BUILD_TYPES_UPPER ${BUILD_TYPE_UPPER})
		endforeach()
	else()
		string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPES_UPPER)
	endif()
	
	foreach(BUILD_TYPE_UPPER ${BUILD_TYPES_UPPER})
		if (BUILD_TYPE_UPPER STREQUAL "DEBUG")
			set_target_properties(qt_tls_backend_open_ssl_plugin
				PROPERTIES IMPORTED_LOCATION_DEBUG ${QT_TLS_BACKEND_OPENSSL_PLUGIN_DEBUG})
		else()
			set_target_properties(qt_tls_backend_open_ssl_plugin
				PROPERTIES IMPORTED_LOCATION_${BUILD_TYPE_UPPER} ${QT_TLS_BACKEND_OPENSSL_PLUGIN_RELEASE})
		endif()
	endforeach()

	add_library(Qt6::TlsBackendOpenSSLPlugin ALIAS qt_tls_backend_open_ssl_plugin)
endif()

# --------------------------------------------------
# Create interface libs
add_library(proxy_pdf_warnings INTERFACE)
if(MSVC)
	target_compile_options(proxy_pdf_warnings INTERFACE /W4 /WX /permissive- /Zc:preprocessor)
else()
	target_compile_options(proxy_pdf_warnings INTERFACE -Wall -Wextra -pedantic -Werror)
endif()

add_library(proxy_pdf_options INTERFACE)
if(MSVC)
	target_compile_options(proxy_pdf_options INTERFACE /utf-8)
endif()

add_library(proxy_pdf_definitions INTERFACE)
target_compile_definitions(proxy_pdf_definitions INTERFACE
	PPP_SOURCE_ROOT="${CMAKE_SOURCE_DIR}/source/")
if(MSVC)
	target_compile_definitions(proxy_pdf_definitions INTERFACE
		_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
		_CRT_SECURE_NO_WARNINGS
		WIN32_LEAN_AND_MEAN
		NOMINMAX)
else()
endif()

option(PROXY_PDF_UNITY_BUILD "Build batch sources for faster builds." OFF)
if(PROXY_PDF_UNITY_BUILD)
	set_property(
		TARGET proxy_pdf_definitions
		PROPERTY UNITY_BUILD ON)
endif()

set(proxy_pdf_version_sources "source/version/ppp/version.cpp")
set(proxy_pdf_version_headers "source/version/ppp/version.hpp")
add_library(proxy_pdf_version STATIC
	${proxy_pdf_version_sources} ${proxy_pdf_version_headers})
target_include_directories(proxy_pdf_version PUBLIC source/version)
target_compile_definitions(proxy_pdf_version PRIVATE
	PROXY_PDF_VERSION=${PROXY_PDF_GIT_VERSION}
	PROXY_PDF_NOW=${PROXY_PDF_NOW})

add_library(proxy_pdf_dependencies INTERFACE)
target_link_libraries(proxy_pdf_dependencies INTERFACE
	dla
	fmt::fmt
	OpenSSL::SSL
	OpenSSL::Crypto
	Qt6::Core
	Qt6::Gui
	Qt6::Widgets
	Qt6::SvgWidgets
	Qt6::Xml
	Qt6::Network
	Qt6::TlsBackendOpenSSLPlugin
	opencv::opencv
	libharu::libharu
	podofo::podofo
	nlohmann_json::nlohmann_json
	efsw::efsw
	magic_enum::magic_enum)

if(WIN32)
	target_link_libraries(proxy_pdf_dependencies INTERFACE
		Qt6::QWindowsIntegrationPlugin)
elseif(APPLE)
	target_link_libraries(proxy_pdf_dependencies INTERFACE
		Qt6::QCocoaIntegrationPlugin)

	macro(target_link_framework target type fwname)
		find_library(FRAMEWORK_${fwname}
			NAMES ${fwname}
			PATHS ${CMAKE_OSX_SYSROOT}/System/Library
			PATH_SUFFIXES Frameworks
			NO_DEFAULT_PATH)

		if(${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
			MESSAGE(ERROR ": Framework ${fwname} not found")
		else()
			target_link_libraries(${target} ${type} ${FRAMEWORK_${fwname}})
			MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
		endif()
	endmacro(target_link_framework)

	target_link_framework(proxy_pdf_dependencies INTERFACE QuartzCore)
	target_link_framework(proxy_pdf_dependencies INTERFACE IOSurface)
 	target_link_framework(proxy_pdf_dependencies INTERFACE UniformTypeIdentifiers)
elseif(UNIX)
	target_link_libraries(proxy_pdf_dependencies INTERFACE
		Qt6::QXcbIntegrationPlugin)

	target_link_libraries(proxy_pdf_definitions INTERFACE
		stdc++exp)
endif() 

add_library(proxy_pdf_pch INTERFACE)
target_precompile_headers(proxy_pdf_pch INTERFACE
	<string>
	<string_view>
	<array>
	<vector>
	<deque>
	<map>
	<unordered_map>
	<set>
	<unordered_set>
	<fstream>
	<regex>
	<filesystem>
	<variant>
	<functional>
	<algorithm>
	<codecvt>
	<iomanip>
	<locale>
	<mutex>
	<fmt/format.h>)

if (WIN32)
	set(proxy_pdf_resources "resources.qrc;res/proxy_pdf.rc")
else()
	set(proxy_pdf_resources "resources.qrc")
endif()

# --------------------------------------------------
# Create library
file(GLOB_RECURSE proxy_pdf_lib_sources CONFIGURE_DEPENDS "source/lib/*.cpp")
file(GLOB_RECURSE proxy_pdf_lib_headers CONFIGURE_DEPENDS "source/lib/*.hpp" "source/lib/*.inl")
add_library(proxy_pdf_lib STATIC ${proxy_pdf_lib_sources} ${proxy_pdf_lib_headers})
target_link_libraries(proxy_pdf_lib PUBLIC
	proxy_pdf_warnings
	proxy_pdf_options
	proxy_pdf_version
	proxy_pdf_definitions
	proxy_pdf_dependencies
	proxy_pdf_pch)
set_target_properties(proxy_pdf_lib PROPERTIES AUTOMOC TRUE)
target_include_directories(proxy_pdf_lib PUBLIC "source/lib/include")
target_include_directories(proxy_pdf_lib PRIVATE "source/lib/source")
set_target_properties(proxy_pdf_lib PROPERTIES
    VS_GLOBAL_RunCodeAnalysis false
    VS_GLOBAL_EnableMicrosoftCodeAnalysis false
    VS_GLOBAL_EnableClangTidyCodeAnalysis true)

# --------------------------------------------------
# Create executable
file(GLOB_RECURSE proxy_pdf_sources CONFIGURE_DEPENDS "source/app/*.cpp")
file(GLOB_RECURSE proxy_pdf_headers CONFIGURE_DEPENDS "source/app/*.hpp" "source/app/*.inl")
if (WIN32)
	qt_add_executable(proxy_pdf WIN32 ${proxy_pdf_sources} ${proxy_pdf_headers} ${proxy_pdf_resources})
else()
	qt_add_executable(proxy_pdf ${proxy_pdf_sources} ${proxy_pdf_headers} ${proxy_pdf_resources})
endif()
target_link_libraries(proxy_pdf PRIVATE
	proxy_pdf_lib)
set_target_properties(proxy_pdf PROPERTIES AUTOMOC TRUE)
target_include_directories(proxy_pdf PRIVATE "source/app" "res")
set_target_properties(proxy_pdf PROPERTIES
    VS_GLOBAL_RunCodeAnalysis false
    VS_GLOBAL_EnableMicrosoftCodeAnalysis false
    VS_GLOBAL_EnableClangTidyCodeAnalysis true)

# --------------------------------------------------
# Set debugging properties
set_target_properties(proxy_pdf PROPERTIES
	VS_DEBUGGER_COMMAND_ARGUMENTS ""
	VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# --------------------------------------------------
# Create tests
if(PPP_BUILD_TESTS)
	message(STATUS "Building ppp test suite")

	add_library(proxy_pdf_test INTERFACE)
	target_link_libraries(proxy_pdf_test INTERFACE
		proxy_pdf_lib)

	enable_testing()
	add_subdirectory(tests)
endif()

# --------------------------------------------------
# Merge files from source and include in the IDE
group_files("${proxy_pdf_lib_sources};${proxy_pdf_lib_headers};${proxy_pdf_sources};${proxy_pdf_headers};${proxy_pdf_version_sources};${proxy_pdf_version_headers}")

# --------------------------------------------------
# Set startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT proxy_pdf)

# --------------------------------------------------
# Install exectuable and resource files
install(TARGETS
	proxy_pdf
	RUNTIME
	DESTINATION .)
install(FILES
	fallback.png
	README.pdf
	DESTINATION .)
