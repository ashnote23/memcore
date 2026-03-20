# Day 20 — Liveness and Readiness Probes
**Date: 20 March 2026**

---

## Goal

Add liveness and readiness probes to the Go API container in the memcore Pod — making Kubernetes aware of the application's health state, not just whether the container process is running.

---

## Why Probes

Kubernetes knows a container is running when the process starts. It does not know whether the application inside is actually healthy and serving traffic. A container can be running but deadlocked, or running but still waiting for a dependency to come up.

Probes give Kubernetes visibility into application health — not just process health.

| Probe | Question | Fails → |
|---|---|---|
| Liveness | Is the container still alive? | Container restarts |
| Readiness | Is it ready to accept traffic? | Removed from Service endpoints |

The distinction matters. A liveness failure means something is broken and needs a restart. A readiness failure means the application is not yet ready — traffic should wait, not force a restart.

---

## Why Both Probes Matter for memcore

The Go API layer depends on the C++ engine via gRPC. On startup, Go attempts to connect to C++ before it can serve HTTP requests. Without a readiness probe, Kubernetes might route traffic to the Go container before the gRPC connection is established — causing request failures during startup.

The `initialDelaySeconds` on the readiness probe gives Go time to complete the gRPC handshake before the first probe fires.

---

## What Was Done

### Added Probes to go-api Container

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
      livenessProbe:
        httpGet:
          path: /due-cards
          port: 8080
        initialDelaySeconds: 10
        periodSeconds: 15
        failureThreshold: 3
      readinessProbe:
        httpGet:
          path: /due-cards
          port: 8080
        initialDelaySeconds: 5
        periodSeconds: 10
        failureThreshold: 3
```

**Field reference:**

| Field | Value | Meaning |
|---|---|---|
| `initialDelaySeconds` | 5–10 | Wait before first probe — gives Go time to connect to C++ |
| `periodSeconds` | 10–15 | How often to probe |
| `failureThreshold` | 3 | Consecutive failures before acting |

### Probe Endpoint Choice

Both probes use `GET /due-cards` — a real application endpoint that returns 200 OK when the Go HTTP server is up and the gRPC connection to C++ is established. A successful response confirms the full stack is healthy, not just the HTTP layer.

### Verified via kubectl describe

```bash
kubectl describe pod memcore -n memcore

# go-api container:
# Liveness:   http-get http://:8080/due-cards delay=10s period=15s #success=1 #failure=3
# Readiness:  http-get http://:8080/due-cards delay=5s period=10s #success=1 #failure=3

# Conditions:
# Initialized       True
# Ready             True
# ContainersReady   True
# PodScheduled      True
```

All four conditions `True` — both containers running and readiness probe passing.

### Verified Probe Endpoint

```bash
kubectl port-forward pod/memcore 8080:8080 -n memcore

curl -X GET http://localhost:8080/due-cards \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"date":0,"topic_id":100}'
# → {"card_ids":[]}
```

Empty array is a valid 200 response — no cards due on date 0. The probe sees 200 OK and marks the container healthy.

---

## Probe Types Reference

Three probe types exist in Kubernetes — today used `httpGet`:

| Type | How it works | Use for |
|---|---|---|
| `httpGet` | HTTP GET request — success if 200-399 | HTTP services |
| `exec` | Runs a command — success if exit code 0 | Non-HTTP services |
| `tcpSocket` | TCP connection — success if port accepts | TCP services |

For memcore's C++ engine, a `tcpSocket` probe on port 50051 would be the correct choice — the gRPC server accepts TCP connections but doesn't serve HTTP.

---

## Common Exam Trap

Liveness and readiness are easy to mix up:

```
Liveness  = is it alive?    fails → restart container
Readiness = is it ready?    fails → remove from Service endpoints (pod stays running)
```

A readiness failure does not delete or restart the Pod. It only stops traffic from being routed to it until the probe passes again.

---

## CKAD Concepts Practiced Today

- `livenessProbe` and `readinessProbe` in Pod spec
- `httpGet` probe type with `path` and `port`
- `initialDelaySeconds`, `periodSeconds`, `failureThreshold`
- `kubectl describe pod` — reading probe config and conditions
- Difference between liveness and readiness behaviour on failure
- Choosing the right probe endpoint for a real application

---

## Files Changed

```
k8s/
└── memcore-pod.yaml     ← added livenessProbe + readinessProbe to go-api
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
| Kubernetes — Resource Limits | Tomorrow |

---

## Reflection

Probes make explicit something that was always implicit: the difference between a process running and an application being healthy. The Go container starting does not mean it is ready — it means the binary is executing. Readiness is a higher bar: the HTTP server is up, the gRPC connection to C++ is established, and the application can serve real traffic.

The `initialDelaySeconds` on the readiness probe encodes a real architectural constraint — Go depends on C++ and needs time to connect. This is the same dependency expressed in Docker Compose via `depends_on`. In Kubernetes there is no `depends_on` for Pods — probes are how you express startup ordering at the application level.