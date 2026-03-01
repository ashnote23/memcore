# Day 12 — Full End-to-End Integration
**Date: 1 March 2026**

---

## Goal

Complete the full system integration — C++ gRPC server, Go gRPC client wiring, and verified end-to-end flow from HTTP request through Go, gRPC, C++ scheduling engine, and back.

---

## What Was Built Today

The complete system is now working end-to-end:

```
curl (HTTP request)
  ↓
Go HTTP server        (port 8080)
  ↓  gRPC call
C++ Engine            (port 50051)
  ↓
SM-2 Scheduler        (real scheduling logic)
  ↓  gRPC response
Go HTTP server
  ↓
JSON response
```

---

## C++ gRPC Server

### FlashcardServiceImpl

Implemented all four gRPC service methods in `core/grpc_server.cpp`:

```cpp
class FlashcardServiceImpl final 
    : public flashcards::FlashcardService::Service {
private:
    ReviewService& reviewService;
public:
    FlashcardServiceImpl(ReviewService& rs) : reviewService(rs) {}
    ...
};
```

Takes `ReviewService&` by reference — one shared instance across the whole program. No duplicate state.

### ReviewComplete Method

```cpp
grpc::Status ReviewComplete(
    grpc::ServerContext* context,
    const flashcards::ReviewCompleteRequest* request,
    flashcards::ReviewCompleteResponse* response) override {
    
    reviewService.review_card(request->user_id(), request->card_id(), request->rating());
    Card card = reviewService.get_card(request->user_id(), request->card_id());
    response->set_next_review_date(card.nextReviewDate);
    return grpc::Status::OK;
}
```

Key points:
- Protobuf field accessors use `field_name()` with parentheses
- Every gRPC method must return `grpc::Status`
- Returns real scheduling data, not hardcoded values

### Server Startup in main.cpp

```cpp
grpc::ServerBuilder builder;
builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
FlashcardServiceImpl service(reviewService);
builder.RegisterService(&service);
std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
std::cout << "gRPC server listening on port 50051\n";
server->Wait();
```

---

## Go gRPC Client Wiring

Updated `handleReview` to call C++ via gRPC instead of returning hardcoded data:

```go
func (s *Server) handleReview(w http.ResponseWriter, r *http.Request) {
    if r.Method == "POST" {
        var req ReviewRequest
        json.NewDecoder(r.Body).Decode(&req)

        grpcReq := &pb.ReviewCompleteRequest{
            UserId: int32(req.UserId),
            CardId: int32(req.CardId),
            Rating: int32(req.Rating),
        }

        grpcRes, err := s.client.ReviewComplete(context.Background(), grpcReq)
        if err != nil {
            http.Error(w, err.Error(), http.StatusInternalServerError)
            return
        }

        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(ReviewResponse{
            NextReviewDate: int(grpcRes.NextReviewDate),
        })
    }
}
```

---

## get_card Added to ReviewService

To return the updated card state after a review:

```cpp
Card ReviewService::get_card(UserId uid, CardId cid) {
    const auto& users = scheduler.get_users();
    return users.at(uid).cards.at(cid);
}
```

Returns the actual card with all scheduling state — easeFactor, interval, repetitions, nextReviewDate.

---

## Build System — CMake + vcpkg

Switched from manual `g++` compilation to CMake with vcpkg integration.

`CMakeLists.txt` links:
- `gRPC::grpc++`
- `protobuf::libprotobuf`

vcpkg toolchain file used:
```
C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Version Mismatch Bug Fixed

The system `protoc` (v34.0) was incompatible with vcpkg protobuf (v6.33.4). Fixed by using vcpkg's own protoc to regenerate stubs:

```powershell
C:\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe ...
```

---

## End-to-End Verified

Tested with real SM-2 values changing across multiple requests:

```bash
curl -X POST http://localhost:8080/review \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"card_id":1,"rating":3}'
# → {"next_review_date":109}

# Second call
# → {"next_review_date":214}  (interval grew)

# Third call  
# → {"next_review_date":428}  (interval grew again)
```

Each review returns a real scheduling result from the SM-2 algorithm running in C++.

---

## Architecture Status After Today

| Layer | Status |
|-------|--------|
| Engine Layer | Complete |
| Scheduling Logic | Complete |
| Service Layer | Complete |
| Persistence Layer | Complete |
| End-to-End C++ Wiring | Complete |
| gRPC Contract (.proto) | Complete |
| gRPC Server (C++) | Complete |
| gRPC Client (Go) | Complete |
| Go API Layer | Complete |
| Integration Testing | Next |

---

## Next Session

- Wire remaining Go handlers (AddCard, GetDueCards, CreateTopic) to call C++ via gRPC
- End-to-end testing of all four endpoints
- Crash recovery testing — kill C++ mid-session and verify recovery

---

## Reflection

Today the entire system came together. Every layer built over the past weeks — C++ engine, persistence, service layer, gRPC contract, Go API — is now connected and working.

The most satisfying moment was seeing `next_review_date` change with each request, proving real SM-2 scheduling data is flowing from C++ through gRPC to the HTTP response.

This is not a toy project. This is a production-like backend system with crash safety, clean architectural boundaries, and a real scheduling algorithm running end to end.