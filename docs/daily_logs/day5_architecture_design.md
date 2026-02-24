# Day 5 — Architecture Design & Core API
**Date: 15–16 February 2026**

---

## Goal

Produce the full architecture design document and finalize the core C++ API surface before implementation begins.

---

## High-Level Architecture

```
Client
  ↓
Go API Layer          (HTTP, JSON, request validation)
  ↓  gRPC
C++ Engine            (scheduling logic, invariants, mutations)
  ↓
File Storage          (snapshot + append-only review log)
```

---

## Core C++ API

| Method | Responsibility |
|--------|---------------|
| `review(CardId, Rating)` | Updates scheduling state, appends log, triggers snapshot |
| `add_card(Card)` | Inserts card into snapshot state |
| `get_due_cards(Date, TopicId?)` | Returns due cards, optional topic filter |
| `create_topic(Topic)` | Registers a new topic entity |

---

## Persistence Strategy

**Snapshot File + Append-Only Review Log** chosen for:

- Fast startup — snapshot avoids replaying full history
- Zero data loss — log fills the gap since last snapshot
- Crash safety — temp file + atomic rename prevents snapshot corruption
- CRC per log record — detects partial writes on recovery

---

## Snapshot Strategy

Every N reviews:
1. Write full state to `snapshot.tmp`
2. `fsync(snapshot.tmp)`
3. `rename(snapshot.tmp → snapshot)` — atomic
4. `fsync` directory
5. Delete review log

---

## Domain Invariants (Engine-Enforced)

- `repetition_count ≥ 0`
- `ease_factor` within valid bounds
- `next_due_date` must move forward after review
- No direct field mutation from outside the engine

---

## Go ↔ C++ Boundary Rules

- Go never mutates card fields or calculates scheduling
- Go never touches storage
- Go only sends `ReviewRequest { card_id, rating }`
- Engine responds with updated scheduling state

---

## Why This Is Production-Like

- Write-ahead logging
- Snapshotting with atomic rename
- Crash consistency
- Durability semantics
- Explicit invariant ownership

This is not a CRUD app. It is a miniature storage engine.