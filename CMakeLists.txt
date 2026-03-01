cmake_minimum_required(VERSION 3.15)
project(memcore)

set(CMAKE_CXX_STANDARD 17)

# Tell CMake where vcpkg packages are
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

find_package(gRPC CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)

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
    C:/vcpkg/installed/x64-windows/include
)

target_link_libraries(memcore PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)