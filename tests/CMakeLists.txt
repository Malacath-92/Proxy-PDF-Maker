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

function(proxyPdfAddTest NAME)
	message(STATUS "\tAdding test " ${NAME})

	set(TEST_NAME ${NAME})
	
	add_executable(${TEST_NAME} ${NAME}.cpp)
	target_link_libraries(${TEST_NAME} PRIVATE
		catch2_main
		proxy_pdf_test)
	add_test(NAME ${TEST_NAME}
	    COMMAND ${TEST_NAME}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	set_target_properties(${TEST_NAME} PROPERTIES
		FOLDER proxy_pdf_tests
		VS_DEBUGGER_COMMAND_ARGUMENTS ""
		VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
						  
endfunction()

message(STATUS "Discovering all tests...")
file(GLOB files "*.cpp")
foreach(file ${files})
	get_filename_component(filename ${file} NAME_WE)
	proxyPdfAddTest(${filename})
endforeach()
