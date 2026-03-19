# Day 18 — ConfigMaps and Secrets
**Date: 15 March 2026**

---

## Goal

Move hardcoded configuration out of the Pod manifest and into Kubernetes ConfigMaps and Secrets — the correct pattern for managing configuration in a production Kubernetes deployment.

---

## Why ConfigMaps and Secrets

Hardcoding configuration directly in a Pod manifest couples the application to its environment. Changing a hostname or port requires editing and redeploying the manifest. In a production system, configuration is separated from the workload definition.

ConfigMaps and Secrets decouple configuration from the Pod spec — the same manifest runs in development, staging, and production with different ConfigMaps injected at deploy time.

| | ConfigMap | Secret |
|---|---|---|
| Use for | Non-sensitive config | Sensitive values |
| Examples | hostnames, ports, file paths | passwords, API keys, tokens |
| Storage | Plain text | Base64 encoded |
| Visible in describe | Yes | Keys only — values hidden |

---

## What Was Done

### 1. Created the ConfigMap

```bash
kubectl create configmap memcore-config \
  --from-literal=CPP_ENGINE_HOST=localhost \
  --from-literal=HTTP_PORT=8080 \
  --from-literal=GRPC_PORT=50051 \
  -n memcore
```

```bash
kubectl describe configmap memcore-config -n memcore

# Data
# ====
# CPP_ENGINE_HOST: localhost
# GRPC_PORT:       50051
# HTTP_PORT:       8080
```

### 2. Created the Secret

```bash
kubectl create secret generic memcore-secret \
  --from-literal=API_KEY=supersecretkey123 \
  --from-literal=DB_PASSWORD=dummypassword \
  -n memcore
```

`describe` on a Secret shows keys but never values — values are base64 encoded and intentionally hidden:

```bash
kubectl describe secret memcore-secret -n memcore

# Data
# ====
# API_KEY:      18 bytes
# DB_PASSWORD:  13 bytes
```

To retrieve a Secret value:
```bash
kubectl get secret memcore-secret -n memcore \
  -o jsonpath='{.data.API_KEY}' | base64 --decode
# → supersecretkey123
```

### 3. Updated Pod Manifest

Replaced hardcoded `env` with `envFrom` — injects all keys from ConfigMap and Secret at once:

```yaml
    - name: go-api
      image: memcore-go:latest
      imagePullPolicy: Never
      ports:
        - containerPort: 8080
      envFrom:
        - configMapRef:
            name: memcore-config
        - secretRef:
            name: memcore-secret
```

**`env` vs `envFrom`:**

```yaml
# env — inject one variable at a time, named explicitly
env:
  - name: CPP_ENGINE_HOST
    valueFrom:
      configMapKeyRef:
        name: memcore-config
        key: CPP_ENGINE_HOST

# envFrom — inject ALL keys from a ConfigMap or Secret at once
envFrom:
  - configMapRef:
      name: memcore-config
```

`envFrom` is cleaner when injecting multiple values. `env` with `valueFrom` is more precise when you need to rename keys or inject selectively.

### 4. Verified Inside the Container

```bash
winpty kubectl exec -it memcore -n memcore -c go-api -- sh

env | grep CPP_ENGINE_HOST   # → CPP_ENGINE_HOST=localhost
env | grep HTTP_PORT          # → HTTP_PORT=8080
env | grep GRPC_PORT          # → GRPC_PORT=50051
env | grep API_KEY            # → API_KEY=supersecretkey123
```

All four values injected correctly from ConfigMap and Secret.

### 5. End-to-End Verified

```bash
kubectl port-forward pod/memcore 8080:8080 -n memcore

curl -X POST http://localhost:8080/topic \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"topic_id":100,"name":"ConfigMaps"}'
# → {"success":true}
```

memcore serving real traffic with configuration managed entirely through Kubernetes objects.

---

## Issue Encountered

Typo in initial ConfigMap creation — `GRPV_PORT` instead of `GRPC_PORT`. ConfigMaps are immutable once created via `--from-literal`. Fix: delete and recreate.

```bash
kubectl delete configmap memcore-config -n memcore
kubectl create configmap memcore-config \
  --from-literal=CPP_ENGINE_HOST=localhost \
  --from-literal=HTTP_PORT=8080 \
  --from-literal=GRPC_PORT=50051 \
  -n memcore
```

Lesson: verify keys with `kubectl describe configmap` immediately after creation.

---

## CKAD Concepts Practiced Today

- Creating ConfigMaps from literal values
- Creating Secrets from literal values
- `envFrom` — injecting all keys from ConfigMap or Secret
- `env` with `valueFrom` — selective key injection
- Decoding a Secret value with `base64 --decode`
- `-o jsonpath` — extracting specific fields from K8s resources
- Redeploying a Pod to pick up updated config

---

## kubectl Commands Learned

```bash
kubectl create configmap <name> --from-literal=k=v -n <ns>   # create configmap
kubectl create secret generic <name> --from-literal=k=v -n <ns>  # create secret
kubectl get configmap -n <ns>                                  # list configmaps
kubectl get secret -n <ns>                                     # list secrets
kubectl describe configmap <name> -n <ns>                      # inspect configmap
kubectl describe secret <name> -n <ns>                         # inspect secret (keys only)
kubectl get secret <name> -n <ns> -o jsonpath='{.data.<key>}' | base64 --decode
```

---

## Files Changed

```
k8s/
└── memcore-pod.yaml     ← replaced hardcoded env with envFrom
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
| Kubernetes — PersistentVolumes + PVCs | Tomorrow |

---

## Reflection

ConfigMaps and Secrets complete the separation between what the application is and how it is configured. The Pod manifest now describes only the workload — images, ports, and which config objects to inject. The configuration itself lives independently and can be changed without touching the workload definition.

This is the same principle applied on Day 14 with environment variables for Docker — `CPP_ENGINE_HOST` was configurable then for the same reason it is configurable now. The pattern carries forward across environments: local development, Docker Compose, and Kubernetes all use the same mechanism with different values injected at the boundary.