project(tinycoroutine)

cmake_minimum_required (VERSION 3.0.2)

add_library(tinycoroutine STATIC "tinycoroutine.c")

set_target_properties(tinycoroutine PROPERTIES PUBLIC_HEADER "tinycoroutine.h")

install(TARGETS tinycoroutine
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		PUBLIC_HEADER DESTINATION include)

