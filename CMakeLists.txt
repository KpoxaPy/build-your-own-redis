cmake_minimum_required(VERSION 3.13)

project(redis-starter-cpp)

set(SOURCE_FILES 
    src/command.cpp
    src/events.cpp
    src/handler.cpp
    src/main.cpp
    src/message.cpp
    src/poller.cpp
    src/server.cpp
    src/storage.cpp
    src/types.cpp
    src/utils.cpp
)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_executable(server ${SOURCE_FILES})

target_link_libraries(server PRIVATE Threads::Threads)