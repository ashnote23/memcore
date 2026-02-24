# Day 7 — Service Layer: Validation & Orchestration
**Date: 21 February 2026**

---

## Goal

Introduce a service layer between main/API and the scheduler that handles validation and orchestration, keeping the scheduler focused purely on algorithm logic.

---

## ReviewService Responsibilities

- Check if user exists before forwarding to scheduler
- Check if card exists before processing a review
- Validate rating range 0–3 — reject invalid input before it reaches the algorithm
- Delegate all scheduling decisions to Scheduler — never compute them directly

---

## Call Flow After This Change

```
main → ReviewService → Scheduler → Model
```

Previously everything was mixed. Now it is cleanly layered.

---

## Folder Ownership Decision

`ReviewService` belongs inside the domain layer alongside the scheduler — not in transport or API. It sits between the application entry point and the scheduling engine.

---

## Rating Validation

```cpp
if (rating < 0 || rating > 3) {
    throw std::invalid_argument("Rating must be between 0 and 3");
}
```

Invalid ratings never reach the scheduler. Algorithm correctness is protected at the boundary.

---

## Key Principle

> Scheduler does logic. Service does validation and orchestration.

That separation makes both components independently testable and clearly reasoned about.

---

## Architecture Status After Today

| Layer | Status |
|-------|--------|
| Engine Layer | Complete |
| Scheduling Logic | Complete |
| Service Layer | Complete |
| Persistence Layer | Not Started |
| gRPC Contract | Not Started |
| Go API Layer | Not Started |