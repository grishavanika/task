set(exe_name example_simple_then)

set(depends_on_lib rename_me)

target_collect_sources(${exe_name})

add_executable(${exe_name} ${${exe_name}_files})

target_link_libraries(${exe_name} PRIVATE ${depends_on_lib})

set_all_warnings(${exe_name} PUBLIC)
