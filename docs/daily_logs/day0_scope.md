# Day 0 — Problem & Scope Definition
**Date: 8 February 2026**

---

## Goal

Define the problem clearly before writing any code. Practice thinking like a backend interviewer by scoping the system, identifying use cases, and modeling data correctly.

---

## Problem Statement

The system helps users memorize and recognize patterns in interview-style questions using spaced repetition. The focus is daily, structured review without cramming.

The target user is a student or professional preparing for technical interviews who wants a lightweight, low-friction daily review tool.

Spaced repetition matters because it reinforces memory just before forgetting occurs, making limited study time far more effective than cramming.

---

## Core Use Cases

1. User creates flashcards containing interview questions and answers
2. User reviews flashcards that are due for the current day
3. User chooses to review a specific topic or performs a randomized review
4. User marks difficulty after reviewing, which influences future scheduling
5. Flashcards can have optional hints visible during review

---

## Non-Goals

1. The system will not use AI to independently decide what to review
2. Users cannot type free-text answers — recall is self-assessed
3. Users cannot manually choose review dates — scheduling is fully system-controlled

---

## Data Entities

- **Flashcard** — question and answer pair
- **Topic** — groups flashcards by subject area
- **ReviewHistory** — records each review attempt and its outcome
- **Notes** — stored as part of a topic, cannot exist independently

---

## Scale Assumptions

- Single-user system — simplifies data ownership and avoids permission complexity
- Up to 500 flashcards
- Around 15 reviews per day

---

## Key Design Principle Learned

> A flashcard does not change. A review happens to a flashcard.

This distinction led to separating long-lived identity data from short-lived review state — a foundational entity modeling insight.

---

## Design Constraints

- Read-heavy workload
- Deterministic scheduling logic
- Minimal UI assumptions — backend-first design
- Future extensibility without rearchitecting core