cmake_minimum_required(VERSION 3.13)

project(build-your-own-redis-cpp)

set(SOURCE_FILES
    src/command.cpp
    src/command_storage.cpp
    src/events.cpp
    src/handler.cpp
    src/handlers_manager.cpp
    src/main.cpp
    src/message_parser.cpp
    src/message.cpp
    src/poller.cpp
    src/rdb_parser.cpp
    src/replica_talker.cpp
    src/replica.cpp
    src/server_talker.cpp
    src/server.cpp
    src/storage_middleware.cpp
    src/storage.cpp
    src/talker.cpp
    src/utils.cpp
)

set(CMAKE_CXX_STANDARD 23)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_executable(server ${SOURCE_FILES})

target_link_libraries(server PRIVATE Threads::Threads)
