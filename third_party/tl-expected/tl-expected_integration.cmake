# tl-expected integration

add_subdirectory(tl-expected)

if (clang_on_msvc)
	target_compile_options(tl-expected INTERFACE
		-Wno-documentation-unknown-command)
endif ()
