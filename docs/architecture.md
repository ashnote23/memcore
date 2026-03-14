# memcore — System Architecture

---

## Overview

memcore is a spaced repetition backend engine built as two separate processes — a C++ scheduling engine and a Go HTTP API layer — communicating over gRPC, fully containerised with Docker and deployable on Kubernetes. The system is designed around correctness under failure, strict ownership boundaries, and deterministic scheduling logic.

This document covers the full system design: why each layer exists, what it owns, how layers communicate, and what guarantees the system provides.

---

## System Diagram

```
┌─────────────────────────────────────────────────────┐
│                     Client                          │
│              (curl / HTTP consumer)                 │
└──────────────────────┬──────────────────────────────┘
                       │ HTTP + JSON
                       │ port 8080
┌──────────────────────▼──────────────────────────────┐
│               Go API Container                      │
│                                                     │
│  POST /review       GET /due-cards                  │
│  POST /card         POST /topic                     │
│                                                     │
│  • HTTP routing and JSON parsing                    │
│  • Request validation                               │
│  • gRPC client — forwards to C++                    │
│  • CPP_ENGINE_HOST env var for container routing    │
└──────────────────────┬──────────────────────────────┘
                       │ gRPC (protobuf)
                       │ port 50051
┌──────────────────────▼──────────────────────────────┐
│               C++ Engine Container                  │
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │            ReviewService                    │   │
│  │   validation + orchestration                │   │
│  └──────────────────┬──────────────────────────┘   │
│                     │                               │
│  ┌──────────────────▼──────────────────────────┐   │
│  │              Scheduler                      │   │
│  │   SM-2 algorithm + card state               │   │
│  └──────────────────┬──────────────────────────┘   │
│                     │                               │
│  ┌──────────────────▼──────────────────────────┐   │
│  │              Storage                        │   │
│  │   /app/data/snapshot.bin                    │   │
│  │   /app/data/review.log                      │   │
│  └──────────────────┬──────────────────────────┘   │
└─────────────────────┼───────────────────────────────┘
                      │ volume mount
┌─────────────────────▼───────────────────────────────┐
│              Host Machine ./data/                   │
│         snapshot.bin + review.log                   │
│         (persists across container restarts)        │
└─────────────────────────────────────────────────────┘
```

---

## Layer Responsibilities

### Go API Layer

**Owns:**
- HTTP transport and JSON serialization
- Route registration and request parsing
- Input forwarding to C++ via gRPC

**Does not own:**
- Any scheduling logic
- Any card state
- Any storage concerns

Go is a thin translation layer. It receives HTTP requests, validates basic input, and forwards to C++. Every scheduling decision is made by the C++ engine.

**Why Go for this layer:**
Go has a built-in HTTP server, clean JSON handling, and excellent gRPC client support. It is well suited for API transport work where the business logic lives elsewhere.

---

### C++ Engine

**Owns:**
- All card state and mutations
- SM-2 scheduling algorithm
- Domain invariants
- Persistence and crash recovery

**Does not own:**
- HTTP transport
- JSON serialization
- Request routing

**Why C++ for this layer:**
C++ provides direct memory control, no garbage collection pauses, and is the natural choice for a performance-sensitive scheduling engine.

---

### Architectural Boundary

The boundary between Go and C++ is enforced by the gRPC contract. Go can only call operations defined in `proto/flashcards.proto`. There is no shared memory, no direct function calls, no way for Go to bypass the contract.

```
Go                          C++
─────────────────           ──────────────────────
HTTP handler          →     gRPC server method
builds gRPC request   →     calls ReviewService
receives response     ←     returns result
serializes to JSON    ←
```

---

## C++ Internal Architecture

The C++ engine is layered into three components:

```
ReviewService       ← validation + orchestration
      ↓
Scheduler           ← SM-2 algorithm + state
      ↓
Storage             ← persistence
```

### Model Layer

Defined in `core/model.h`:

```cpp
struct Card {
    CardId id;
    TopicId topicId;
    double easeFactor = 1.3;
    int interval = 0;
    int repetitions = 0;
    Date nextReviewDate = 0;
};

struct User {
    UserId id;
    unordered_map<CardId, Card> cards;
    unordered_map<TopicId, Topic> topics;
    priority_queue<pair<Date, CardId>, vector<...>, greater<>> dueQueue;
};
```

The due queue is a min-heap ordered by `nextReviewDate`. This gives O(log n) insert and O(1) access to the next due card.

---

### Scheduler

Owns all card state. Enforces SM-2 scheduling logic. Exposes a read-only view via `get_users() const` for the persistence layer.

**Key methods:**
```cpp
void create_user(UserId uid);
void create_topic(UserId uid, Topic topic);
void add_card(UserId uid, Card card);
void review_complete(UserId uid, CardId cid, int rating);
vector<CardId> get_due_cards(UserId uid, Date date, TopicId topicId = -1);
const unordered_map<UserId, User>& get_users() const;
```

---

### ReviewService

Sits between the entry point and the scheduler. Validates input before forwarding — invalid ratings, unknown users, unknown cards are all caught here and never reach the scheduler.

```cpp
class ReviewService {
private:
    Scheduler& scheduler;   // reference — shared instance, not owned
public:
    ReviewService(Scheduler& schedulerRef);
    void review_card(UserId uid, CardId cid, int rating);
    void add_card(UserId uid, CardId cid, TopicId tid);
    void create_topic(UserId uid, TopicId tid, const string& name);
    vector<CardId> get_due_cards(UserId uid, Date date, TopicId topicId = -1);
    Card get_card(UserId uid, CardId cid);
};
```

`ReviewService` takes `Scheduler&` by reference — one shared scheduler instance across the whole program. No duplicate state.

---

### Storage

Handles all file I/O. Reads from and writes to two files. File paths are configurable via environment variables — supports both local development and Docker volume mounts.

```cpp
class Storage {
public:
    Storage(string snapshotPath, string logPath);
    void save_snapshot(const unordered_map<UserId, User>& users);
    void load_snapshot(Scheduler& scheduler);
    void append_log(UserId uid, CardId cid, int rating, int timestamp);
    void replay_log(Scheduler& scheduler);
};
```

---

## SM-2 Scheduling Algorithm

### Rating Scale

| Rating | Meaning |
|--------|---------|
| 0 | Complete blackout |
| 1 | Hard |
| 2 | Medium |
| 3 | Easy |

### Interval Rules

```
Rating 0-1 (failure):
  repetitions = 0
  interval = 1

Rating 2-3 (success):
  repetition 1 → interval = 1
  repetition 2 → interval = 6
  repetition 3+ → interval = previous_interval × ease_factor
```

### Ease Factor Formula

```
EF' = EF + (0.1 − (3 − q)(0.08 + (3 − q) × 0.02))
EF  = max(1.3, EF')
```

### Why EF Floor = 1.3

Without the floor, repeated low ratings drive EF below 1.0. This causes intervals to shrink geometrically — the system collapses into a permanent short-review loop.

The scheduler behaves as a discrete dynamical system:
```
I(n+1) = I(n) × EF
```

For the system to be stable, EF must remain above 1.0. The 1.3 floor guarantees:
- Minimum 30% interval growth per successful review
- No unstable fixed point near zero
- Long-term spacing always increases

---

## Persistence Design

### Why Snapshot + Log

| Approach | Problem |
|----------|---------|
| Snapshot only | Loses reviews since last checkpoint |
| Log only | Full replay on every startup — O(n) growth |
| Snapshot + Log | Fast startup + zero data loss |

On startup: load snapshot (reconstruct state) → replay log (fill gap since checkpoint).

### Snapshot Format

```
[card_count : int32]
per card:
  [userId : int32]
  [cardId : int32]
  [topicId : int32]
  [easeFactor : double]
  [interval : int32]
  [repetitions : int32]
  [nextReviewDate : int32]
```

Count header written first — reader knows exactly how many records to expect.

### Log Format

```
per record:
  [userId : int32]
  [cardId : int32]
  [rating : int32]
  [timestamp : int32]
  [CRC32 : uint32]
```

No count header — records are appended indefinitely. Reading stops at EOF or first CRC mismatch.

### CRC32 Crash Safety

A crash mid-write leaves a partial record at the end of the log. On recovery:

```
expected = crc32(data fields only)
if expected != record.crc → partial write → stop replay
```

CRC is computed over the four data fields only — never over the CRC field itself (circular).

### Log Cleared After Snapshot

After `save_snapshot` completes, the log is truncated. This prevents events already baked into the snapshot from being replayed again on the next startup.

---

## gRPC Contract

Defined in `proto/flashcards.proto`. This is the single source of truth for the Go/C++ interface.

```protobuf
syntax = "proto3";
package flashcards;

service FlashcardService {
    rpc ReviewComplete (ReviewCompleteRequest)  returns (ReviewCompleteResponse);
    rpc AddCard        (AddCardRequest)         returns (AddCardResponse);
    rpc GetDueCards    (GetDueCardsRequest)      returns (GetDueCardsResponse);
    rpc CreateTopic    (CreateTopicRequest)      returns (CreateTopicResponse);
}

message ReviewCompleteRequest {
    int32 user_id = 1;
    int32 card_id = 2;
    int32 rating  = 3;
}
message ReviewCompleteResponse {
    int32 next_review_date = 1;
}

message AddCardRequest {
    int32 user_id  = 1;
    int32 card_id  = 2;
    int32 topic_id = 3;
}
message AddCardResponse {
    bool success = 1;
}

message GetDueCardsRequest {
    int32 user_id  = 1;
    int32 date     = 2;
    int32 topic_id = 3;
}
message GetDueCardsResponse {
    repeated int32 card_ids = 1;
}

message CreateTopicRequest {
    int32  user_id  = 1;
    int32  topic_id = 2;
    string name     = 3;
}
message CreateTopicResponse {
    bool success = 1;
}
```

Stubs are generated for both languages:
- C++: `core/proto/flashcards.pb.h`, `core/proto/flashcards.grpc.pb.h`
- Go: `api/proto/flashcards.pb.go`, `api/proto/flashcards_grpc.pb.go`

---

## Docker Architecture

### Container Design

Two containers, one network:

```
┌─────────────────────┐     gRPC      ┌─────────────────────┐
│   go-api            │ ────────────→ │   cpp-engine        │
│   port 8080         │               │   port 50051        │
└─────────────────────┘               └─────────────────────┘

Volume: ./data → /app/data
```

### Container Networking

Each container has its own network namespace — `localhost` inside Go refers to the Go container, not C++. Docker Compose creates an internal DNS where containers find each other by service name:

```
cpp-engine:50051
```

### Environment-Driven Host Resolution

```go
host := os.Getenv("CPP_ENGINE_HOST")
if host == "" {
    host = "localhost"  // local development
}
// Docker Compose sets CPP_ENGINE_HOST=cpp-engine
```

One codebase works in both environments — no code changes needed between local and Docker.

### Volume-Backed Persistence

```yaml
volumes:
  - ./data:/app/data
```

`snapshot.bin` and `review.log` live on the host machine, not inside the container. Containers can be destroyed and recreated — data survives.

### Two-Stage Go Build

```dockerfile
FROM golang:1.25 AS builder   # large image — has compiler
RUN go build -o api .

FROM debian:bookworm-slim      # small image — just the binary
COPY --from=builder /app/api .
```

Final Go image is ~20MB instead of ~800MB.

### Proto Regeneration in C++ Container

Pre-generated stubs from vcpkg are version-specific and incompatible with Ubuntu's system protobuf. Stubs are regenerated inside the container using its own protoc — guarantees version compatibility.

```dockerfile
RUN protoc --cpp_out=core \
           --grpc_out=core \
           --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin \
           proto/flashcards.proto
```

---

## Kubernetes Architecture

### Pod Design

Both containers run inside a single Pod. Containers in the same Pod share the same network namespace — they communicate over `localhost`, not over a Kubernetes Service.

```
┌──────────────────────────────────────────────────┐
│                  memcore Pod                     │
│                                                  │
│  ┌───────────────┐   gRPC    ┌────────────────┐  │
│  │    go-api     │──────────▶│   cpp-engine   │  │
│  │   port 8080   │ localhost │   port 50051   │  │
│  └───────────────┘           └────────────────┘  │
│                                                  │
│  Shared network namespace                        │
│  CPP_ENGINE_HOST=localhost                       │
└──────────────────────────────────────────────────┘
```

### Why One Pod and Not Two

Splitting Go and C++ into separate Pods would require a ClusterIP Service for intra-pod gRPC communication — adding DNS resolution and a network hop where none is needed. The architectural boundary between Go and C++ is a logical boundary, not a deployment boundary. One Pod expresses that correctly.

### Key Difference from Docker Compose

| Environment | How Go finds C++ |
|---|---|
| Local dev | `localhost:50051` (default) |
| Docker Compose | `cpp-engine:50051` (service name DNS) |
| Kubernetes Pod | `localhost:50051` (shared network namespace) |

The `CPP_ENGINE_HOST` environment variable handles all three cases without code changes.

### Manifest

```yaml
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
kubectl apply -f k8s/memcore-pod.yaml
kubectl port-forward pod/memcore 8080:8080

POST /topic     → {"success":true}
POST /card      → {"success":true}
GET  /due-cards → {"card_ids":[1]}
POST /review    → {"next_review_date":1}
```

---

## Startup Sequence

```
1. Create Scheduler
2. Create Storage (paths from environment variables)
3. if snapshot exists:
       load_snapshot(scheduler)   → reconstruct card state
       replay_log(scheduler)      → reapply events since checkpoint
   else:
       create topics and cards    → first run setup
4. Create ReviewService(scheduler)
5. Run simulation / start gRPC server
6. On shutdown:
       save_snapshot()            → checkpoint full state
       log cleared automatically
```

---

## Key Design Decisions

### Why not a database?

A database would introduce a dependency, a query language, and a runtime that adds complexity without benefit for this use case. The data model is simple and known at compile time. A custom binary format gives full control over the layout and crash recovery semantics.

### Why snapshot + log instead of snapshot alone?

A snapshot alone loses any reviews that happen after the last checkpoint. The log fills that gap. Together they give both fast startup and zero data loss.

### Why gRPC instead of shared memory or raw TCP?

gRPC gives a strongly typed, versioned contract between Go and C++. Shared memory would couple the processes tightly and require manual synchronization. Raw TCP would require defining a custom protocol. gRPC generates the serialization and deserialization code automatically from the `.proto` file.

### Why does ReviewService take Scheduler by reference?

If `ReviewService` owned its own `Scheduler` instance, the `Storage` layer and `ReviewService` would be operating on different schedulers with different state. One shared reference guarantees a single source of truth for all card state.

### Why is the log truncated after a snapshot?

If the log is not cleared, events already baked into the snapshot will be replayed again on the next startup — double-applying reviews and causing scheduling state to drift forward incorrectly.

### Why environment variables for file paths and host names?

Hardcoded paths and hostnames break when moving between environments — local development, Docker, or Kubernetes. Environment variables make the system configurable without code changes.

### Why run Go and C++ in the same Kubernetes Pod?

Containers in the same Pod share a network namespace — Go reaches C++ on `localhost:50051` with no Service or DNS overhead. The gRPC boundary between Go and C++ is a logical boundary enforced by the contract, not a network boundary requiring separate Pods. Splitting them would add infrastructure complexity that serves no architectural purpose at this stage.

---

## System Guarantees

| Guarantee | Mechanism |
|-----------|-----------|
| No data loss on crash | Append-only log written before in-memory state changes |
| No corrupt snapshot on crash | Log cleared only after snapshot write completes |
| No partial log replay | CRC32 per record — mismatch stops replay |
| No scheduling bypass | gRPC contract enforced — Go cannot call C++ directly |
| Deterministic scheduling | SM-2 is purely functional given the same inputs |
| No state duplication | Single Scheduler instance shared via reference |
| Persistence across restarts | Volume-mounted data directory on host machine |
| Environment portability | All paths and hostnames configurable via env vars |
| Kubernetes portability | Single Pod manifest — runs on any K8s cluster with local images loaded |