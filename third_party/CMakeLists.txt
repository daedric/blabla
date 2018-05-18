set(OPENSSL_USE_STATIC_LIBS ON)
option(BUILD_BENCH OFF)
option(BUILD_TESTS OFF)
option(BUILD_EXAMPLES OFF)

add_subdirectory(commonpp)
add_sanitizers(commonpp)

set(MURMUR3_SRCS
        murmur3/murmur3.h
        murmur3/murmur3.c
        )
add_library(murmur3 ${MURMUR3_SRCS})
add_sanitizers(murmur3)
target_include_directories(murmur3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/murmur3")
target_compile_options(murmur3 PRIVATE "-Wno-implicit-fallthrough")