# Day 4 — Scheduler Logic: SM-2 Stability
**Date: 14 February 2026**

---

## Goal

Finalize scheduler logic using classic SM-2 and prove the mathematical stability of the algorithm before implementing it.

---

## Decisions Made

- Use classic SM-2 — no probabilistic scheduling, no adaptive heuristics
- 0–3 grading scale
- Small dataset makes a deterministic model sufficient
- Simplicity over optimization

---

## Grading Scale

| Rating | Meaning |
|--------|---------|
| 0 | Complete blackout |
| 1 | Hard |
| 2 | Medium |
| 3 | Easy |

---

## Scheduler Design

### Grade Handling
- `0–1` → failure → reset repetitions → interval = 1
- `2–3` → success → increment repetitions

### Interval Rules
- Repetition 1 → 1 day
- Repetition 2 → 6 days
- Repetition 3+ → `interval = previous_interval × ease_factor` (geometric progression)

### Ease Factor Formula
```
EF' = EF + (0.1 − (3 − q)(0.08 + (3 − q) × 0.02))
EF  = max(1.3, EF')
```

---

## Why EF Floor = 1.3 Is Necessary

Without the floor:
- Repeated low grades reduce EF below 1.0
- Intervals shrink geometrically
- Card gets trapped in a permanent short-review loop

With EF ≥ 1.3:
- Minimum 30% interval growth per successful review is guaranteed
- No unstable fixed point near zero
- System remains mathematically stable

The scheduler behaves as a discrete dynamical system:

```
I(n+1) = I(n) × EF
```

Long-term behavior depends entirely on EF. The 1.3 floor removes collapse scenarios and ensures eventual long-term spacing.

---

## Status

- Scheduler logic finalized and locked
- Stability reasoning understood and documented
- No further changes needed for current dataset size