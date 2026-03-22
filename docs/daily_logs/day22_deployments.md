# Day 22 — Deployments
**Date: 22 March 2026**

---

## Goal

Replace the raw memcore Pod with a Kubernetes Deployment — adding self-healing, rolling updates, rollback, and scaling to the memcore workload.

---

## Why Deployments Over Raw Pods

A raw Pod has no supervisor. If it crashes, it stays crashed. Updates require manual delete and recreate. Scaling requires creating Pods one by one.

A Deployment is a higher-level abstraction that manages Pods through a ReplicaSet:

| | Raw Pod | Deployment |
|---|---|---|
| Pod crashes | Gone forever | ReplicaSet recreates automatically |
| Rolling update | Manual delete + recreate | Automatic, zero downtime |
| Rollback | Manual | One command |
| Scaling | Manual | One command |

In production, raw Pods are almost never used directly. Every real workload runs as a Deployment.

---

## Deployment Hierarchy

```
Deployment
    ↓ manages rollouts and rollbacks
ReplicaSet
    ↓ ensures correct number of Pods
Pod
    ↓ runs the actual containers
```

When you create a Deployment, Kubernetes automatically creates a ReplicaSet which automatically creates the Pods. You never create Pods directly when using a Deployment.

---

## What Was Done

### 1. Created Deployment Manifest

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: memcore
  namespace: memcore
  labels:
    app: memcore
spec:
  replicas: 1
  selector:
    matchLabels:
      app: memcore
  template:
    metadata:
      labels:
        app: memcore
        tier: backend
        component: engine
    spec:
      volumes:
        - name: memcore-storage
          persistentVolumeClaim:
            claimName: memcore-data
      containers:
        - name: cpp-engine
          image: memcore-c++:latest
          imagePullPolicy: Never
          ports:
            - containerPort: 50051
          resources:
            requests:
              memory: "128Mi"
              cpu: "250m"
            limits:
              memory: "256Mi"
              cpu: "500m"
          volumeMounts:
            - name: memcore-storage
              mountPath: /app/data
          envFrom:
            - configMapRef:
                name: memcore-config
            - secretRef:
                name: memcore-secret

        - name: go-api
          image: memcore-go:latest
          imagePullPolicy: Never
          ports:
            - containerPort: 8080
          resources:
            requests:
              memory: "64Mi"
              cpu: "125m"
            limits:
              memory: "128Mi"
              cpu: "250m"
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

**Key difference from Pod manifest:**
- `kind: Deployment` — not `kind: Pod`
- `spec.replicas` — desired number of Pods
- `spec.selector.matchLabels` — how Deployment finds its Pods
- `spec.template` — the Pod definition lives inside here

### 2. Verified Deployment Hierarchy

```bash
kubectl get all -n memcore

# NAME                           READY   STATUS    RESTARTS   AGE
# pod/memcore-66bcf795c4-fhvkx   2/2     Running   0          2m

# NAME                      READY   UP-TO-DATE   AVAILABLE
# deployment.apps/memcore   1/1     1            1

# NAME                                 DESIRED   CURRENT   READY
# replicaset.apps/memcore-66bcf795c4   1         1         1
```

Pod name format: `memcore-66bcf795c4-fhvkx`
- `memcore` — deployment name
- `66bcf795c4` — replicaset hash
- `fhvkx` — random pod ID

### 3. Self-Healing Test

```bash
kubectl delete pod memcore-66bcf795c4-fhvkx -n memcore
kubectl get pods -n memcore --watch

# Old pod terminates → new pod created instantly
# memcore-66bcf795c4-rbthr   2/2   Running   0   35s
```

ReplicaSet detected 0 Pods running and immediately created a replacement.

### 4. Scaling

```bash
kubectl scale deployment memcore -n memcore --replicas=2
kubectl get pods -n memcore
# Two pods running

kubectl scale deployment memcore -n memcore --replicas=1
kubectl get pods -n memcore
# Back to one pod
```

### 5. Rolling Update + Rollback

```bash
# simulate bad update
kubectl set image deployment/memcore go-api=memcore-go:v2 -n memcore

# v2 doesn't exist — ErrImageNeverPull
kubectl rollout status deployment/memcore -n memcore
# Waiting for deployment "memcore" rollout to finish: 1 old replicas are pending termination...

# old pods kept running throughout — zero downtime
kubectl get pods -n memcore
# memcore-66bcf795c4-fhvkx   2/2   Running   0   11m  ← old version still serving
# memcore-55488957c4-g6lbk   1/2   ErrImageNeverPull  ← new version failed

# rollback
kubectl rollout undo deployment/memcore -n memcore
kubectl rollout status deployment/memcore -n memcore
# deployment "memcore" successfully rolled out
```

**Key behaviour during failed update:**
Old Pods keep running and serving traffic until new Pods pass readiness probes. Kubernetes never kills the old version until the new version is confirmed healthy. This is what makes rolling updates safe.

---

## Rollout Commands Reference

```bash
kubectl rollout status deployment/<n> -n <ns>      # watch rollout progress
kubectl rollout history deployment/<n> -n <ns>     # see revision history
kubectl rollout undo deployment/<n> -n <ns>        # rollback to previous
kubectl rollout undo deployment/<n> -n <ns> --to-revision=1  # rollback to specific revision
```

---

## CKAD Concepts Practiced Today

- `Deployment` manifest — `replicas`, `selector`, `template`
- Deployment → ReplicaSet → Pod hierarchy
- Self-healing — ReplicaSet recreates crashed Pods
- `kubectl scale` — scaling replicas imperatively
- `kubectl set image` — triggering a rolling update
- `kubectl rollout status` — watching update progress
- `kubectl rollout undo` — rolling back a failed update
- Rolling update safety — old Pods stay running until new Pods are healthy

---

## kubectl Commands Learned

```bash
kubectl get deployment -n <ns>                          # list deployments
kubectl get replicaset -n <ns>                          # list replicasets
kubectl get all -n <ns>                                 # all resources
kubectl scale deployment <n> -n <ns> --replicas=<N>    # scale
kubectl set image deployment/<n> <container>=<image>    # update image
kubectl rollout status deployment/<n> -n <ns>           # rollout progress
kubectl rollout history deployment/<n> -n <ns>          # revision history
kubectl rollout undo deployment/<n> -n <ns>             # rollback
```

---

## Files Added / Changed

```
k8s/
├── memcore-deployment.yaml     ← new — replaces memcore-pod.yaml
└── memcore-pod.yaml            ← kept for reference
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
| Kubernetes — Jobs + CronJobs | Tomorrow |

---

## Reflection

The Deployment is the first Kubernetes primitive that manages state over time — not just what is running now, but how it got there and how to get back if something goes wrong. The rollout history is a record of every change made to the workload. The rollback is a guarantee that a bad change can always be undone.

The rolling update behaviour — old Pods staying alive until new Pods are healthy — is the same principle behind the WAL + snapshot persistence design: never destroy the old state until the new state is confirmed correct. The same reasoning that made crash recovery correct in the storage layer makes rolling updates safe at the deployment layer.