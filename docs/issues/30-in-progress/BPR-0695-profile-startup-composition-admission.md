---
schema: "implement-rfc.card.v1"
id: "BPR-0695"
title: "Admit one profile before constructing its private runtime"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0694", "BPR-0693"]
---

# Admit one profile before constructing its private runtime

## Scope

Asynchronous startup admission resolves a requested durable BPR profile before constructing its existing single-window runtime. Keep RegistryRoot separate from ProfileDataRoot; all private stores, caches, recovery, workspaces and diagnostics receive selected root, and vault/host/binding factories receive selected ID. Unknown or corrupt registry refuses without fallback. Same-profile multi-window sharing, activation routing and cross-process owner locks are separate later slices.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns CMake, shared composition, bookkeeping and acceptance. Workers own
explicitly assigned new authority/adapter/test files and preserve others.

## Acceptance

Focused admission host cancellation/stale/failure checks and fixture public course admit Default and profile2, reopen isolated durable settings, and verify no nonDefault private service receives registry base. Architecture and composition boundary checks.

## Evidence

Read-only authority inventory identifies one current application-data root and
Default-valued native factory calls. No implementation or new check yet.
