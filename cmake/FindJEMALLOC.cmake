FIND_LIBRARY(JEMALLOC_LIBRARIES
  NAMES
    libjemalloc.a
    jemalloc

  PATHS
    /usr/
    /usr/local/
    /opt/local/

  PATH_SUFFIXES
    lib64
    lib
)

MESSAGE(STATUS "LIB: ${JEMALLOC_LIBRARIES}")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JEMALLOC DEFAULT_MSG JEMALLOC_LIBRARIES)
MARK_AS_ADVANCED(JEMALLOC_LIBRARIES)
