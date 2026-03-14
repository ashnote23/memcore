# Day 16 — Multi-Container Pod on Kubernetes
**Date: 14 March 2026**

---

## Goal

Run both the Go API layer and C++ engine together inside a single Kubernetes Pod, restore the gRPC connection between them, and verify all four HTTP endpoints serve real traffic through Kubernetes.

---

## Why Multi-Container Pod

Yesterday the Go binary started but nothing listened on port 8080. The root cause was clear: Go connects to C++ via gRPC on startup. Without the C++ engine running alongside it, Go cannot complete initialisation.

In Docker Compose this is solved by a shared internal network and service name DNS (`cpp-engine:50051`). In Kubernetes, the equivalent is a multi-container Pod — two containers sharing the same network namespace, same `localhost`, same lifecycle.

The architectural boundary between Go and C++ is a logical boundary enforced by the gRPC contract. It is not a deployment boundary. One Pod expresses that correctly.

---

## Key Concept — Shared Network Namespace

Containers in the same Pod share a network namespace. This means:

- They share the same IP address
- They can reach each other on `localhost`
- No Service or DNS resolution needed for intra-pod communication

```
┌──────────────────────────────────────────────────┐
│                  memcore Pod                     │
│                                                  │
│  ┌───────────────┐   gRPC    ┌────────────────┐  │
│  │    go-api     │──────────▶│   cpp-engine   │  │
│  │   port 8080   │ localhost │   port 50051   │  │
│  └───────────────┘           └────────────────┘  │
│                                                  │
│  CPP_ENGINE_HOST=localhost                       │
└──────────────────────────────────────────────────┘
```

This is the same as local development — Go reaches C++ on `localhost:50051` by default. No code change needed. Only the environment variable changes.

---

## What Was Done

### 1. Loaded C++ Image into minikube

minikube runs its own Docker daemon. Images built locally must be explicitly loaded:

```bash
minikube image load memcore-c++:latest
minikube image ls | grep memcore
# docker.io/library/memcore-c++:latest
# docker.io/library/memcore-go:latest
```

### 2. Updated Pod Manifest

Replaced the single-container Pod from Day 15 with a multi-container Pod:

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: memcore
  labels:
    app: memcore
spec:
  containers:
    - name: cpp-engine
      image: memcore-c++:latest
      imagePullPolicy: Never
      ports:
        - containerPort: 50051

    - name: go-api
      image: memcore-go:latest
      imagePullPolicy: Never
      ports:
        - containerPort: 8080
      env:
        - name: CPP_ENGINE_HOST
          value: "localhost"
```

`CPP_ENGINE_HOST=localhost` — Go reaches C++ on `localhost:50051` inside the shared Pod network. This is the same default used in local development.

### 3. Deployed and Verified

```bash
kubectl delete pod memcore-go        # remove yesterday's single-container pod
kubectl apply -f k8s/memcore-pod.yaml
kubectl get pods

# NAME      READY   STATUS    RESTARTS   AGE
# memcore   2/2     Running   0          1m
```

`2/2` confirms both containers are running inside the same Pod.

### 4. End-to-End Traffic Verification

```bash
kubectl port-forward pod/memcore 8080:8080
```

```bash
# Create a topic
curl -X POST http://localhost:8080/topic \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"topic_id":100,"name":"Kubernetes"}'
# → {"success":true}

# Add a card
curl -X POST http://localhost:8080/card \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"card_id":1,"topic_id":100}'
# → {"success":true}

# Get due cards
curl -X GET http://localhost:8080/due-cards \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"date":0,"topic_id":100}'
# → {"card_ids":[1]}

# Review a card
curl -X POST http://localhost:8080/review \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"card_id":1,"rating":3}'
# → {"next_review_date":1}
```

All four endpoints returning correct responses. The full stack is live:

```
curl → port-forward → K8s Pod → Go HTTP → gRPC → C++ engine → WAL storage
```

---

## Key Difference from Docker Compose

| Environment | How Go finds C++ |
|---|---|
| Local dev | `localhost:50051` (default) |
| Docker Compose | `cpp-engine:50051` (service name DNS) |
| Kubernetes Pod | `localhost:50051` (shared network namespace) |

The `CPP_ENGINE_HOST` environment variable handles all three cases. No code change needed between environments — only configuration changes.

---

## CKAD Concepts Practiced Today

- Multi-container Pods — multiple containers in a single Pod spec
- Shared network namespace — intra-pod communication via localhost
- `env` in container spec — injecting environment variables
- `kubectl delete pod` — removing a running Pod
- `kubectl port-forward` — exposing a Pod port to localhost
- `READY 2/2` — understanding the ready count in `kubectl get pods`

---

## Files Changed

```
k8s/
└── memcore-pod.yaml     ← updated from single to multi-container Pod
README.md                ← added Kubernetes section
docs/architecture.md     ← added Kubernetes architecture section
```

---

## Architecture Status After Today

| Layer | Status |
|---|---|
| C++ Engine | Complete |
| Persistence | Complete |
| gRPC Contract | Complete |
| Go API Layer | Complete |
| Integration Testing | Complete |
| Containerisation (Docker) | Complete |
| Kubernetes — Multi-Container Pod | Complete |
| Kubernetes — Namespaces + Labels | Tomorrow |

---

## Reflection

The multi-container Pod works because the same design decision made on Day 1 — Go owns transport, C++ owns scheduling, gRPC enforces the boundary — maps cleanly onto the Kubernetes model. The boundary is logical, not physical. Kubernetes makes that explicit: one Pod, two containers, one shared network.

The `CPP_ENGINE_HOST` environment variable introduced on Day 14 for Docker Compose now serves a third purpose — it makes the same codebase run correctly in local development, Docker Compose, and Kubernetes without modification. A decision made for one environment turned out to be the right abstraction for all three.