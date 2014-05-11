solution "TinyCoroutine"

	configurations { "Debug", "Release" }

	configuration "Debug"
		flags { "Symbols" }
	configuration "Release"
		flags { "Optimize" }
		defines { "NDEBUG" }

	project "tinycoroutine"
		language "C"
		kind "StaticLib"

		files { 
			"tinycoroutine.c",
			"tinycoroutine.h",
		}

	project "coroutine_test"
		language "C"
		kind "ConsoleApp"

		files {
			"tinycoroutine_test.c"
		}

		links { "tinycoroutine" }
