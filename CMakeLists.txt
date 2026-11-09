cmake_minimum_required(VERSION 3.13)
project(nebula-sdl C)
if(WIN32)
	enable_language(RC)
endif()

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release)
endif()

add_library(nebula_core STATIC src/nebula.c src/draw.c)
target_include_directories(nebula_core PUBLIC src)
if(NOT WIN32)
	target_link_libraries(nebula_core PUBLIC m)
endif()

# headless PPM renderer, handy for testing without a display
add_executable(render_ppm tools/render_ppm.c)
target_link_libraries(render_ppm nebula_core)

find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
	add_executable(nebula src/main.c)
	target_link_libraries(nebula nebula_core
		$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)
	if(TARGET SDL2::SDL2main AND WIN32)
		target_link_libraries(nebula SDL2::SDL2main)
	endif()
endif()

if(NOT SDL2_FOUND)
	message(STATUS "SDL2 not found: skipping the SDL front end")
endif()

# Native Windows screen saver: no SDL, only system DLLs
if(WIN32)
	add_executable(nebula_scr WIN32
		win32/scr_main.c win32/settings.c win32/nebula.rc)
	target_include_directories(nebula_scr PRIVATE win32)
	set_target_properties(nebula_scr PROPERTIES
		OUTPUT_NAME nebula SUFFIX ".scr")
	target_link_libraries(nebula_scr nebula_core
		winmm comctl32 user32 gdi32 advapi32)
	if(MSVC)
		target_compile_definitions(nebula_scr PRIVATE _CRT_SECURE_NO_WARNINGS)
	endif()
endif()
