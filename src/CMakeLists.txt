include_directories(${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_subdirectory(lib)

macro(add_binary_server bin_name)
    set(${bin_name}_SRCS)
    foreach(ARG ${ARGN})
        list(APPEND ${bin_name}_SRCS "${ARG}")
    endforeach()

    add_executable(${bin_name} ${${bin_name}_SRCS})
    target_link_libraries(${bin_name} blabla)
    add_sanitizers(${bin_name})
endmacro()

macro(add_binary_client bin_name)
    set(${bin_name}_SRCS)
    foreach(ARG ${ARGN})
        list(APPEND ${bin_name}_SRCS "${ARG}")
    endforeach()

    add_executable(${bin_name} ${${bin_name}_SRCS})
    target_link_libraries(${bin_name} blabla_client)
    add_sanitizers(${bin_name})
endmacro()

add_subdirectory(bin)

find_program(CLANG_FORMAT
            NAMES
                clang-format
                clang-format-9.0
                clang-format-8.1
                clang-format-8.0
                clang-format-7.0
                clang-format-6.2
                clang-format-6.0
                clang-format-5.2
                clang-format-5.0
                clang-format-4.0
                clang-format-3.9
                clang-format-3.8
        )


if (CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.h *.hpp *.c *.cc)
    add_custom_target(
            format
            COMMAND
                ${CLANG_FORMAT}
                -style=file
                -i
                ${ALL_SOURCE_FILES})
endif()