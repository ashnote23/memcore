# memcore

A crash-safe spaced repetition engine built in C++, with a Go API layer connected via gRPC — fully containerised with Docker and deployable on Kubernetes.

This is not a flashcard app. It is a backend storage engine that happens to schedule flashcards — built with the same reasoning you would apply to a production system: write-ahead logging, snapshot recovery, strict architectural boundaries, and deterministic scheduling logic.

---

## Run with Docker

```bash
git clone https://github.com/ashnote23/memcore.git
cd memcore
docker compose up
```

That's it. No dependencies to install.

---

## Run on Kubernetes

```bash
kubectl apply -f k8s/memcore-pod.yaml
kubectl port-forward pod/memcore 8080:8080
```

Both containers (Go API + C++ engine) run as a single multi-container Pod sharing the same network namespace.

---

## Why This Exists

Most spaced repetition tools are wrappers around a database with some scheduling logic sprinkled in. This project treats the scheduling engine as the core and builds everything else around it — the same way a database treats its storage engine as the core.

The goal was to learn how to think about backend systems properly: not just make something work, but make something that is correct under failure, has clear ownership boundaries, and can be reasoned about at every layer.

---

## Architecture

```
Client (curl / HTTP)
  ↓  HTTP + JSON
Go API Layer              (port 8080)
  ↓  gRPC (protobuf)
C++ Engine                (port 50051)
  ↓
File Storage              (snapshot.bin + review.log)
```

**C++ owns:**
- The SM-2 scheduling algorithm
- All card state and mutations
- Persistence and crash recovery
- Domain invariants

**Go owns:**
- HTTP routing and JSON serialization
- Input validation before forwarding to C++
- gRPC client — translates HTTP requests into gRPC calls

The boundary is enforced strictly. Go never computes a scheduling decision. C++ never handles an HTTP request.

---

## API Endpoints

| Method | Route | Description |
|--------|-------|-------------|
| POST | `/review` | Submit a card review — returns next review date |
| POST | `/card` | Add a new flashcard |
| POST | `/topic` | Create a new topic |
| GET | `/due-cards` | Get cards due for review on a given date |

### Example

```bash
# Create a topic
curl -X POST http://localhost:8080/topic \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"topic_id":100,"name":"DSA"}'
# → {"success":true}

# Add a card
curl -X POST http://localhost:8080/card \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"card_id":1,"topic_id":100}'
# → {"success":true}

# Get due cards
curl -X GET http://localhost:8080/due-cards \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"date":0,"topic_id":100}'
# → {"card_ids":[1]}

# Review a card
curl -X POST http://localhost:8080/review \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"card_id":1,"rating":3}'
# → {"next_review_date":1}
```

---

## Scheduling — SM-2 Algorithm

Cards are scheduled using a classic SM-2 implementation with a 0–3 rating scale.

| Rating | Meaning |
|--------|---------|
| 0 | Complete blackout |
| 1 | Hard |
| 2 | Medium |
| 3 | Easy |

**Interval growth:**
- Repetition 1 → 1 day
- Repetition 2 → 6 days
- Repetition 3+ → `previous_interval × ease_factor`

**Ease factor update:**
```
EF' = EF + (0.1 − (3 − q)(0.08 + (3 − q) × 0.02))
EF  = max(1.3, EF')
```

The 1.3 floor is not arbitrary. Without it, repeated low ratings drive EF below 1.0, causing intervals to shrink geometrically and trapping cards in a permanent short-review loop. The floor guarantees minimum 30% interval growth per successful review and keeps the system mathematically stable.

---

## Persistence — Snapshot + Append-Only Log

The engine persists state using two files:

**`snapshot.bin`** — full materialized state of all cards at a checkpoint.

**`review.log`** — append-only log of every review event since the last snapshot.

```
Snapshot format:
[card_count : int]
[userId][cardId][topicId][easeFactor][interval][repetitions][nextReviewDate]
...

Log format:
[userId][cardId][rating][timestamp][CRC32]
...
```

**On startup:**
1. Load snapshot → reconstruct in-memory card state
2. Replay review log → reapply events since last checkpoint
3. Rebuild due-card heap from reconstructed state

**Crash safety:**
- Each log record includes a CRC32 checksum — a partial write from a crash is detected on replay and discarded safely
- The log has no count header — reading stops at EOF or the first CRC mismatch
- Log is cleared after every snapshot — no double replay on restart

---

## gRPC Contract

The interface between Go and C++ is defined in `proto/flashcards.proto`:

```protobuf
service FlashcardService {
    rpc ReviewComplete (ReviewCompleteRequest)  returns (ReviewCompleteResponse);
    rpc AddCard        (AddCardRequest)         returns (AddCardResponse);
    rpc GetDueCards    (GetDueCardsRequest)      returns (GetDueCardsResponse);
    rpc CreateTopic    (CreateTopicRequest)      returns (CreateTopicResponse);
}
```

Go and C++ stubs are generated from this file — Go never calls C++ in any way not defined by this contract.

---

## Docker

Two containers orchestrated with Docker Compose:

```
┌─────────────────────┐     gRPC      ┌─────────────────────┐
│   go-api            │ ────────────→ │   cpp-engine        │
│   port 8080         │               │   port 50051        │
└─────────────────────┘               └─────────────────────┘

Volume: ./data → /app/data  (snapshot.bin + review.log persist here)
```

**Persistence across restarts** — `snapshot.bin` and `review.log` are stored in a volume-mounted `./data` folder on the host. Containers can be destroyed and recreated — card history survives.

**Environment-driven configuration** — `CPP_ENGINE_HOST` controls how Go finds C++. Defaults to `localhost` for local development, set to `cpp-engine` in Docker Compose for container networking.

---

## Kubernetes

Both containers run as a single multi-container Pod. They share the same network namespace — Go reaches C++ on `localhost:50051`, the same as local development.

```
┌──────────────────────────────────────────┐
│              memcore Pod                 │
│                                          │
│  ┌─────────────┐      ┌───────────────┐  │
│  │   go-api    │─────▶│  cpp-engine   │  │
│  │  port 8080  │ gRPC │  port 50051   │  │
│  └─────────────┘      └───────────────┘  │
│                                          │
│  Shared network namespace                │
│  CPP_ENGINE_HOST=localhost               │
└──────────────────────────────────────────┘
```

**Why one Pod and not two:**
Containers in the same Pod share `localhost`. Go connects to C++ on `localhost:50051` — no Service or DNS resolution needed. Splitting them into separate Pods would require a ClusterIP Service between them, adding unnecessary complexity at this stage.

**Key difference from Docker Compose:**
Docker Compose uses service names (`cpp-engine:50051`) for container discovery. Kubernetes Pods use `localhost` for intra-pod communication. The `CPP_ENGINE_HOST` environment variable handles both — `cpp-engine` in Docker Compose, `localhost` in Kubernetes.

### Manifest

```yaml
# k8s/memcore-pod.yaml
apiVersion: v1
kind: Pod
metadata:
  name: memcore
  labels:
    app: memcore
spec:
  containers:
    - name: cpp-engine
      image: memcore-c++:latest
      imagePullPolicy: Never
      ports:
        - containerPort: 50051

    - name: go-api
      image: memcore-go:latest
      imagePullPolicy: Never
      ports:
        - containerPort: 8080
      env:
        - name: CPP_ENGINE_HOST
          value: "localhost"
```

### End-to-End Verified on Kubernetes

```bash
POST /topic     → {"success":true}
POST /card      → {"success":true}
GET  /due-cards → {"card_ids":[1]}
POST /review    → {"next_review_date":1}
```

---

## Project Structure

```
memcore/
├── main.cpp                        C++ entry point + simulation
├── CMakeLists.txt                  CMake build config
├── Dockerfile                      C++ engine container
├── docker-compose.yml              orchestrates both containers
├── .dockerignore
├── proto/
│   └── flashcards.proto            gRPC service contract
├── core/
│   ├── model.h                     data models (User, Card, Topic)
│   ├── scheduler.h / .cpp          SM-2 scheduling logic
│   ├── review_service.h / .cpp     validation + orchestration
│   ├── grpc_server.cpp             C++ gRPC server implementation
│   └── persistence/
│       └── storage.h / .cpp        snapshot + log implementation
├── api/
│   ├── main.go                     Go HTTP server + gRPC client
│   ├── Dockerfile                  Go API container
│   ├── go.mod / go.sum
│   └── proto/                      generated Go gRPC stubs
├── k8s/
│   └── memcore-pod.yaml            Kubernetes multi-container Pod manifest
├── data/                           volume mount — persisted files
└── docs/
    ├── architecture.md             full system design document
    └── daily_logs/                 engineering log — design decisions
```

---

## Build Locally (without Docker)

### Prerequisites
- Visual Studio 2022 Build Tools with C++ desktop development
- CMake 3.15+
- vcpkg with gRPC:
```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
./vcpkg.exe install grpc
```
- Go 1.25+
- protoc with Go and C++ plugins

### Build C++ Engine
```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

### Run
```powershell
# Terminal 1 — C++ engine
./build/Debug/memcore.exe

# Terminal 2 — Go API
cd api
go run main.go
```

---

## Design Decisions

Every major decision in this project was derived from constraints, not picked from a list. The reasoning is documented day by day in [`docs/daily_logs/`](docs/daily_logs/).

Key decisions documented:
- Why C++ owns scheduling and Go owns transport
- Why gRPC was chosen over raw TCP or shared memory
- Why the SM-2 ease factor floor is mathematically necessary
- Why snapshot + log beats a database for this use case
- Why the log has no count header but the snapshot does
- Why CRC is computed over data fields only, never over itself
- Why `ReviewService` takes a `Scheduler&` reference instead of owning one
- Why environment variables replace hardcoded hostnames for Docker
- Why Go and C++ run in the same Kubernetes Pod

---

## Status

| Layer | Status |
|-------|--------|
| C++ Scheduling Engine | Complete |
| Service Layer | Complete |
| Persistence Layer | Complete |
| Crash Recovery | Verified |
| gRPC Contract | Complete |
| C++ gRPC Server | Complete |
| Go API Layer | Complete |
| Integration Testing | Complete |
| Containerisation (Docker) | Complete |
| Kubernetes Deployment | Complete |

---

## What This Demonstrates

- Backend system design from first principles
- Crash-consistent storage with write-ahead logging
- Clean architectural boundaries enforced at the language level
- Schema-first API design with gRPC and protobuf
- Production-like durability reasoning (CRC checksums, atomic writes)
- Spaced repetition algorithm implementation and stability analysis
- Layered C++ architecture: model → scheduler → service → persistence → gRPC
- Go API layer with gRPC client integration
- Docker containerisation with volume-backed persistence
- Kubernetes multi-container Pod deployment with shared network namespace