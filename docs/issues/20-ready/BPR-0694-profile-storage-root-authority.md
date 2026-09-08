---
schema: "implement-rfc.card.v1"
id: "BPR-0694"
title: "Resolve private storage roots from admitted profile identities"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0691"]
---

# Resolve private storage roots from admitted profile identities

## Scope

Application root authority selected only from a valid durable registry entry and a SQLite/filesystem adapter resolving owner-private ID-derived roots. Default retains the canonical base root; isolated profiles use profiles/profile-N. Reject unknown IDs, malformed authority, symlinks/path escape and foreign roots before profile stores are constructed. No labels become paths; no profile runtime or window UI in this slice.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns CMake, shared composition, bookkeeping and acceptance. Workers own
explicitly assigned new authority/adapter/test files and preserve others.

## Acceptance

Focused authority and filesystem contract tests cover Default in place, distinct profile roots, rename invariance, invalid tokens, symlink refusal, permissions and cancellation. Existing domain data must remain untouched.

## Evidence

Read-only authority inventory identifies one current application-data root and
Default-valued native factory calls. No implementation or new check yet.
