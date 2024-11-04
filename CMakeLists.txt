cmake_minimum_required(VERSION 3.17)

# --------------------------------------------------
# Set global properties
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/publish")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# --------------------------------------------------
# Run conan, needs to happen before project definition
include(cmake/conan.cmake)
run_conan(
    HOST_PROFILE default auto-cmake
    BUILD_PROFILE default
    INSTALL_ARGS --build=missing
)

# --------------------------------------------------
# Define Project
project(ProxyPDF CXX C)

# --------------------------------------------------
# Add submodules
add_subdirectory("submodules/dla")

# --------------------------------------------------
# Add formatting target
include(cmake/clang-format.cmake)
setup_format_target(format_proxy_pdf)

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
find_package(libharu REQUIRED)

# --------------------------------------------------
# Create interface libs
add_library(proxy_pdf_warnings INTERFACE)
if(MSVC)
	target_compile_options(proxy_pdf_warnings INTERFACE /W4 /WX /permissive-)
else()
	target_compile_options(proxy_pdf_warnings INTERFACE -Wall -Wextra -pedantic -Werror)
endif()

add_library(proxy_pdf_definitions INTERFACE)
target_compile_definitions(proxy_pdf_definitions INTERFACE
	_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
	WIN32_LEAN_AND_MEAN
	NOMINMAX)

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
	Qt6::Core
	Qt6::Gui
	Qt6::Widgets
	Qt6::QWindowsIntegrationPlugin
	libharu::libharu)

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

# --------------------------------------------------
# Create executbale
file(GLOB_RECURSE proxy_pdf_sources CONFIGURE_DEPENDS "source/app/*.cpp")
file(GLOB_RECURSE proxy_pdf_headers CONFIGURE_DEPENDS "source/app/*.hpp" "source/app/*.inl")
set(proxy_pdf_resources "res/proxy_pdf.rc")
add_executable(proxy_pdf ${proxy_pdf_sources} ${proxy_pdf_headers} ${proxy_pdf_resources})
target_link_libraries(proxy_pdf PRIVATE
	proxy_pdf_warnings
	proxy_pdf_definitions
	proxy_pdf_dependencies
	proxy_pdf_pch)
target_include_directories(proxy_pdf PRIVATE "source/app" "res")

# --------------------------------------------------
# Merge files from source and include in the IDE
function(group_files sources)
	foreach(FILE ${sources})
		# Get the directory of the source file
		get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

		# Remove common directory prefix to make the group
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

		# Make sure we are using windows slashes
		string(REPLACE "/" "\\" GROUP "${GROUP}")

		# Strip the root parts for each possible component
		if("${FILE}" MATCHES "source/launcher/.*")
			string(SUBSTRING ${GROUP} 16 -1 GROUP)
		elseif("${FILE}" MATCHES "source/proxy_pdf/.*")
			string(SUBSTRING ${GROUP} 17 -1 GROUP)
		elseif("${FILE}" MATCHES "source/shared/.*")
			string(SUBSTRING ${GROUP} 14 -1 GROUP)
		endif()

		# Do the grouping
		source_group("${GROUP}" FILES "${FILE}")
	endforeach()
endfunction()

group_files("${proxy_pdf_sources};${proxy_pdf_headers};${proxy_pdf_version_sources};${proxy_pdf_version_headers}")

# --------------------------------------------------
# Set debugging properties
set_target_properties(proxy_pdf PROPERTIES
	VS_DEBUGGER_COMMAND_ARGUMENTS ""
	VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT proxy_pdf)

# --------------------------------------------------
# Install shared lib and launcher
install(TARGETS
	proxy_pdf
	RUNTIME
	DESTINATION .)
