# Day 6 — Core Engine Implementation
**Date: 17–18 February 2026**

---

## Goal

Implement the C++ core scheduling engine — multi-user support, SM-2 logic, heap-based due-card management, and full behavioral validation.

---

## Project Structure

```
core/
├── model.h              data models (User, Card, Topic)
├── scheduler.h          Scheduler interface
├── scheduler.cpp        SM-2 scheduling logic
├── review_service.h     service layer interface
└── review_service.cpp   validation + orchestration
```

---

## Core Data Design

### User
```cpp
unordered_map<CardId, Card> cards
unordered_map<TopicId, Topic> topics
priority_queue<pair<Date, CardId>, vector<...>, greater<>> dueQueue
```

### Card
```cpp
CardId id
TopicId topicId
double easeFactor = 1.3
int interval = 0
int repetitions = 0
Date nextReviewDate = 0
```

---

## Heap Design — Key Architecture Decision

**Initially used:** `priority_queue<CardId>` with a stateful comparator dependent on an external cards map. This created constructor instability and pointer coupling.

**Refactored to:** `priority_queue<pair<Date, CardId>, vector<...>, greater<>>`

**Why this is better:**
- Self-contained — no external dependency
- Ordered by `nextReviewDate` directly
- No pointer coupling
- Clean and scalable

---

## Scheduling Behavior Verified

With constant rating = 3, observed interval growth:

```
1 → 6 → 9 → 15 → 27
```

Ease factor increased gradually: `1.4 → 1.5 → 1.6 → 1.7 → 1.8`

Behavior is monotonic, stable, and independently evolving per card.

---

## STL Lessons Learned

- `unordered_map::operator[]` requires default constructors — container behavior directly affects class design
- Default parameters belong only in header declarations, never in definitions
- Heap removal requires full rebuild since `priority_queue` has no arbitrary removal

---

## Architecture Status After Today

| Layer | Status |
|-------|--------|
| Engine Layer | Stable |
| Heap Structure | Refactored |
| Scheduling Logic | Verified |
| Model Layer | Cleaned |
| Service Layer | Not Started |
| Persistence Layer | Not Started |