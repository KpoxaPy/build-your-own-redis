cmake_minimum_required(VERSION 3.13)

project(build-your-own-redis-cpp)

set(SOURCE_FILES 
    src/command.cpp
    src/events.cpp
    src/handler.cpp
    src/handlers_manager.cpp
    src/main.cpp
    src/message.cpp
    src/message_parser.cpp
    src/poller.cpp
    src/replica.cpp
    src/replica_talker.cpp
    src/server.cpp
    src/server_talker.cpp
    src/storage.cpp
    src/talker.cpp
    src/utils.cpp
)

set(CMAKE_CXX_STANDARD 23)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_executable(server ${SOURCE_FILES})

target_link_libraries(server PRIVATE Threads::Threads)