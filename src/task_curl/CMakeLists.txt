
if (TARGET Curl_Integrated)
	set(lib_name task_curl)

	target_collect_sources(${lib_name})

	add_library(${lib_name} ${${lib_name}_files})
	set_all_warnings(${lib_name} PUBLIC)
	target_include_directories(${lib_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
	target_include_directories(${lib_name} PUBLIC include)
	target_link_libraries(${lib_name} PUBLIC rename_me)
	target_link_libraries(${lib_name} PUBLIC Curl_Integrated)
else ()
	message("Skipping task_curl creation since CURL is missing")
endif ()
