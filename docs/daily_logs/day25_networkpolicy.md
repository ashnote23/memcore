# Day 25 — Network Policies
**Date: 28 March 2026**

---

## Goal

Add a Network Policy to the memcore namespace — restricting which traffic is allowed in and out of the memcore Pods, replacing the default allow-all behaviour with explicit rules.

---

## Why Network Policies

By default Kubernetes allows all Pod-to-Pod traffic freely. Any Pod can reach any other Pod in the cluster. In production this is dangerous — a compromised Pod can reach everything.

Network Policies are firewall rules for Pods:

```
Without Network Policy:
memcore Pod → can reach ANY pod in cluster

With Network Policy:
memcore Pod → can ONLY reach what the policy allows
Everything else is denied
```

---

## Key Concepts

**podSelector** — which Pods the policy applies to. Pods matching the labels are governed by the policy.

**policyTypes** — which direction of traffic the policy controls:
- `Ingress` — incoming traffic TO the Pod
- `Egress` — outgoing traffic FROM the Pod

**Default deny** — once a policy selects a Pod, all traffic not explicitly allowed is blocked.

---

## What Was Done

### Network Policy Manifest

Written from scratch — `kubectl create` does not support NetworkPolicy imperatively:

```yaml
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: memcore-netpol
  namespace: memcore
spec:
  podSelector:
    matchLabels:
      app: memcore
  policyTypes:
  - Ingress
  - Egress
  ingress:
  - from:
    - namespaceSelector:
        matchLabels:
          kubernetes.io/metadata.name: memcore
    ports:
    - protocol: TCP
      port: 8080
    - protocol: TCP
      port: 50051
  egress:
  - to:
    - namespaceSelector:
        matchLabels:
          kubernetes.io/metadata.name: memcore
    ports:
    - protocol: TCP
      port: 50051
  - to:
    - namespaceSelector:
        matchLabels:
          kubernetes.io/metadata.name: kube-system
    ports:
    - protocol: UDP
      port: 53
```

**Ingress rule:** Allow incoming traffic on port 8080 (HTTP) and 50051 (gRPC) from within the memcore namespace only.

**Egress rule 1:** Allow outgoing gRPC traffic on port 50051 within the memcore namespace — Go API reaching C++ engine.

**Egress rule 2:** Allow DNS resolution on port 53 UDP to kube-system — without this containers cannot resolve hostnames.

### Verified

```bash
kubectl get networkpolicy -n memcore
# NAME             POD-SELECTOR   AGE
# memcore-netpol   app=memcore    10s
```

---

## Common Mistakes Made Today

| Mistake | Correct |
|---|---|
| `ploicyTypes` | `policyTypes` |
| `portocol` | `protocol` |
| `namespaceSelector` + `- matchLabels` | `namespaceSelector` + `matchLabels` (no `-`) |
| `ports` indented under `namespaceSelector` | `ports` at same level as `to`/`from` |

NetworkPolicy YAML indentation is strict — a misplaced `-` or extra indent changes the meaning entirely.

---

## Ingress vs Egress

Easy to confuse — always think relative to the Pod:

```
Ingress = incoming traffic TO the Pod
          (someone calling your API)

Egress  = outgoing traffic FROM the Pod
          (your Pod calling another service)
```

---

## Why Port 53 UDP in Egress

Without DNS egress, containers cannot resolve hostnames. `memcore-grpc` Service name cannot be resolved. `kube-system` hosts CoreDNS which handles all cluster DNS — allowing UDP 53 to kube-system enables hostname resolution across the cluster.

---

## CKAD Concepts Practiced Today

- Writing a NetworkPolicy from scratch — no imperative command available
- `podSelector` — selecting Pods by label
- `policyTypes` — Ingress and Egress
- `namespaceSelector` — restricting traffic by namespace
- `ports` — protocol and port number
- Default deny behaviour — unlisted traffic is blocked
- DNS egress — why port 53 UDP must be explicitly allowed

---

## Files Added

```
k8s/
└── memcore-netpol.yaml     ← Network Policy for memcore Pods
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
| Kubernetes — Network Policies | Complete |
| Kubernetes — SecurityContexts | Tomorrow |

---

## Reflection

Network Policies make explicit what was always assumed — that not everything should talk to everything. The memcore architecture already enforced a strict boundary between Go and C++ at the language level via gRPC. Network Policies enforce the same boundary at the infrastructure level — Go can reach C++ on port 50051, nothing else is allowed out. The boundary that was a design decision on Day 1 is now a cluster-level enforcement on Day 25.

The DNS egress rule is a reminder that infrastructure has dependencies that are easy to forget. Blocking all egress and wondering why hostnames stop resolving is a common production incident. Explicit rules require explicit thinking about every dependency — including the ones that are usually invisible.