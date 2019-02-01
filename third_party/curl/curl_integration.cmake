# curl integration
include(CMakePrintHelpers)

find_package(CURL)
if (CURL_FOUND)
	cmake_print_variables(CURL_LIBRARIES CURL_INCLUDE_DIRS)

	add_library(Curl_Integrated INTERFACE)
	target_link_libraries(Curl_Integrated INTERFACE ${CURL_LIBRARIES})
	target_include_directories(Curl_Integrated INTERFACE ${CURL_INCLUDE_DIRS})
endif()
