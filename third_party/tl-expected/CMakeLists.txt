
# Download and unpack tl-expected at configure time
configure_file(CMakeLists-tl-expected.txt.in tl-expected-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tl-expected-download)
if (result)
	message(FATAL_ERROR "CMake step for tl-expected failed: ${result}")
endif ()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tl-expected-download)
if (result)
	message(FATAL_ERROR "Build step for tl-expected failed: ${result}")
endif ()

# Add tl-expected directly to our build
add_library(tl-expected INTERFACE)
target_include_directories(tl-expected INTERFACE
	${CMAKE_BINARY_DIR}/tl-expected-src)
