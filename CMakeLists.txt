project(blabla C CXX)
cmake_minimum_required(VERSION 3.6)

CMAKE_POLICY(SET CMP0005 NEW)
CMAKE_POLICY(SET CMP0054 NEW)
#CMAKE_POLICY(SET CMP0075 NEW)

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting cmake build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Default install prefix" FORCE)
endif()


set(CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard for all targets.")
set(CMAKE_CXX_STANDARD_REQUIRED true)

if (${CMAKE_CXX_COMPILER_ID} MATCHES GNU OR ${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    add_definitions("-W -Wextra -Wall -Wno-unused-parameter")
    include(CheckCXXCompilerFlag)
      # With clang + gcc we gave __attribute__ ((unused))
    SET(HAVE_UNUSED_ATTR 1)

    IF (UNIX)
        list(APPEND CMAKE_REQUIRED_LIBRARIES "rt")
    ENDIF()

    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
        add_definitions("-fstack-protector-all")

        if (${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
            add_definitions("-ggdb3")
        endif()
    endif()
endif()

set(CMAKE_MODULE_PATH
    "${CMAKE_SOURCE_DIR}/cmake"
    "${CMAKE_SOURCE_DIR}/cmake/externals/sanitizers-cmake/cmake"
    ${CMAKE_MODULE_PATH}
)

find_package(Sanitizers)

include(SubDirList)

if ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    set(HAVE_SYSLOG 1)
endif()

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
find_package(Threads REQUIRED)
find_package(Boost
        COMPONENTS
            log
            log_setup
            iostreams
            program_options
            filesystem
            system
            thread
        REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})

find_package(ZLIB REQUIRED)

SET(DEP_LIBRARIES
    commonpp
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${Backtrace_LIBRARIES}
    )

if (${CMAKE_BUILD_TYPE} MATCHES "Release" OR
    ${CMAKE_BUILD_TYPE} MATCHES "RelWithDebInfo")
    find_package(JEMALLOC)
    if (JEMALLOC_FOUND)
        list(APPEND DEP_LIBRARIES ${JEMALLOC_LIBRARIES})
        if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
            list(APPEND DEP_LIBRARIES "dl")
        endif()
        add_definitions(-DJEMALLOC_ENABLED)
        message(STATUS "Linking with JEMalloc")
    else()
        message(WARNING "Failed to find JEMalloc libraries for Release build!")
    endif()
endif()

# Add threads at the end since jemalloc could need them
list(APPEND DEP_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

add_subdirectory(proto)
add_subdirectory(src)
add_subdirectory(third_party)

if (${BUILD_TESTS})
    enable_testing()
    add_subdirectory(tests/)
endif()
