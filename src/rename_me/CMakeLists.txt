set(lib_name rename_me)

target_collect_sources(${lib_name})

add_library(${lib_name} ${${lib_name}_files})

set_all_warnings(${lib_name} PUBLIC)

target_include_directories(${lib_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(${lib_name} PUBLIC include)

target_link_libraries(${lib_name} PUBLIC expected-lite)

# Now library include future_task example and on GCC on *nix this
# depends on pthread
if (NOT ${windows} AND ${gcc})
	target_link_libraries(${lib_name} PUBLIC pthread)
endif ()
