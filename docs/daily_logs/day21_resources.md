# Day 21 — Resource Requests and Limits
**Date: 21 March 2026**

---

## Goal

Add CPU and memory resource requests and limits to both containers in the memcore Pod — giving Kubernetes the information it needs to schedule and constrain workloads correctly.

---

## Why Resource Constraints

Without resource constraints, containers can consume as much CPU and memory as the node has available. In a shared cluster, one misbehaving container can starve others. Kubernetes uses requests and limits to make scheduling decisions and enforce runtime constraints.

| | Request | Limit |
|---|---|---|
| Meaning | Minimum guaranteed | Maximum allowed |
| Used by scheduler | Yes — to find a suitable node | No |
| Enforced at runtime | No | Yes |
| Exceeds it → | Nothing | CPU throttled / OOMKilled |

---

## Memory vs CPU Behaviour at Limit

This distinction matters for debugging production issues:

**Memory limit exceeded:**
Container is OOMKilled (Out Of Memory Killed) — terminated immediately. Memory is a hard limit. Exceed it and the container dies.

**CPU limit exceeded:**
CPU is throttled — the container slows down but keeps running. CPU is a soft limit. Exceed it and the container slows.

---

## Units

**Memory:**
| Unit | Value |
|---|---|
| `Mi` | Mebibytes — 1 Mi = 1,048,576 bytes |
| `Gi` | Gibibytes — 1 Gi = 1,073,741,824 bytes |

Always use `Mi` and `Gi` in Kubernetes — not `M` and `G`. Binary units are the standard.

**CPU:**
```
125m  = 125 millicores = 0.125 of one CPU core
250m  = 250 millicores = 0.25 of one CPU core
500m  = 500 millicores = 0.5 of one CPU core
1000m = 1 full CPU core
```

---

## What Was Done

### Added Resources to Both Containers

```yaml
    - name: cpp-engine
      resources:
        requests:
          memory: "128Mi"
          cpu: "250m"
        limits:
          memory: "256Mi"
          cpu: "500m"

    - name: go-api
      resources:
        requests:
          memory: "64Mi"
          cpu: "125m"
        limits:
          memory: "128Mi"
          cpu: "250m"
```

**Why different values for each container:**
- C++ engine owns scheduling logic, WAL writes, and snapshot operations — heavier workload, higher allocation
- Go API layer is a thin translation layer — HTTP routing and gRPC forwarding only, lower allocation

### Verified via kubectl describe

```bash
kubectl describe pod memcore -n memcore | grep -A 4 "Limits"

# cpp-engine:
# Limits:
#   cpu:     500m
#   memory:  256Mi
# Requests:
#   cpu:     250m
#   memory:  128Mi

# go-api:
# Limits:
#   cpu:     250m
#   memory:  128Mi
# Requests:
#   cpu:     125m
#   memory:  64Mi
```

---

## CKAD Concepts Practiced Today

- `resources.requests` and `resources.limits` in container spec
- CPU units — millicores (`m`)
- Memory units — mebibytes (`Mi`), gibibytes (`Gi`)
- Difference between request and limit behaviour
- OOMKilled vs CPU throttling
- Why requests and limits differ per container based on workload

---

## kubectl Commands Learned

```bash
kubectl describe pod <n> | grep -A 4 Limits    # verify resource config
kubectl top pods -n <namespace>                  # live CPU/memory usage
kubectl top nodes                                # node level usage
```

---

## Files Changed

```
k8s/
└── memcore-pod.yaml     ← added resources to cpp-engine and go-api
```

---

## Architecture Status After Today

| Layer | Status |
|---|---|
| C++ Engine | Complete |
| Persistence | Complete |
| gRPC Contract | Complete |
| Go API Layer | Complete |
| Containerisation (Docker) | Complete |
| Kubernetes — Multi-Container Pod | Complete |
| Kubernetes — Namespaces + Labels | Complete |
| Kubernetes — ConfigMaps + Secrets | Complete |
| Kubernetes — PersistentVolumes + PVCs | Complete |
| Kubernetes — Liveness + Readiness Probes | Complete |
| Kubernetes — Resource Requests + Limits | Complete |
| Kubernetes — Deployments | Tomorrow |

---

## Reflection

Resource requests and limits make explicit what was always implicit — how much the system needs and how much it is allowed to use. The difference in allocation between the C++ engine and Go API layer reflects the same reasoning applied throughout this project: C++ owns the heavy work, Go is a thin transport layer. That architectural decision now has a concrete expression in the cluster's resource model.

The OOMKilled vs throttling distinction is not just exam knowledge — it changes how you debug production incidents. A container that keeps restarting with exit code 137 is OOMKilled. A container that is slow but running is CPU throttled. Knowing which one you're dealing with determines the fix.