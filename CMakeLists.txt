cmake_minimum_required(VERSION 3.14)
project(tropic_engine LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Dependencies
find_package(cJSON REQUIRED)
find_package(CVector REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(
	cglm
	GIT_REPOSITORY https://github.com/recp/cglm.git
	GIT_TAG v0.9.0
)
FetchContent_MakeAvailable(cglm)

# =========================================================================
# BUILD THE TROPIC ENGINE LIBRARY
# =========================================================================

# Notice we use CMAKE_CURRENT_SOURCE_DIR to be safe with sub-directories
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
if(NOT ENGINE_SOURCES)
	message(WARNING "No C sources found in ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_library(tropic STATIC ${ENGINE_SOURCES})

target_include_directories(tropic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

target_compile_options(tropic PRIVATE -g -Wall -Wextra -pedantic -D_GNU_SOURCE)

target_link_libraries(tropic PUBLIC
	${CJSON_LIBRARIES}
	CVector::vector
	GLEW::GLEW
	GLUT::GLUT
	OpenGL::GL
	cglm
)

# =========================================================================
# INCLUDE SUBDIRECTORIES
# =========================================================================

# This tells CMake to go into the tests/ folder and look for a CMakeLists.txt
add_subdirectory(tests)
