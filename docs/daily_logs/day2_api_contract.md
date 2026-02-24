# Day 2 — API Contract & Ownership Boundaries
**Date: 10 February 2026**

---

## Goal

Finalize the ownership boundary between C++ and Go by defining the core API contract and making the invariants explicit and enforceable.

---

## Core API Defined

**API Name:** `ReviewComplete`

**Meaning:** A user has finished reviewing a flashcard.

**Inputs:**
- FlashcardId
- Timestamp
- DifficultyRating (0–3)

**Output:**
- Updated scheduling state for that flashcard

---

## System Laws

- Go must never interfere with or infer scheduling logic
- The C++ core is the single source of truth for spaced repetition behavior
- When Go calls `ReviewComplete`, C++ guarantees a deterministic schedule update

---

## Key Realization

Storage concerns and IPC concerns are separate layers. A strong interface protects internal implementation changes — future storage growth does not require changing the API contract.

---

## Reflection

Today helped me think in terms of interfaces and ownership, not just features. This is the kind of reasoning I want to demonstrate in backend interviews — clear boundaries, intentional design, and defensible decisions.