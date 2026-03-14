# Day 15 — First Kubernetes Deployment
**Date: 13 March 2026**

---

## Goal

Deploy the memcore Go API container onto a local Kubernetes cluster for the first time. Understand how Kubernetes manages containers differently from Docker Compose, and identify what breaks at the boundary.

---

## Why Kubernetes After Docker

Docker Compose runs containers on a single machine. Kubernetes orchestrates containers across machines — handling restarts, scaling, networking, and scheduling automatically.

The goal is not to replace Docker Compose for local development. The goal is to understand how a production system would run memcore: self-healing, declaratively configured, and independent of any single machine.

---

## Environment

- Windows + Git Bash
- Docker Desktop (local image registry)
- minikube with Docker driver — single-node local Kubernetes cluster
- kubectl for cluster interaction
- winpty required for interactive terminal sessions in Git Bash on Windows

---

## Key Concept — Pod vs Container

In Docker, the unit of deployment is a container. In Kubernetes, the unit of deployment is a Pod.

A Pod wraps one or more containers that share:
- The same network namespace (same IP, same localhost)
- The same storage volumes

For memcore this matters immediately: Go and C++ must be in the same Pod so Go can reach C++ via `localhost:50051` — the same way they communicate inside Docker Compose's internal network.

---

## What Was Done

### 1. Project Structure

Created a `k8s/` folder inside memcore to hold all Kubernetes manifests:

```
memcore/
├── k8s/
│   └── memcore-pod.yaml     ← all Kubernetes manifests live here
├── core/
├── api/
├── docker-compose.yml
└── README.md
```

### 2. Loading the Local Image into minikube

minikube runs its own Docker daemon — separate from Docker Desktop. A locally built image is not automatically available inside minikube.

```bash
minikube image load memcore-go:latest
```

Without this step Kubernetes throws `ErrImagePull` — it tries Docker Hub, finds nothing, and fails.

### 3. Writing the Pod Manifest

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: memcore-go
  labels:
    app: memcore
    tier: api
spec:
  containers:
    - name: memcore-go
      image: memcore-go:latest
      imagePullPolicy: Never
      ports:
        - containerPort: 8080
```

`imagePullPolicy: Never` tells Kubernetes to use only what is already loaded locally — never attempt an external pull. Without this, Kubernetes defaults to pulling from Docker Hub even if the image exists locally.

### 4. Deploying and Verifying

```bash
kubectl apply -f k8s/memcore-pod.yaml
kubectl get pods

# NAME          READY   STATUS    RESTARTS   AGE
# memcore-go    1/1     Running   0          1m
```

### 5. Inspecting the Running Container

```bash
# Shell into the running Pod
winpty kubectl exec -it memcore-go -- sh

# Binary is present and correct size
ls -la
# -rwxr-xr-x 1 root root 17170245 Mar 9 11:47 api

# Go binary running as PID 1
cat /proc/1/cmdline | tr '\0' ' '
# ./api

# Nothing listening on any port
cat /proc/net/tcp
# (empty)
```

---

## Key Finding — Why HTTP Is Not Serving

The Go binary starts. Nothing listens on port 8080.

This is expected. The Go API layer connects to the C++ engine via gRPC on startup:

```
Go API → gRPC → cpp-engine:50051
```

Without the C++ container running alongside it, the Go service cannot complete initialisation. The connection attempt fails silently and no HTTP server starts.

In Docker Compose this was solved with `depends_on` and a shared internal network. In Kubernetes it is solved with a multi-container Pod — both containers share the same network namespace, so Go can reach C++ on `localhost:50051`.

This is tomorrow's problem.

---

## Issues Encountered

| Issue | Cause | Fix |
|---|---|---|
| `ErrImagePull` | Kubernetes tried Docker Hub | `minikube image load` + `imagePullPolicy: Never` |
| Hyper-V error on `minikube start` | Hyper-V not enabled on Windows | `minikube start --driver=docker` |
| `/bin/sh` path error on exec | Git Bash maps Unix paths to Windows | `winpty kubectl exec -it -- sh` |
| `wget` and `ps` not found | Minimal production image | Used `/proc/net/tcp` and `/proc/1/cmdline` instead |

---

## kubectl Commands Learned

```bash
kubectl apply -f <file>            # create or update resources from a manifest
kubectl get pods                   # list pods and their status
kubectl describe pod <name>        # full details — events, state, config
kubectl logs <name>                # container stdout
kubectl exec -it <name> -- sh      # interactive shell into running container
kubectl delete pod <name>          # remove a pod
```

---

## Concepts Introduced Today

**Declarative configuration** — `kubectl apply` does not tell Kubernetes what to do step by step. It declares desired state. Kubernetes figures out how to reach it.

**imagePullPolicy** — controls when Kubernetes fetches an image. `Never` means local only. `Always` means pull every time. `IfNotPresent` means pull only if missing.

**Labels** — key-value pairs attached to resources. Not used for routing yet but required later for Services to find the right Pods.

---

## Files Added

```
k8s/
└── memcore-pod.yaml     ← first Kubernetes manifest
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
| Kubernetes — Single Container Pod | Complete |
| Kubernetes — Multi-Container Pod | Tomorrow |

---

## Reflection

The jump from Docker Compose to Kubernetes is not about learning new tools — it is about learning a new mental model. Docker Compose thinks in terms of processes. Kubernetes thinks in terms of desired state. You describe what you want. The cluster figures out how to make it true and keeps it true.

The failure today — Go starting without C++ — was not a bug. It was the system behaving correctly under an incomplete deployment. The boundary between Go and C++ that was enforced throughout this project now needs to be expressed in Kubernetes terms: two containers, one Pod, one shared network. That constraint was always there. Kubernetes just makes it explicit.