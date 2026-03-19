# Day 19 — PersistentVolumes and PVCs
**Date: 15 March 2026**

---

## Goal

Mount persistent storage into the memcore Pod so that `snapshot.bin` and `review.log` survive Pod restarts — the Kubernetes equivalent of the Docker volume added on Day 14.

---

## Why Persistent Storage

Without a PVC, all files written inside a container live in the container's ephemeral filesystem. When the Pod is deleted and recreated, everything is gone. For memcore this means losing all card state — the entire point of the WAL + snapshot persistence layer.

A PersistentVolumeClaim gives the Pod access to storage that exists independently of the Pod's lifecycle. The volume outlives the Pod.

---

## Kubernetes Storage Model

```
PersistentVolume (PV)
  ← the actual storage provisioned by the cluster
  ← exists independently of any Pod or PVC

PersistentVolumeClaim (PVC)
  ← a request for storage by a workload
  ← Kubernetes binds it to a matching PV
  ← Bound = PVC matched to available PV storage

Pod volumeMount
  ← makes the PVC available at a path inside a container
  ← declared in spec.volumes, mounted in container.volumeMounts
```

You never use a PV directly. You always claim storage via a PVC and mount it into the Pod.

---

## What Was Done

### 1. Created the PVC

```yaml
# k8s/memcore-pvc.yaml
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: memcore-data
  namespace: memcore
spec:
  accessModes:
    - ReadWriteOnce
  resources:
    requests:
      storage: 1Gi
```

```bash
kubectl apply -f k8s/memcore-pvc.yaml
kubectl get pvc -n memcore

# NAME           STATUS   VOLUME         CAPACITY   ACCESS MODES
# memcore-data   Bound    pvc-abc1234    1Gi        RWO
```

`Bound` — Kubernetes found available storage and matched it to the PVC.

**`ReadWriteOnce`** — the volume can be mounted by one node at a time, and that node can both read and write. Not "read and write only once" — it means single-node access, full read+write.

| Access Mode | Meaning |
|---|---|
| `ReadWriteOnce` | One node at a time, read + write |
| `ReadOnlyMany` | Many nodes, read only |
| `ReadWriteMany` | Many nodes, read + write |

### 2. Updated Pod Manifest

Added `spec.volumes` to declare the PVC and `volumeMounts` on the cpp-engine container to mount it at `/app/data`:

```yaml
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
      envFrom:
        - configMapRef:
            name: memcore-config
        - secretRef:
            name: memcore-secret
```

`spec.volumes` declares the PVC as a named volume. `volumeMounts` mounts that named volume into the container at a specific path. Two separate steps — declaration and mount.

### 3. Verified Mount Inside Container

```bash
winpty kubectl exec -it memcore -n memcore -c cpp-engine -- sh

ls /app/data
# review.log  snapshot.bin
```

The C++ engine is writing directly to the PVC-backed volume.

### 4. Full Persistence Test

Created card data, deleted the Pod, recreated it, verified data survived:

```bash
# create data
curl -X POST http://localhost:8080/topic \
  -d '{"user_id":1,"topic_id":100,"name":"Kubernetes"}'
curl -X POST http://localhost:8080/card \
  -d '{"user_id":1,"card_id":1,"topic_id":100}'
curl -X POST http://localhost:8080/review \
  -d '{"user_id":1,"card_id":1,"rating":3}'

# restart Pod
kubectl delete pod memcore -n memcore
kubectl apply -f k8s/memcore-pod.yaml

# verify data survived
curl -X GET http://localhost:8080/due-cards \
  -d '{"user_id":1,"date":100,"topic_id":100}'
# → {"card_ids":[2,1]}  ✅
```

The C++ engine's startup sequence ran correctly on restart:
```
1. Load snapshot  → reconstruct card state
2. Replay WAL log → reapply events since checkpoint
3. Rebuild heap   → restore due card queue
```

Card state survived Pod deletion and recreation — persistence verified end to end on Kubernetes.

---

## Docker Compose vs Kubernetes — Storage Comparison

| | Docker Compose | Kubernetes |
|---|---|---|
| Declaration | `volumes: - ./data:/app/data` | PVC + `spec.volumes` |
| Mount path | `/app/data` | `/app/data` |
| Storage location | Host machine `./data/` | PV managed by cluster |
| Survives restart | Yes | Yes |
| Survives deletion | Yes (host folder) | Yes (PV lifecycle) |

Same guarantee, different mechanism. The C++ engine sees `/app/data` in both cases — the platform difference is invisible to the application.

---

## CKAD Concepts Practiced Today

- Writing a PVC manifest — `accessModes`, `storage` request
- Declaring a volume in `spec.volumes` from a PVC
- Mounting a volume in `container.volumeMounts`
- Understanding `Bound` PVC status
- `ReadWriteOnce` access mode
- Verifying mounts inside a running container
- End-to-end persistence test — delete Pod, recreate, verify data

---

## kubectl Commands Learned

```bash
kubectl apply -f memcore-pvc.yaml           # create PVC
kubectl get pvc -n <namespace>              # list PVCs and status
kubectl describe pvc <name> -n <namespace>  # full PVC details
kubectl get pv                              # list PersistentVolumes
```

---

## Files Added / Changed

```
k8s/
├── memcore-pvc.yaml     ← new — PersistentVolumeClaim for WAL storage
└── memcore-pod.yaml     ← updated — added volumes + volumeMounts
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
| Kubernetes — Services + Ingress | Week 2 |

---

## Reflection

The persistence test today closed the loop on a decision made on Day 3 of this project: snapshot + append-only log with CRC32 crash recovery. That design was built for correctness under failure. Today it was verified under a different kind of failure — Pod deletion in Kubernetes.

The storage layer didn't change. The application didn't change. The WAL and snapshot mechanism works identically whether the process restarts on a local machine, inside Docker, or inside a Kubernetes Pod backed by a PVC. That's what good abstraction looks like — the persistence guarantee holds regardless of the deployment environment.