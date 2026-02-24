# Day 1 — Architecture Responsibilities
**Date: 9 February 2026**

---

## Goal

Move beyond data modeling and define system structure. Decide how to split responsibilities between C++ and Go, and define what crosses the boundary between them. No code was written intentionally.

---

## C++ Core Responsibilities

- Spaced repetition logic and scheduling decisions
- Enforcing review rules and maintaining algorithm correctness
- Permanent ownership of flashcard data and review history
- Must never depend on Go for data correctness

---

## Go API Layer Responsibilities

- Exposing HTTP APIs and handling user interaction
- Collecting review feedback and forwarding it to the C++ core
- Strictly forbidden from making scheduling decisions
- If Go crashes, scheduling correctness must still hold

---

## Interface Between Go and C++

**Input to C++ on review:**
- Flashcard identifier
- Timestamp of review
- Difficulty feedback

No decisions are sent — only facts.

**Output from C++:**
- The scheduling decision produced by the spaced repetition logic

The core decides what happens, not how it is displayed.

---

## Core Invariant

The C++ core must never allow external components to modify or bypass the spaced repetition algorithm. All scheduling decisions are enforced centrally.

---

## Key Takeaways

- Language choice must follow responsibility, not preference
- Core logic and orchestration must remain in separate layers
- Interfaces should exchange facts, not decisions
- Clear boundaries make systems easier to reason about in interviews