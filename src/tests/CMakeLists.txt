include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(test_name rename_me)
add_subdirectory(test_${test_name})
set_target_properties(test_${test_name} PROPERTIES FOLDER tests)
new_test(${test_name} test_${test_name})

set(test_name task_curl)
add_subdirectory(test_${test_name})
if (TARGET test_task_curl)
	set_target_properties(test_${test_name} PROPERTIES FOLDER tests)
	new_test(${test_name} test_${test_name})
endif ()

