# curl integration

find_package(CURL)
if (CURL_FOUND)
	add_library(Curl_Integrated INTERFACE)
	target_link_libraries(Curl_Integrated INTERFACE ${CURL_LIBRARIES})
	target_include_directories(Curl_Integrated INTERFACE ${CURL_INCLUDE_DIRS})
endif()
