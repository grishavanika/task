
if (TARGET task_curl)
	set(exe_name test_task_curl)

	set(depends_on_lib task_curl)

	target_collect_sources(${exe_name})

	add_executable(${exe_name} ${${exe_name}_files})

	set_all_warnings(${exe_name} PUBLIC)

	target_link_libraries(${exe_name} PRIVATE ${depends_on_lib})
	target_link_libraries(${exe_name} PRIVATE GTest_Integrated)

	#target_install_binaries(${exe_name})
else ()
	message("Skipping test_task_curl creation since task_curl is missing")
endif ()
