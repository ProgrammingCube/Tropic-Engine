# We are already inside the tests/ directory here!

# Collect all C sources from the current directory (tests/)
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
if(NOT TEST_SOURCES)
	message(WARNING "No C sources found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Create the test executable
add_executable(engine_test ${TEST_SOURCES})

# Compile options for the test
target_compile_options(engine_test PRIVATE -g -Wall -Wextra -pedantic -D_GNU_SOURCE)

# Link it to the engine library created in the parent directory
target_link_libraries(engine_test PRIVATE tropic)
