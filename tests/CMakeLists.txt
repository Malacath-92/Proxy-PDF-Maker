# --------------------------------------------------
# Find packages
find_package(Catch2 QUIET REQUIRED
	COMPONENTS
	catch2_with_main)

# --------------------------------------------------
# Build Catch2 main seperately for faster building
add_library(catch2_main INTERFACE)
target_link_libraries(catch2_main
	INTERFACE
	Catch2::Catch2WithMain)

function(proxyPdfAddTest NAME SHOULD_COPY_DEPENDENCIES)
	message(STATUS "\tAdding test " ${NAME})

	set(TEST_NAME ${NAME})

	add_executable(${TEST_NAME} ${NAME}.cpp)
	target_link_libraries(${TEST_NAME} PRIVATE
		catch2_main
		proxy_pdf_test)
	add_test(NAME ${TEST_NAME}
		COMMAND ${TEST_NAME}
		WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_NAME}>)
	set_target_properties(${TEST_NAME} PROPERTIES
		FOLDER proxy_pdf_tests
		VS_DEBUGGER_COMMAND_ARGUMENTS ""
		VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_NAME}>)

	if (${SHOULD_COPY_DEPENDENCIES})
		# --------------------------------------------------
		# Create a custom command that copies files that tests need
		add_custom_command(TARGET ${TEST_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${CMAKE_SOURCE_DIR}/fallback.png"
				"${CMAKE_SOURCE_DIR}/res/cubes/Foils Vibrance.CUBE"
				"${CMAKE_SOURCE_DIR}/tests/madness.CUBE"
				"$<TARGET_FILE_DIR:${TEST_NAME}>"
			COMMENT "Copying test dependencies to $<TARGET_FILE_DIR:${TEST_NAME}>")
	endif()
endfunction()

message(STATUS "Discovering all tests...")
file(GLOB files "*.cpp")
set(PPP_NEEDS_TEST_DEPENDENCIES_COMMAND TRUE)
foreach(file ${files})
	get_filename_component(filename ${file} NAME_WE)
	proxyPdfAddTest(${filename} ${PPP_NEEDS_TEST_DEPENDENCIES_COMMAND})
	set(PPP_NEEDS_TEST_DEPENDENCIES_COMMAND FALSE)
endforeach()

# --------------------------------------------------
# CLI Tests need some special treatment
target_compile_definitions(cli_tests PRIVATE
	PROXY_PDF_CLI_DIR="$<TARGET_FILE_DIR:proxy_pdf_cli>"
	PROXY_PDF_CLI_EXE="$<TARGET_FILE_DIR:proxy_pdf_cli>/$<TARGET_FILE_NAME:proxy_pdf_cli>")
