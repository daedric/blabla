include(LibFindMacros)

libfind_pkg_check_modules(GRPC_PKGCONF grpc)


find_path(grpc.h_INCLUDE_DIR
        NAMES grpc.h grpc/grpc.h
        PATHS ${GRPC_PKGCONF_INCLUDE_DIRS}
        )

find_library(GRPC_LIBRARY
        NAMES grpc
        PATHS ${GRPC_PKGCONF_LIBRARY_DIRS}
        )

set(GRPC_PROCESS_INCLUDES GRPC_INCLUDE_DIR)
set(GRPC_PROCESS_LIBS GRPC_LIBRARY)
libfind_process(GRPC)