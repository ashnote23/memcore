cmake_minimum_required(VERSION 3.15)
project(memcore)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

# Find grpc headers explicitly
find_path(GRPCPP_INCLUDE_DIR grpcpp/grpcpp.h
    PATHS /usr/include /usr/local/include
)

add_executable(memcore
    main.cpp
    core/scheduler.cpp
    core/review_service.cpp
    core/persistence/storage.cpp
    core/grpc_server.cpp
    core/proto/flashcards.pb.cc
    core/proto/flashcards.grpc.pb.cc
)

target_include_directories(memcore PRIVATE
    core
    core/proto
    ${GRPCPP_INCLUDE_DIR}
    ${GRPC_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
)

target_link_libraries(memcore PRIVATE
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)