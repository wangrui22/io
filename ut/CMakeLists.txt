aux_source_directory(./ SRC)

set(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -std=c++11 -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -std=c++11 -O3 -Wall")

add_definitions(-DBOOST_LOG_DYN_LINK)

find_package(Boost COMPONENTS system filesystem log thread REQUIRED)

add_executable(server mi_main_server.cpp)
target_link_libraries(server pthread ${Boost_LIBRARIES} libsocket)

add_executable(client mi_main_client.cpp)
target_link_libraries(client pthread ${Boost_LIBRARIES} libsocket)