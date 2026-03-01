# Day 11 — Go API Layer & gRPC Setup
**Date: 28 February 2026**

---

## Goal

Build the Go HTTP API layer from scratch, learn Go fundamentals, define request/response structs, write handler functions, verify all four endpoints work, and set up gRPC tooling to generate Go stubs.

---

## Go Fundamentals Learned Today

### Key Differences from C++

| C++ | Go |
|-----|----|
| Classes with methods | Structs with functions |
| Manual memory management | Garbage collector |
| No built-in HTTP | Built-in `net/http` package |
| Exceptions | Explicit error returns |
| Unused imports allowed | Unused imports are compile errors |

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
- Fields starting with **uppercase** are exported — visible to JSON parser and other packages
- Fields starting with **lowercase** are unexported — invisible to JSON encoding
- All struct fields must start with uppercase to work with `encoding/json`

### If/Else Syntax
The `else` must be on the same line as the closing brace:
```go
if condition {
    // ...
} else {
    // ...
}
```

### Pointer Syntax
`&req` passes the memory address of `req` so the decoder can write directly into it — same concept as pointers in C++.

---

## Go Module Initialized

```powershell
cd api
go mod init memcore/api
```

Created `go.mod` — Go's project configuration file that declares the module name and tracks dependencies.

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
func (s *Server) handleReview(w http.ResponseWriter, r *http.Request) {
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
- `w http.ResponseWriter` — writes the response back to the client
- `r *http.Request` — contains the incoming request (method, body, headers)
- `r.Method` — the HTTP method (GET, POST, etc.)
- `r.Body` — the request body containing JSON
- `Content-Type` header must be set before writing the response body

### Server Struct Pattern
Handlers are methods on a `Server` struct that holds the gRPC client:

```go
type Server struct {
    client pb.FlashcardServiceClient
}
```

This allows all handlers to share one gRPC connection — opening a new connection per request would be wasteful and slow.

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
    conn, err := grpc.Dial("localhost:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
    if err != nil {
        log.Fatalf("Failed to connect: %v", err)
    }
    defer conn.Close()

    client := pb.NewFlashcardServiceClient(conn)
    server := &Server{client: client}

    http.HandleFunc("/review",    server.handleReview)
    http.HandleFunc("/card",      server.handleAddCard)
    http.HandleFunc("/due-cards", server.handleGetDueCards)
    http.HandleFunc("/topic",     server.handleCreateTopic)
    http.ListenAndServe(":8080", nil)
}
```

---

## gRPC Tooling Setup

### Go Plugins Installed

```powershell
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
```

Verified:
```
protoc-gen-go v1.36.11
protoc-gen-go-grpc 1.6.1
```

### Go Stubs Generated

Added `go_package` option to `proto/flashcards.proto`:
```protobuf
option go_package = "memcore/api/proto";
```

Generated Go stubs:
```powershell
protoc --go_out=api --go_opt=paths=source_relative \
       --go-grpc_out=api --go-grpc_opt=paths=source_relative \
       proto/flashcards.proto
```

Generated files:
- `api/proto/flashcards.pb.go` — message types
- `api/proto/flashcards_grpc.pb.go` — gRPC service client and server interfaces

### gRPC Dependency Added

```powershell
go get google.golang.org/grpc
```

Updated `go.mod` with the dependency and created `go.sum` with cryptographic checksums.

**go.mod** — declares what packages the project needs and their versions.

**go.sum** — security file with checksums of every downloaded package. Same concept as CRC in the persistence layer — detects if a package has been tampered with.

---

## Endpoints Verified

All four endpoints tested and working:

```bash
# Review
curl -X POST http://localhost:8080/review \
  -d '{"user_id":1,"card_id":5,"rating":3}'
# → {"next_review_date":42}

# Add Card
curl -X POST http://localhost:8080/card \
  -d '{"user_id":1,"card_id":5,"topic_id":3}'
# → {"success":true}

# Create Topic
curl -X POST http://localhost:8080/topic \
  -d '{"user_id":1,"topic_id":2,"name":"sample"}'
# → {"success":true}

# Get Due Cards
curl -X GET http://localhost:8080/due-cards \
  -d '{"user_id":1,"date":2,"topic_id":3}'
# → {"card_ids":[1,2]}
```

---

## Architecture Status After Today

| Layer | Status |
|-------|--------|
| Engine Layer | Complete |
| Scheduling Logic | Complete |
| Service Layer | Complete |
| Persistence Layer | Complete |
| gRPC Contract (.proto) | Complete |
| Go stubs generated | Complete |
| Go HTTP server + routes | Complete |
| gRPC client connection | Complete |
| gRPC Server (C++) | Next |
| Wire handlers to C++ | Next |

---

## Next Session

- Build C++ gRPC server using vcpkg + CMake
- Wire Go handlers to call real C++ scheduling logic
- Verify end-to-end flow with real SM-2 values

---

## Reflection

Today was the first time writing Go. The language felt clean and explicit — built-in HTTP, consistent error handling, and struct tags for JSON mapping make API development straightforward. The handler pattern is consistent and easy to extend.

The gRPC tooling setup revealed an important lesson — generated code must always match the library version it was generated against. The `go_package` option and plugin versions both matter for compatibility.