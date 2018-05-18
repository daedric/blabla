include(CheckIncludeFiles)

set(BLABLA_SRC
    blabla/Blabla.cpp
    blabla/Blabla.hpp
    blabla/Router.hpp
    blabla/Router.cpp

    blabla/handlers/Protocol.hpp
    blabla/handlers/Protocol.cpp
    blabla/handlers/Acceptor.hpp
    blabla/handlers/Client.hpp
    blabla/handlers/Client.cpp
    blabla/handlers/Buffer.hpp
)

add_library(blabla ${BLABLA_SRC})
add_sanitizers(blabla)
target_link_libraries(blabla blabla_proto murmur3 ${DEP_LIBRARIES})
target_include_directories(blabla PRIVATE "${blabla_SOURCE_DIR}/third_party/hat-trie")
target_include_directories(blabla PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(blabla PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(CLIENT_SRC
    blabla/client/Client.cpp
    blabla/client/Client.hpp)

add_library(blabla_client ${CLIENT_SRC})
add_sanitizers(blabla_client)
target_link_libraries(blabla_client blabla_proto ${DEP_LIBRARIES})
target_include_directories(blabla_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})