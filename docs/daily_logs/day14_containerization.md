# Day 14 — Containerisation with Docker
**Date: 9 March 2026**

---

## Goal

Containerise the full memcore system using Docker — C++ engine and Go API each in their own container, communicating via gRPC, with persistent storage surviving container restarts.

---

## Why Containerise

Without Docker, running memcore requires:
- Visual Studio Build Tools
- CMake + vcpkg
- gRPC compiled from source
- Go installed
- protoc + plugins

With Docker:
```bash
docker compose up
```

Anyone can run the full system with one command on any machine.

---

## Container Architecture

```
┌─────────────────────┐     gRPC      ┌─────────────────────┐
│   go-api            │ ────────────→ │   cpp-engine        │
│   port 8080         │               │   port 50051        │
└─────────────────────┘               └─────────────────────┘
        ↑
    HTTP requests
        
Volume: ./data → /app/data  (snapshot.bin + review.log persist here)
```

---

## Key Design Decisions

### Why Two Containers

Each process runs independently — Go and C++ have different runtimes, dependencies, and build processes. Separating them follows the same boundary enforced throughout the project: Go owns transport, C++ owns scheduling.

### Container Networking — Why Not localhost

Inside Docker each container has its own network namespace. `localhost` inside the Go container refers to the Go container itself — not C++.

Docker Compose creates an internal network where containers find each other by service name:

```
# Go container connects to C++ using service name
cpp-engine:50051
```

### Environment Variable for Host

Hardcoding `cpp-engine` would break local development. Used an environment variable instead:

```go
host := os.Getenv("CPP_ENGINE_HOST")
if host == "" {
    host = "localhost"  // default for local dev
}
conn, err := grpc.Dial(host+":50051", ...)
```

Docker Compose sets `CPP_ENGINE_HOST=cpp-engine`. Local dev uses the default. One codebase, two environments.

### Volume for Persistence

Without a volume, `snapshot.bin` and `review.log` live inside the container and are destroyed when it stops. The volume maps `./data` on the host machine into `/app/data` inside the container — data lives on the host, survives container restarts.

```yaml
volumes:
  - ./data:/app/data
```

File paths made configurable via environment variables:

```cpp
std::string snapshotPath = std::getenv("SNAPSHOT_PATH") 
    ? std::getenv("SNAPSHOT_PATH") : "snapshot.bin";
std::string logPath = std::getenv("LOG_PATH") 
    ? std::getenv("LOG_PATH") : "review.log";
```

---

## Go Dockerfile — Two Stage Build

```dockerfile
# Stage 1 - Build
FROM golang:1.25 AS builder
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build -o api .

# Stage 2 - Run
FROM debian:bookworm-slim
WORKDIR /app
COPY --from=builder /app/api .
EXPOSE 8080
CMD ["./api"]
```

**Why two stages:** The builder image (~800MB) has the Go compiler. The runner image (~20MB) only needs the compiled binary. Two stages keeps the final image small.

**Dependency caching:** `go.mod` and `go.sum` are copied and downloaded before source files. If source changes but dependencies don't, Docker reuses the cached download layer.

---

## C++ Dockerfile — Key Challenges

### Version Mismatch
The pre-generated stubs in `core/proto/` were generated with vcpkg protobuf v6.33. Ubuntu's system protobuf is older and incompatible.

**Fix:** Regenerate stubs inside the container using the container's own protoc — guarantees version compatibility regardless of the host machine.

```dockerfile
RUN mkdir -p core/proto && \
    protoc --cpp_out=core \
           --grpc_out=core \
           --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin \
           proto/flashcards.proto
```

### Dynamic Linking
The runner stage still needs `libgrpc++` and `libprotobuf` even though it's not compiling. The binary was dynamically linked at compile time — it loads these libraries at runtime. Without them the binary crashes immediately.

### .dockerignore
The local `build/` folder was being copied into the container causing `CMakeCache.txt` conflicts. Fixed with `.dockerignore`:

```
build/
data/
*.exe
*.obj
snapshot.bin
review.log
.git/
```

---

## Docker Compose

```yaml
version: '3.8'

services:
  cpp-engine:
    build:
      context: .
      dockerfile: Dockerfile
    ports:
      - "50051:50051"
    volumes:
      - ./data:/app/data
    environment:
      - SNAPSHOT_PATH=/app/data/snapshot.bin
      - LOG_PATH=/app/data/review.log

  go-api:
    build:
      context: ./api
      dockerfile: Dockerfile
    ports:
      - "8080:8080"
    environment:
      - CPP_ENGINE_HOST=cpp-engine
    depends_on:
      - cpp-engine
```

`depends_on` ensures C++ starts before Go — Go won't attempt a gRPC connection until the engine is up.

---

## End-to-End Verified in Docker

```bash
# All four endpoints working inside containers
POST /topic     → {"success":true}
POST /card      → {"success":true}
GET  /due-cards → {"card_ids":[1]}
POST /review    → {"next_review_date":1}

# Crash recovery verified
docker compose down
docker compose up
GET  /due-cards date=100 → {"card_ids":[2,1]}  ← data survived restart
```

---

## Files Added

```
Dockerfile              ← C++ engine container
api/Dockerfile          ← Go API container
docker-compose.yml      ← orchestrates both containers
.dockerignore           ← excludes build artifacts
data/                   ← volume mount for persistence
```

---

## Architecture Status After Today

| Layer | Status |
|-------|--------|
| C++ Engine | Complete |
| Persistence | Complete |
| gRPC Contract | Complete |
| Go API Layer | Complete |
| Integration Testing | Complete |
| Containerisation | Complete |

---

## How to Run

```bash
# Clone and run
git clone https://github.com/yourusername/memcore.git
cd memcore
docker compose up
```

That's it. No dependencies to install.

---

## Reflection

Containerisation forced thinking about the system from the outside in — what does this need to run, what survives restarts, how do processes find each other across network boundaries. Every decision made during the architecture phase paid off here: clean boundaries, configurable paths, environment-driven behaviour. The system that was hard to reason about as a tangle of processes became clean and portable as two well-defined containers.