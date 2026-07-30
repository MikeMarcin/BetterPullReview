---
id: BPR-0035
title: Verify and close RFC-BPR-0002
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0030, BPR-0031, BPR-0032, BPR-0033, BPR-0034, BPR-0036, BPR-0037, BPR-0038]
---

# Verify and close RFC-BPR-0002

Replay the complete claim matrix from clean state, directly inspect the real
composition and rendered application, verify target-native credential adapters
and secret-free evidence, review the final diff skeptically, resolve every
documentation-ledger entry, and move every RFC card to `done/`.

Mark RFC-BPR-0002 Auto-Verified only when GitHub.com, recorded GHES 3.18, CLI
broker, native HTTPS, all three native credential stores, core/host/Qt
boundaries, security assertions, documentation, and tracker state agree.

Every passing claim must name a reproducible command or interaction, a
falsifiable success condition, and retained secret-free evidence. Closeout
also requires fresh `dev` and `core-only` matrices, target-native builds and
network/vault checks, architecture/runtime/binary-dependency gates,
documentation validation over the immutable RFC base, fixture and mutation
audits, source-pack validation, and `git diff --check`.
