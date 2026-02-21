cmake_minimum_required(VERSION 3.14)
project(tropic_engine LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(cJSON REQUIRED)
find_package(CVector REQUIRED)

# Graphics libraries
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(OpenGL REQUIRED)

# Collect all C sources from src/ (reconfigures when new files are added)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)
if(NOT SOURCES)
	message(WARNING "No C sources found in ${CMAKE_SOURCE_DIR}/src")
endif()

add_executable(tropic_engine ${SOURCES})

# Use headers from inc/
target_include_directories(tropic_engine PRIVATE ${CMAKE_SOURCE_DIR}/inc)

target_compile_options(tropic_engine PRIVATE -g)
target_link_libraries(tropic_engine PRIVATE
	${CJSON_LIBRARIES}
	CVector::vector
	GLEW::GLEW
	GLUT::GLUT
	OpenGL::GL
)