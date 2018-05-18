include(LibFindMacros)

libfind_package(GRPC++ GRPC)

libfind_pkg_check_modules(GRPC++_PKGCONF grpc++)


find_path(grpc.h_INCLUDE_DIR
        NAMES grpc++.h grpc++/grpc++.h
        PATHS ${GRPC++_PKGCONF_INCLUDE_DIRS}
        )

find_library(GRPC++_LIBRARY
        NAMES grpc++
        PATHS ${GRPC++_PKGCONF_LIBRARY_DIRS}
        )

set(GRPC++_PROCESS_INCLUDES GRPC++_INCLUDE_DIR GRPC_INCLUDE_DIR)
set(GRPC++_PROCESS_LIBS GRPC++_LIBRARY GRPC_LIBRARY)
libfind_process(GRPC++)