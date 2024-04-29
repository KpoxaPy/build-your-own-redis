cmake_minimum_required(VERSION 3.13)

project(redis-starter-cpp)

set(SOURCE_FILES 
    src/client.cpp
    src/main.cpp
    src/message.cpp
    src/request.cpp
    src/response.cpp
    src/server.cpp
    src/types.cpp
)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(asio CONFIG REQUIRED)

add_executable(server ${SOURCE_FILES})

target_link_libraries(server PRIVATE asio asio::asio)
target_link_libraries(server PRIVATE Threads::Threads)