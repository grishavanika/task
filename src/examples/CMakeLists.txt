include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(example_name simple_then)

add_subdirectory(example_${example_name})
set_target_properties(example_${example_name} PROPERTIES FOLDER examples)
