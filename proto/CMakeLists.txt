find_package(Protobuf REQUIRED)
protobuf_generate_cpp(BLABLA_SRCS BLABLA_HDRS service.proto)
add_library(blabla_proto ${BLABLA_SRCS} ${BLABLA_HDRS})
add_sanitizers(blabla_proto)
target_link_libraries(blabla_proto ${Protobuf_LIBRARIES})
target_include_directories(blabla_proto PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../>
        $<INSTALL_INTERFACE:include/blabla>
        )