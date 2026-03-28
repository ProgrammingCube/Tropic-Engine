file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" TROPIC_SOURCE_DIR_CMAKE)

set(LEVEL_EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/level_editor.cpp
    ${CMAKE_SOURCE_DIR}/tests/level_loader.c
)

add_executable(tropic_level_editor ${LEVEL_EDITOR_SOURCES})

target_include_directories(tropic_level_editor PRIVATE
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/tests
)

target_compile_definitions(tropic_level_editor PRIVATE
    TROPIC_SOURCE_DIR="${TROPIC_SOURCE_DIR_CMAKE}"
)

if(MSVC)
    target_compile_options(tropic_level_editor PRIVATE /W4)
    target_compile_definitions(tropic_level_editor PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(tropic_level_editor PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(tropic_level_editor PRIVATE _GNU_SOURCE)
endif()

target_link_libraries(tropic_level_editor PRIVATE tropic)

add_custom_command(TARGET tropic_level_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:tropic_level_editor>/assets
)

if(WIN32)
    add_custom_command(TARGET tropic_level_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:vector>
            $<TARGET_FILE_DIR:tropic_level_editor>
    )
endif()
