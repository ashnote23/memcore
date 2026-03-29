# Day 26 — SecurityContexts
**Date: 29 March 2026**

---

## Goal

Add SecurityContexts to the memcore Deployment — restricting container privileges, preventing root execution, and blocking privilege escalation attacks.

---

## Why SecurityContexts

By default containers run as root inside a Pod. If an attacker breaks into a container running as root, they have full root access to everything inside it. SecurityContexts reduce the blast radius of a container compromise by restricting what the process can do.

Two levels:
- **Pod-level** — applies to all containers in the Pod
- **Container-level** — applies to a specific container only

Container-level settings override Pod-level settings for that specific container.

---

## Key Fields

| Field | What it does |
|---|---|
| `runAsNonRoot` | Container must not run as root — fails if image runs as UID 0 |
| `runAsUser` | Run as specific user ID |
| `allowPrivilegeEscalation` | Prevent gaining more privileges than the parent process |
| `readOnlyRootFilesystem` | Container cannot write to its own filesystem |
| `capabilities` | Add or drop Linux kernel capabilities |

---

## What Was Done

### Added SecurityContext to Deployment

**Pod-level** — applies to both cpp-engine and go-api:

```yaml
    spec:
      securityContext:
        runAsNonRoot: true
        runAsUser: 1000
```

**Container-level** — applies to each container individually:

```yaml
          securityContext:
            allowPrivilegeEscalation: false
            readOnlyRootFilesystem: false
```

**Why `readOnlyRootFilesystem: false`:**
The C++ engine writes `snapshot.bin` and `review.log` to `/app/data`. Setting this to `true` would block all filesystem writes — breaking WAL persistence entirely. The PVC mount at `/app/data` is the only write path needed, and it is a separate volume, not the root filesystem.

### Verified

```bash
winpty kubectl exec -it memcore-7c46bbb979-fwwg8 -n memcore -c go-api -- sh

id
# uid=1000 gid=0(root) groups=0(root)
```

`uid=1000` confirms the container is running as user 1000, not root. The `runAsNonRoot` + `runAsUser: 1000` combination is working correctly.

---

## Pod-level vs Container-level

```yaml
spec:
  securityContext:          ← Pod-level — applies to ALL containers
    runAsNonRoot: true
    runAsUser: 1000
  containers:
  - name: cpp-engine
    securityContext:        ← Container-level — applies to THIS container only
      allowPrivilegeEscalation: false
      readOnlyRootFilesystem: false
```

Container-level settings override Pod-level settings. If cpp-engine needed a different user ID, it could set `runAsUser` at container level and override the Pod-level value.

---

## Common YAML Mistake Today

`securityContext` at Pod level must be inside `template.spec`, not at `template` level:

```yaml
# wrong
  template:
    metadata:
      ...
  securityContext:      ← outside spec — ignored

# correct
  template:
    metadata:
      ...
    spec:
      securityContext:  ← inside spec — applied
```

---

## CKAD Concepts Practiced Today

- Pod-level vs container-level `securityContext`
- `runAsNonRoot` + `runAsUser` — non-root execution
- `allowPrivilegeEscalation: false` — preventing privilege escalation
- `readOnlyRootFilesystem` — filesystem write restriction
- Verifying security context with `id` inside a running container
- Why persistence requirements affect security decisions

---

## Files Changed

```
k8s/
└── memcore-pod.yaml     ← added securityContext at Pod and container level
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
| Kubernetes — SecurityContexts | Complete |
| Kubernetes — ServiceAccounts + RBAC | Tomorrow |

---

## Reflection

SecurityContexts make the principle of least privilege concrete at the container level. The memcore architecture already enforced least privilege at the application level — Go cannot call C++ except through the gRPC contract, C++ cannot handle HTTP. SecurityContexts extend that principle to the operating system level — the process cannot run as root, cannot escalate privileges, and cannot write outside its designated volume.

The `readOnlyRootFilesystem: false` decision is the honest one. Setting it to `true` would look more secure on paper but would break the persistence layer. Security decisions must account for real system requirements — the WAL needs to write, and that write path must remain open. The correct answer is not maximum restriction but minimum necessary access.