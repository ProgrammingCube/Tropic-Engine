cmake_minimum_required(VERSION 3.14)
project(tropic_engine LANGUAGES C CXX)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT tropic_level_editor)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies
find_package(cJSON REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

include(FetchContent)

FetchContent_Declare(
	cglm
	GIT_REPOSITORY https://github.com/recp/cglm.git
	GIT_TAG v0.9.0
)
FetchContent_MakeAvailable(cglm)

# Build CVector as a static library to avoid DLL issues on Windows
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)

FetchContent_Declare(
	CVector
	GIT_REPOSITORY https://github.com/ProgrammingCube/c_vector.git
)
FetchContent_MakeAvailable(CVector)

# =========================================================================
# BUILD THE TROPIC ENGINE LIBRARY
# =========================================================================

# Notice we use CMAKE_CURRENT_SOURCE_DIR to be safe with sub-directories
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
if(NOT ENGINE_SOURCES)
	message(WARNING "No C sources found in ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_library(tropic STATIC ${ENGINE_SOURCES} "inc/beat_grid.h")

target_include_directories(tropic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# Use compiler-specific flags
if(MSVC)
	target_compile_options(tropic PRIVATE /W4)
	target_compile_definitions(tropic PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(tropic PRIVATE -Wall -Wextra -g)
	target_compile_definitions(tropic PRIVATE _GNU_SOURCE)
endif()

target_link_libraries(tropic PUBLIC
	cjson
	vector
	glfw
	OpenGL::GL
	cglm
)

function(tropic_sync_assets_for_target target_name)
    add_custom_command(TARGET ${target_name} PRE_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory
			$<TARGET_FILE_DIR:${target_name}>/assets
		COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_SOURCE_DIR}/assets
			$<TARGET_FILE_DIR:${target_name}>/assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_SOURCE_DIR}/assets
            ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/assets
		COMMENT "Syncing assets for ${target_name}"
	)

	set_target_properties(${target_name} PROPERTIES
       VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}"
	)
endfunction()

# =========================================================================
# INCLUDE SUBDIRECTORIES
# =========================================================================

add_subdirectory(editor)

# This tells CMake to go into the tests/ folder and look for a CMakeLists.txt
add_subdirectory(tests)