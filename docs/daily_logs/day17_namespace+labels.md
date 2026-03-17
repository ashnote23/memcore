# Day 17 — Namespaces and Labels
**Date: 14 March 2026**

---

## Goal

Move the memcore Pod out of the `default` namespace into a dedicated `memcore` namespace, apply meaningful labels, and practice label-based querying — a core CKAD exam skill.

---

## Why Namespaces

Without namespaces, every resource in a Kubernetes cluster shares the same flat space. In production, a cluster runs many applications simultaneously — namespaces provide isolation, access control boundaries, and resource quota scope.

The `default` namespace is a convenience for learning. Production deployments always use dedicated namespaces.

```
default/          ← where memcore was
memcore/          ← where memcore lives now
kube-system/      ← Kubernetes internal components
```

---

## Why Labels

Labels are key-value pairs attached to Kubernetes resources. They have no meaning to Kubernetes itself — their meaning is defined by the system using them.

Labels matter because:
- Services use label selectors to find which Pods to route traffic to
- Deployments use label selectors to manage their Pods
- Operators use label selectors to find and manage resources
- `kubectl` queries use label selectors to filter resources

A resource without labels cannot be selected. A resource with the wrong labels will not be found.

---

## What Was Done

### 1. Created the Namespace

```bash
kubectl create namespace memcore
kubectl get namespaces
# NAME              STATUS   AGE
# default           Active   2d
# memcore           Active   5s
# kube-system       Active   2d
```

### 2. Updated Pod Manifest

Added `namespace` to metadata and three labels to the Pod:

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: memcore
  namespace: memcore
  labels:
    app: memcore
    tier: backend
    component: engine
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

**Label design:**

| Label | Value | Selects |
|---|---|---|
| `app` | `memcore` | everything belonging to the memcore application |
| `tier` | `backend` | everything at the backend layer |
| `component` | `engine` | the scheduling engine specifically |

This structure scales — a future Redis cache would be `app=memcore, tier=backend, component=cache`. A future UI would be `app=memcore, tier=frontend`. Labels let you select at any granularity.

### 3. Redeployed into memcore Namespace

```bash
kubectl delete pod memcore              # remove from default
kubectl apply -f k8s/memcore-pod.yaml  # create in memcore namespace
kubectl get pods -n memcore

# NAME      READY   STATUS    RESTARTS   AGE
# memcore   2/2     Running   0          30s
```

### 4. Label Querying

```bash
# all pods in namespace
kubectl get pods -n memcore

# filter by single label
kubectl get pods -n memcore -l app=memcore

# filter by multiple labels
kubectl get pods -n memcore -l app=memcore,tier=backend

# show labels column
kubectl get pods -n memcore --show-labels

# full details including labels
kubectl describe pod memcore -n memcore
```

### 5. Set Default Namespace

Avoids typing `-n memcore` on every command — important for exam speed:

```bash
kubectl config set-context --current --namespace=memcore
kubectl config view --minify | grep namespace
# namespace: memcore
```

After this, `kubectl get pods` automatically targets the `memcore` namespace.

### 6. Imperative Practice

CKAD is time-pressured — imperative commands are faster than writing YAML:

```bash
# create namespace without YAML
kubectl create namespace test-ns

# run pod directly into namespace
kubectl run test-pod --image=nginx -n test-ns

# clean up
kubectl delete pod test-pod -n test-ns
kubectl delete namespace test-ns
```

---

## Common Mistake

```bash
# wrong — set-context is a subcommand, not a flag
kubectl config --set-context --current --namespace=memcore

# correct
kubectl config set-context --current --namespace=memcore
```

`set-context` has no `--` prefix. A typo here silently does nothing — the context is not updated.

---

## CKAD Concepts Practiced Today

- Creating and listing namespaces
- Scoping resources to a namespace via `metadata.namespace`
- Designing a label schema for a multi-component application
- Label selector queries with `-l key=value` and `-l key=value,key=value`
- `--show-labels` flag
- Setting default namespace with `kubectl config set-context`
- Imperative namespace and Pod creation

---

## kubectl Commands Learned

```bash
kubectl create namespace <name>                          # create namespace
kubectl get namespaces                                   # list all namespaces
kubectl get pods -n <namespace>                          # pods in namespace
kubectl get pods -n <namespace> -l <key>=<value>         # filter by label
kubectl get pods -n <namespace> --show-labels            # show label column
kubectl describe pod <name> -n <namespace>               # full pod details
kubectl config set-context --current --namespace=<name>  # set default namespace
kubectl config view --minify | grep namespace            # verify default namespace
```

---

## Files Changed

```
k8s/
└── memcore-pod.yaml     ← added namespace + labels to metadata
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
| Kubernetes — ConfigMaps + Secrets | Tomorrow |

---

## Reflection

Namespaces and labels feel like administrative detail but they are the foundation of everything else in Kubernetes. A Service finds its Pods by label selector. An Operator manages resources by label selector. RBAC is scoped to namespaces. Without a deliberate label schema, a cluster becomes hard to query, hard to secure, and hard to automate.

The three-label schema chosen today — `app`, `tier`, `component` — follows the same reasoning applied throughout this project: design for extensibility from the start. Adding a cache or a UI layer later requires no rethinking of the label structure. The schema already accommodates it.