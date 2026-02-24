# Day 3 — IPC Decision: Selecting gRPC
**Date: 13 February 2026**

---

## Goal

Finalize the inter-process communication mechanism between Go and C++ by reasoning from constraints rather than defaulting to a familiar tool.

---

## System Constraints Defined

- Go and C++ will run on the same machine
- They must run as separate processes
- They should scale independently in the future
- If C++ crashes, Go should continue running
- Strict boundary required between business logic and API layer
- Storage implementation in C++ must be changeable without breaking the interface

---

## Key Realizations

- Storage and IPC are separate layers — changing one should not affect the other
- What is needed is a strict, versioned, strongly defined contract
- Flexibility is not the same as architectural purity

---

## Final Decision: gRPC

Selected gRPC as the communication layer between Go and C++.

**Reasoning:**
- Strong schema definition via `.proto` files
- Code generation for both C++ and Go
- Versionable contracts — interface changes are explicit and intentional
- Clear service boundary prevents Go from interfering with scheduling
- Maintains architectural purity

---

## Reflection

Today moved from "choosing tools" to "deriving architecture from constraints." That shift in thinking is exactly what backend interviews require. The discomfort of not jumping straight to an answer was productive — it forced better reasoning.