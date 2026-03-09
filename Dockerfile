# Stage 1 - Build
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    git \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .


# Regenerate proto stubs
RUN mkdir -p core/proto && \
    protoc --cpp_out=core \
           --grpc_out=core \
           --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin \
           proto/flashcards.proto

# Verify stubs were generated
RUN ls -la core/proto/

# Build with CMake
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --parallel

# Stage 2 - Run
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libgrpc++1.51 \
    libprotobuf32 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/memcore .
EXPOSE 50051
CMD ["./memcore"]