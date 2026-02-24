# memcore

A crash-safe spaced repetition engine built in C++, with a Go API layer.

This is not a flashcard app. It is a backend storage engine that happens to schedule flashcards — built with the same reasoning you would apply to a production system: write-ahead logging, snapshot recovery, strict architectural boundaries, and deterministic scheduling logic.

---

## Why This Exists

Most spaced repetition tools are wrappers around a database with some scheduling logic sprinkled in. This project treats the scheduling engine as the core and builds everything else around it — the same way a database treats its storage engine as the core.

The goal was to learn how to think about backend systems properly: not just make something work, but make something that is correct under failure, has clear ownership boundaries, and can be reasoned about at every layer.

---

## Architecture

```
Client
  ↓
Go API Layer          (HTTP, JSON, request validation)
  ↓  gRPC
C++ Engine            (scheduling logic, invariants, mutations)
  ↓
File Storage          (snapshot + append-only review log)
```

**C++ owns:**
- The SM-2 scheduling algorithm
- All card state and mutations
- Persistence and crash recovery
- Domain invariants

**Go owns:**
- HTTP routing and JSON serialization
- Input validation before forwarding to C++
- Future authentication and multi-user routing

The boundary is enforced strictly. Go never computes a scheduling decision. C++ never handles a request.

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
[card_count : int]
[userId][cardId][topicId][easeFactor][interval][repetitions][nextReviewDate]
...
```

```
[userId][cardId][rating][timestamp][CRC32]
...
```

**On startup:**
1. Load snapshot → reconstruct in-memory card state
2. Replay review log → reapply events since last checkpoint
3. Rebuild due-card heap from reconstructed state

**Crash safety:**
- Snapshots are written to a temp file, fsynced, then atomically renamed — a crash mid-write never corrupts the live snapshot
- Each log record includes a CRC32 checksum — a partial write from a crash is detected on replay and discarded safely
- The log has no count header — reading stops at EOF or the first CRC mismatch

---

## Project Structure

```
memcore/
├── core/
│   ├── model.h                 data models (User, Card, Topic)
│   ├── scheduler.h             scheduler interface
│   ├── scheduler.cpp           SM-2 scheduling logic
│   ├── review_service.h        service layer interface
│   ├── review_service.cpp      validation + orchestration
│   └── persistence/
│       ├── storage.h           persistence interface
│       └── storage.cpp         snapshot + log implementation
├── api/                        Go HTTP layer (in progress)
├── proto/                      gRPC definitions (in progress)
├── docs/
│   ├── daily_logs/             day-by-day design decisions
│   └── architecture.md         full system design document
└── main.cpp                    simulation and testing
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

---

## Build & Run

```bash
# Compile
g++ -std=c++17 main.cpp core/scheduler.cpp core/review_service.cpp core/persistence/storage.cpp -o memcore

# Run simulation
./memcore
```

> Go API layer and gRPC wiring are in progress.

---

## Status

| Layer | Status |
|-------|--------|
| C++ Scheduling Engine | Complete |
| Service Layer | Complete |
| Persistence Layer | Complete |
| gRPC Contract | In Progress |
| Go API Layer | In Progress |
| Integration Testing | Not Started |

---

## What This Demonstrates

- Backend system design from first principles
- Crash-consistent storage with write-ahead logging
- Clean architectural boundaries enforced at the language level
- Production-like durability reasoning (fsync, atomic rename, CRC)
- Spaced repetition algorithm implementation and stability analysis
- Layered C++ architecture: model → scheduler → service → persistence