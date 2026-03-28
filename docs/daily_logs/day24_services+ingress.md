# Day 24 — Services and Ingress
**Date: 23 March 2026**

---

## Goal

Expose memcore to internal and external traffic using Kubernetes Services and Ingress — replacing `kubectl port-forward` with proper production-grade networking primitives.

---

## Why Services

Pods get new IP addresses every time they restart. Talking directly to a Pod IP is unreliable — the IP changes and any hardcoded reference breaks. A Service gives Pods a stable endpoint that never changes, regardless of how many times the Pod restarts or scales.

Services also load balance across multiple Pod replicas automatically.

---

## Three Service Types

| Type | Accessible from | Use for |
|---|---|---|
| `ClusterIP` | Inside cluster only | Internal service-to-service communication |
| `NodePort` | Outside cluster via node IP + port | Development and testing |
| `LoadBalancer` | Outside cluster via cloud load balancer | Production on cloud |

---

## Port Fields

```
external traffic → nodePort (30080) → port (8080) → targetPort (8080) → container
```

| Field | Meaning |
|---|---|
| `port` | Port the Service listens on inside the cluster |
| `targetPort` | Port the container is listening on |
| `nodePort` | Port exposed on the Node for external access (30000-32767) |

---

## What Was Done

### 1. ClusterIP Service — Internal gRPC

Base generated with:
```bash
kubectl expose deployment memcore \
  --name=memcore-grpc \
  --port=50051 \
  --target-port=50051 \
  --dry-run=client -o yaml \
  -n memcore > k8s/memcore-svc-grpc.yaml
```

Added manually: `type: ClusterIP`, correct name.

```yaml
apiVersion: v1
kind: Service
metadata:
  name: memcore-grpc
spec:
  type: ClusterIP
  ports:
  - port: 50051
    protocol: TCP
    targetPort: 50051
  selector:
    app: memcore
```

```bash
kubectl get service -n memcore
# memcore-grpc   ClusterIP   10.111.76.97   <none>   50051/TCP
```

Stable internal IP — never changes even as Pods restart.

### 2. NodePort Service — External HTTP

Base generated with:
```bash
kubectl expose deployment memcore \
  --name=memcore-http \
  --port=8080 \
  --target-port=8080 \
  --dry-run=client -o yaml \
  -n memcore > k8s/memcore-svc-http.yaml
```

Added manually: `type: NodePort`, `nodePort: 30080`.

```yaml
apiVersion: v1
kind: Service
metadata:
  name: memcore-http
spec:
  type: NodePort
  ports:
  - port: 8080
    protocol: TCP
    targetPort: 8080
    nodePort: 30080
  selector:
    app: memcore
```

```bash
kubectl get service -n memcore
# memcore-http   NodePort   10.98.208.131   <none>   8080:30080/TCP
```

`8080:30080` — port 8080 inside cluster maps to port 30080 on the node.

### 3. Ingress — Path-Based Routing

NodePort exposes a high port (30080) — not clean for production. Ingress sits in front of all Services and routes by host and path.

Enabled ingress addon:
```bash
minikube addons enable ingress
```

Base generated with:
```bash
kubectl create ingress memcore-ingress \
  --rule="memcore.local/=memcore-http:8080" \
  --dry-run=client -o yaml \
  -n memcore > k8s/memcore-ingress.yaml
```

Changed `pathType: Exact` → `pathType: Prefix` so all paths route correctly:

```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: memcore-ingress
spec:
  rules:
  - host: memcore.local
    http:
      paths:
      - backend:
          service:
            name: memcore-http
            port:
              number: 8080
        path: /
        pathType: Prefix
```

**`pathType: Prefix` vs `Exact`:**
- `Exact` — matches `/` only
- `Prefix` — matches `/`, `/topic`, `/card`, `/due-cards`, `/review`

### 4. Verified End-to-End via Ingress

```bash
kubectl port-forward -n ingress-nginx service/ingress-nginx-controller 8181:80

curl -X POST http://localhost:8181/topic \
  -H "Host: memcore.local" \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"topic_id":100,"name":"Ingress"}'
# → {"success":true}
```

The `Host: memcore.local` header tells the Ingress controller which rule to apply — without it the controller cannot match the request to a rule.

---

## Full Networking Architecture

```
External traffic
      ↓
Ingress (memcore.local)       ← path-based routing
      ↓
memcore-http Service           ← NodePort 30080
      ↓
memcore Pod
      ↓  
go-api → gRPC → cpp-engine
      ↑
memcore-grpc Service           ← ClusterIP 50051 (internal)
```

---

## Why Ingress Over NodePort

| | NodePort | Ingress |
|---|---|---|
| Port | High port (30080) | Standard port 80/443 |
| Routing | One Service per port | Many Services by host/path |
| TLS | Manual | Built-in via annotations |
| Production ready | No | Yes |

---

## CKAD Concepts Practiced Today

- `ClusterIP` Service — stable internal endpoint
- `NodePort` Service — external access via node port
- `port`, `targetPort`, `nodePort` — three distinct port fields
- Ingress manifest — `host`, `path`, `pathType`, `backend`
- `pathType: Prefix` vs `pathType: Exact`
- Ingress controller — nginx reverse proxy reading Ingress rules
- `Host` header — how Ingress matches requests to rules
- Base-first workflow for Services and Ingress

---

## kubectl Commands Learned

```bash
kubectl expose deployment <n> --port=<p> --dry-run=client -o yaml   # generate Service
kubectl create ingress <n> --rule="host/path=svc:port" --dry-run=client -o yaml
kubectl get service -n <ns>                   # list services
kubectl get ingress -n <ns>                   # list ingress rules
kubectl describe ingress <n> -n <ns>          # ingress details
minikube addons enable ingress                # enable ingress controller
```

---

## Files Added

```
k8s/
├── memcore-svc-grpc.yaml     ← ClusterIP Service for gRPC (port 50051)
├── memcore-svc-http.yaml     ← NodePort Service for HTTP (port 30080)
└── memcore-ingress.yaml      ← Ingress routing memcore.local
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
| Kubernetes — Deployments | Complete |
| Kubernetes — Jobs + CronJobs | Complete |
| Kubernetes — Services + Ingress | Complete |
| Kubernetes — Network Policies | Tomorrow |

---

## Reflection

Services and Ingress complete the networking layer — the last piece needed to make memcore accessible as a real production workload. The three-tier network model (ClusterIP for internal, NodePort for development, Ingress for production) maps directly onto the three-environment model used throughout this project: local development, Docker Compose, and Kubernetes each have their own networking mechanism, and the application is insulated from all of them by the Service abstraction.

The `Host` header requirement for Ingress routing is the same principle as the `CPP_ENGINE_HOST` environment variable — the routing decision is driven by configuration at the boundary, not by hardcoded assumptions inside the application.