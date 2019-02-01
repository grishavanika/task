cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
# 3.8* is required because of C++17 support
project(RenameMe)

include(tools/cmake/utils.cmake)
include(tools/cmake/clang_msvc_integration.cmake)
include(CTest)

# Enable folders for VS
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Leave only most Debug/Release configurations
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

detect_compilers()
set_cpp17_standard()

if (clang_on_msvc)
	make_clang_work_on_windows()
	# TODO: make this work. Now it fails to compile with
	# invalid argument '/MDd' not allowed with '-fsanitize=address'
	# add_compile_options(-fsanitize=address)
endif ()

if (gcc)
	message("GCC: -Wno-unused-function")
	add_compile_options(-Wno-unused-function)	
endif ()

add_subdirectory(third_party)
add_subdirectory(src)

