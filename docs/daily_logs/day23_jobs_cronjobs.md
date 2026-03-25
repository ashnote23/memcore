# Day 23 — Jobs and CronJobs
**Date: 22 March 2026**

---

## Goal

Add one-off and scheduled task primitives to the memcore Kubernetes setup using Jobs and CronJobs — covering snapshot verification and scheduled storage checks.

---

## Why Jobs and CronJobs

Deployments run processes that never stop — the Go HTTP server and C++ engine run indefinitely. Some tasks are fundamentally different: they run once, do their work, and exit. Kubernetes has two primitives for this:

| | Job | CronJob |
|---|---|---|
| Runs | Once | On a schedule |
| Use for | DB migration, batch processing, one-off checks | Backups, reports, cleanup |
| Completes | Yes — Pod exits 0 when done | Creates a new Job on each schedule tick |
| memcore use case | Snapshot verification | Scheduled storage check |

---

## Workflow — Base First

Every manifest was generated from an imperative command and edited minimally:

```bash
# generate base
kubectl create job <name> --image=<x> --dry-run=client -o yaml -- <command> > file.yaml

# edit only what's needed
code file.yaml

# apply
kubectl apply -f file.yaml
```

This is the correct CKAD exam workflow — generate fast, edit minimally, apply.

---

## What Was Done

### 1. Job — memcore-snapshot-check

**Base generated with:**
```bash
kubectl create job memcore-snapshot-check \
  --image=busybox \
  --dry-run=client -o yaml \
  -- /bin/sh -c 'echo "checking storage..." && ls /app/data' \
  > k8s/memcore-job.yaml
```

**Added manually:**
- `namespace: memcore`
- `completions: 1`
- `parallelism: 1`
- `backoffLimit: 3`
- PVC volume mount at `/app/data`

**Result:**
```bash
kubectl get jobs -n memcore
# memcore-snapshot-check   Complete   1/1   16s

kubectl logs -l job-name=memcore-snapshot-check -n memcore
# Checking memcore storage...
# test.txt
# Snapshot check complete!
```

The Job mounted the same PVC as the Deployment and read storage data correctly — verified the WAL storage is accessible from a one-off task.

---

### 2. CronJob — memcore-scheduled-check

**Base generated with:**
```bash
kubectl create cronjob memcore-scheduled-check \
  --image=busybox \
  --schedule="*/1 * * * *" \
  --dry-run=client -o yaml \
  -- /bin/sh -c 'echo "scheduled check..." && date' \
  > k8s/memcore-cronjob.yaml
```

**Fixed before applying:**
- Windows path `C:/Program Files/Git/usr/bin/sh` → `/bin/sh`
- Schedule `*/1****` → `*/1 * * * *` (spaces required)

**Result:**
```bash
kubectl get cronjob -n memcore
# memcore-scheduled-check   */1 * * * *   False   1   2s

kubectl get jobs -n memcore
# memcore-scheduled-check-29573663   Complete   1/1   9s   86s
# memcore-scheduled-check-29573664   Complete   1/1   9s   26s
```

Two runs confirmed — CronJob created a new Job every minute automatically.

---

## Key Concepts

### backoffLimit
Number of times Kubernetes retries a failed Job before marking it Failed:
```yaml
backoffLimit: 3   # try 3 times → if all fail → Job = Failed
```

### restartPolicy
Jobs only allow two values — `Always` is forbidden:
```
restartPolicy: Never      ← create new Pod on failure
restartPolicy: OnFailure  ← restart same Pod on failure
restartPolicy: Always     ← NOT allowed in Jobs (Deployments only)
```

### Cron Syntax
```
*/1 * * * *
 │  │ │ │ │
 │  │ │ │ └── day of week (0-7)
 │  │ │ └──── month (1-12)
 │  │ └────── day of month (1-31)
 │  └──────── hour (0-23)
 └─────────── minute (0-59)

*/1 = every 1 minute
```

### Label selector for Job logs
Job Pods have auto-generated names — use label selector instead of pod name:
```bash
kubectl logs -l job-name=<job-name> -n <namespace>
```

---

## CKAD Concepts Practiced Today

- `Job` manifest — `completions`, `parallelism`, `backoffLimit`
- `CronJob` manifest — `schedule`, `jobTemplate`
- `restartPolicy: Never` vs `OnFailure` in Jobs
- Base-first workflow — generate → edit → apply
- `kubectl logs -l job-name=` — log retrieval by label
- Cron schedule syntax
- Mounting PVC inside a Job to access persistent storage

---

## kubectl Commands Learned

```bash
kubectl create job <n> --image=<x> --dry-run=client -o yaml -- <cmd>
kubectl create cronjob <n> --image=<x> --schedule="* * * * *" --dry-run=client -o yaml -- <cmd>
kubectl get jobs -n <ns>
kubectl get cronjob -n <ns>
kubectl logs -l job-name=<n> -n <ns>
kubectl delete job <n> -n <ns>
kubectl delete cronjob <n> -n <ns>
```

---

## Files Added

```
k8s/
├── memcore-job.yaml         ← one-off snapshot verification
└── memcore-cronjob.yaml     ← scheduled storage check every minute
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
| Kubernetes — Services + Networking | Tomorrow |

---

## Reflection

Jobs and CronJobs complete the workload primitives — long-running services (Deployment), one-off tasks (Job), and scheduled tasks (CronJob). The snapshot verification Job is a natural fit for memcore: the same PVC that the C++ engine writes to can be independently read by a Job without touching the running Deployment. The storage layer's clean separation from the application layer made this trivial to implement — another consequence of good boundary design paying off at the infrastructure level.