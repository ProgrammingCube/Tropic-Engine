# We are already inside the tests/ directory here!

# Collect all C sources from the current directory (tests/)
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
if(NOT TEST_SOURCES)
	message(WARNING "No C sources found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Create the test executable
add_executable(tropic_test ${TEST_SOURCES})

# Compile options for the test
if(MSVC)
	target_compile_options(tropic_test PRIVATE /W4)
	target_compile_definitions(tropic_test PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(tropic_test PRIVATE -g -Wall -Wextra -pedantic)
	target_compile_definitions(tropic_test PRIVATE _GNU_SOURCE)
endif()

# Link it to the engine library created in the parent directory
target_link_libraries(tropic_test PRIVATE tropic)

# Assets are needed on every platform
add_custom_command(TARGET tropic_test POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/assets
		$<TARGET_FILE_DIR:tropic_test>/assets
)

# Only needed if vector is actually a Windows DLL build
if(WIN32)
	add_custom_command(TARGET tropic_test POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:vector>
			$<TARGET_FILE_DIR:tropic_test>
	)
endif()
