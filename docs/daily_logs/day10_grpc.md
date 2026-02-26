# Day 10 — gRPC Contract Definition
**Date: 26 February 2026**

---

## Goal

Define the gRPC contract between Go and C++ using a `.proto` file. Understand what the contract contains, why each field exists, and how the service boundary is enforced through schema-first design.

---

## What is a .proto File

A `.proto` file defines:
1. **Messages** — the data structures exchanged between services (request and response)
2. **Service** — the remote functions that can be called

Every field has a number (e.g. `= 1`, `= 2`). This is how protobuf identifies fields in binary format on the wire — not by name. This makes the contract versionable and backward compatible.

---

## Why gRPC Was Chosen (Recap)

- Strong schema definition — contract is explicit and enforced
- Code generation for both C++ and Go from a single `.proto` file
- Versionable contracts — field numbers preserve compatibility
- Clear service boundary — Go cannot call anything not defined in the contract
- Prevents Go from interfering with scheduling logic

---

## Contract Design

### Operations Defined

| Operation | Purpose |
|-----------|---------|
| `ReviewComplete` | User finishes reviewing a card — triggers scheduling update |
| `AddCard` | Adds a new flashcard for a user |
| `GetDueCards` | Returns cards due for review on a given date |
| `CreateTopic` | Creates a new topic grouping |

---

## Message Definitions

### ReviewComplete
```protobuf
message ReviewCompleteRequest {
    int32 user_id = 1;
    int32 card_id = 2;
    int32 rating  = 3;
}

message ReviewCompleteResponse {
    int32 next_review_date = 1;
}
```
Request carries only facts — no decisions. C++ computes the next review date and returns it.

### AddCard
```protobuf
message AddCardRequest {
    int32 user_id  = 1;
    int32 card_id  = 2;
    int32 topic_id = 3;
}

message AddCardResponse {
    bool success = 1;
}
```

### GetDueCards
```protobuf
message GetDueCardsRequest {
    int32 user_id  = 1;
    int32 date     = 2;
    int32 topic_id = 3;
}

message GetDueCardsResponse {
    repeated int32 card_ids = 1;
}
```
`repeated` means a list — protobuf's way of expressing arrays.
`topic_id` is optional — allows filtering by topic or returning all due cards.

### CreateTopic
```protobuf
message CreateTopicRequest {
    int32  user_id  = 1;
    int32  topic_id = 2;
    string name     = 3;
}

message CreateTopicResponse {
    bool success = 1;
}
```

---

## Full Service Definition

```protobuf
service FlashcardService {
    rpc CreateTopic   (CreateTopicRequest)    returns (CreateTopicResponse);
    rpc ReviewComplete(ReviewCompleteRequest) returns (ReviewCompleteResponse);
    rpc AddCard       (AddCardRequest)        returns (AddCardResponse);
    rpc GetDueCards   (GetDueCardsRequest)    returns (GetDueCardsResponse);
}
```

---

## Boundary Enforced by This Contract

- Go can only call operations defined in `FlashcardService`
- Go never sees internal scheduling state
- Go never computes a field — it only sends facts and receives results
- C++ owns all decisions — the response carries the outcome, not the logic

---

## Tooling Decision

Full gRPC wiring on Windows (installing grpc C++ plugin, Go plugin, compiling stubs) was deferred to avoid tooling overhead consuming a full session. The `.proto` contract is the most important deliverable — it defines the boundary clearly regardless of whether the plumbing is wired yet.

`protoc` was installed and verified:
```
libprotoc 34.0
```

---

## Architecture Status After Today

| Layer | Status |
|-------|--------|
| Engine Layer | Complete |
| Scheduling Logic | Complete |
| Service Layer | Complete |
| Persistence Layer | Complete |
| End-to-End Wiring | Complete |
| gRPC Contract (.proto) | Complete |
| gRPC Server Wiring (C++) | Deferred |
| Go API Layer | Next |
| Integration Testing | Not Started |

---

## Next Session

- Start Go API layer — HTTP server setup
- Define routes matching the gRPC contract
- Wire Go to call C++ engine via gRPC client

---

## Reflection

Today the contract between Go and C++ became explicit and versioned. Writing the `.proto` file forced clarity on exactly what data crosses the boundary — nothing more, nothing less. Every field has a reason. Every response carries only what the caller needs.

This is schema-first thinking applied in practice.
