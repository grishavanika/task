# curl integration
include(CMakePrintHelpers)

if (NOT gcc_on_msvc)
	# AppVeyor has CURL installed, but it's not for MinGW, skip MinGW now
	find_package(CURL)
	if (CURL_FOUND)

		# See Vcpkg issue: https://github.com/Microsoft/vcpkg/issues/1909
		if (WIN32)
			list(LENGTH CURL_LIBRARY CURL_LIBRARY_LENGTH)
			if (CURL_LIBRARY_LENGTH EQUAL 1)
				set(CURL_LIBRARY_DEBUG_LIB	 ${CURL_LIBRARY})
				set(CURL_LIBRARY_RELEASE_LIB ${CURL_LIBRARY_DEBUG_LIB}/../../../lib/libcurl.lib)
				get_filename_component(CURL_LIBRARY_RELEASE_LIB ${CURL_LIBRARY_RELEASE_LIB} REALPATH)
				unset(CURL_LIBRARY CACHE)
				unset(CURL_LIBRARY)
				unset(CURL_LIBRARIES CACHE)
				unset(CURL_LIBRARIES)
				set(CURL_LIBRARY "debug;${CURL_LIBRARY_DEBUG_LIB};optimized;${CURL_LIBRARY_RELEASE_LIB}")
				set(CURL_LIBRARIES ${CURL_LIBRARY})
			endif ()
		endif ()

		cmake_print_variables(CURL_LIBRARIES CURL_INCLUDE_DIRS)

		add_library(Curl_Integrated INTERFACE)
		target_link_libraries(Curl_Integrated INTERFACE ${CURL_LIBRARIES})
		target_include_directories(Curl_Integrated INTERFACE ${CURL_INCLUDE_DIRS})
	endif ()
endif ()
