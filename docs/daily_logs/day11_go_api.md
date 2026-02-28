# Day 11 — Go API Layer
**Date: 28 February 2026**

---

## Goal

Build the Go HTTP API layer from scratch — learning Go fundamentals, defining request/response structs, writing handler functions, and verifying all four endpoints work correctly.

---

## Go Fundamentals Learned Today

### Key Differences from C++

| C++ | Go |
|-----|----|
| Classes with methods | Structs with functions |
| Manual memory management | Garbage collector |
| No built-in HTTP | Built-in `net/http` package |
| Exceptions | Explicit error returns |

### Struct Tags
Go uses struct tags to map JSON field names to struct fields:

```go
type ReviewRequest struct {
    UserId int `json:"user_id"`
    CardId int `json:"card_id"`
    Rating int `json:"rating"`
}
```

The backtick syntax `` `json:"user_id"` `` tells Go's JSON parser which JSON key maps to which struct field.

### Exported vs Unexported Fields
In Go, fields starting with **uppercase** are exported (visible to JSON parser and other packages). Fields starting with **lowercase** are unexported and invisible to JSON encoding. All struct fields must start with uppercase to work with `encoding/json`.

### If/Else Syntax
The `else` must be on the same line as the closing brace:
```go
if condition {
    // ...
} else {
    // ...
}
```

### Unused Imports Are Errors
Go will refuse to compile if an import is declared but not used. Every import must be actively referenced.

---

## Request/Response Structs Defined

```go
type ReviewRequest struct {
    UserId int `json:"user_id"`
    CardId int `json:"card_id"`
    Rating int `json:"rating"`
}

type ReviewResponse struct {
    NextReviewDate int `json:"next_review_date"`
}

type AddCardRequest struct {
    UserId  int `json:"user_id"`
    CardId  int `json:"card_id"`
    TopicId int `json:"topic_id"`
}

type AddCardResponse struct {
    Success bool `json:"success"`
}

type GetDueCardsRequest struct {
    UserId  int `json:"user_id"`
    Date    int `json:"date"`
    TopicId int `json:"topic_id"`
}

type GetDueCardsResponse struct {
    CardIds []int `json:"card_ids"`
}

type CreateTopicRequest struct {
    UserId  int    `json:"user_id"`
    TopicId int    `json:"topic_id"`
    Name    string `json:"name"`
}

type CreateTopicResponse struct {
    Success bool `json:"success"`
}
```

`[]int` is a Go slice — the equivalent of a dynamic array. Used for returning a list of card IDs.

---

## Handler Pattern

Every handler follows the same structure:

```go
func handleReview(w http.ResponseWriter, r *http.Request) {
    if r.Method == "POST" {
        var req ReviewRequest
        json.NewDecoder(r.Body).Decode(&req)

        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(ReviewResponse{NextReviewDate: 42})
    } else {
        return
    }
}
```

Key points:
- `w http.ResponseWriter` — used to write the response back to the client
- `r *http.Request` — contains the incoming request (method, body, headers)
- `r.Method` — the HTTP method (GET, POST, etc.)
- `r.Body` — the request body containing JSON
- `&req` — passes address of req so decoder can write into it (same as C++ pointers)
- `Content-Type` header must be set before writing the response body

---

## HTTP Method Decisions

| Route | Method | Reason |
|-------|--------|--------|
| `/review` | POST | Changes scheduling state — has side effects |
| `/card` | POST | Creates a new card — has side effects |
| `/topic` | POST | Creates a new topic — has side effects |
| `/due-cards` | GET | Only retrieves data — no side effects |

---

## Routes Registered

```go
func main() {
    http.HandleFunc("/review",    handleReview)
    http.HandleFunc("/card",      handleAddCard)
    http.HandleFunc("/due-cards", handleGetDueCards)
    http.HandleFunc("/topic",     handleCreateTopic)
    http.ListenAndServe(":8080", nil)
}
```

---

## Endpoints Verified

All four endpoints tested and working:

```bash
# Review
curl -X POST http://localhost:8080/review \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"card_id":5,"rating":3}'
# → {"next_review_date":42}

# Add Card
curl -X POST http://localhost:8080/card \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"card_id":5,"topic_id":3}'
# → {"success":true}

# Create Topic
curl -X POST http://localhost:8080/topic \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"topic_id":2,"name":"sample"}'
# → {"success":true}

# Get Due Cards
curl -X GET http://localhost:8080/due-cards \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"date":2,"topic_id":3}'
# → {"card_ids":[1,2]}
```

---

## Current State of Handlers

Handlers currently return hardcoded responses. The next step is wiring them to the C++ engine via gRPC so they return real scheduling data.

---

## Architecture Status After Today

| Layer | Status |
|-------|--------|
| Engine Layer | Complete |
| Scheduling Logic | Complete |
| Service Layer | Complete |
| Persistence Layer | Complete |
| End-to-End Wiring (C++) | Complete |
| gRPC Contract (.proto) | Complete |
| Go API Layer (skeleton) | Complete |
| gRPC Wiring (Go ↔ C++) | Next |
| Integration Testing | Not Started |

---

## Next Session

- Wire Go handlers to call C++ engine via gRPC
- Replace hardcoded responses with real scheduling data
- End-to-end flow: HTTP request → Go → gRPC → C++ → response

---

## Reflection

Today was the first time writing Go. The language is cleaner than expected — explicit error handling, no classes, and a built-in HTTP server make it well suited for API layers. The struct tag system for JSON mapping is elegant and makes the request/response shapes self-documenting.

The handler pattern is consistent and easy to extend — adding a new endpoint means following the same structure every time.