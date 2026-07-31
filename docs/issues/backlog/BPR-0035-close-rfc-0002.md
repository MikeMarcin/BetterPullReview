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

Mark RFC-BPR-0002 Auto-Verified only when GitHub.com, recorded GHES 3.18,
native HTTPS, all three native credential stores, the fixed zero-launch CLI
boundary, core/host/Qt boundaries, security assertions, documentation, and
tracker state agree. Positive GitHub CLI execution is deferred to
RFC-BPR-0015 and is not a closeout criterion.

Every passing claim must name a reproducible command or interaction, a
falsifiable success condition, and retained secret-free evidence. Closeout
also requires fresh `dev` and `core-only` matrices, target-native builds and
network/vault checks, architecture/runtime/binary-dependency gates,
documentation validation over the immutable RFC base, fixture and mutation
audits, source-pack validation, and `git diff --check`.

The product-owner-approved Windows deferral does not block available-target
implementation, security review, documentation, simplification, or work on
later RFCs. BPR-0036 remains open until its real-machine evidence exists and
therefore blocks only the affected Windows claims and this final
Auto-Verified transition.

## Available-platform checkpoint

At `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`, BPR-0016 through
BPR-0034 plus BPR-0037/BPR-0038 are in `done/`. The current RFC-owned macOS
matrix is 46/46, `core-only` is 17/17, and the final simplification's focused
AddressSanitizer/UndefinedBehaviorSanitizer matrix is 7/7. Documentation,
architecture, security, tests-disabled Release, native macOS, pinned Linux,
original-pack, workflow-absence, and diff gates are recorded in the
verification ledger.

This checkpoint is not an RFC closeout and changes no `Planned` Windows row.
BPR-0036 remains open for the product owner's real Windows run. This card
remains in `backlog`, and RFC-BPR-0002 remains `Proposed`.
