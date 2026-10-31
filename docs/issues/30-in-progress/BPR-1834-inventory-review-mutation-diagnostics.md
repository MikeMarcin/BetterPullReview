---
schema: "implement-rfc.card.v1"
id: "BPR-1834"
title: "Inventory the injected review-mutation diagnostic implementations"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1828"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue":"BPR-1811","mandate":"release-v041-full-stable-N8-oracle-followup","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","frozen_release_source":"d9d4a822dbf603c7bb22c83574f5a64d2bee5645","integration":"Future-main follow-up only; frozen release source, publisher and producers remain unchanged."}
---

# Inventory the injected review-mutation diagnostic implementations

N8 Release qualification records a failing `live_mutation_isolation` row on
macOS and Windows. BPR-1828 added an application-layer diagnostic decorator
and its credential-free scripted test provider, but omitted their exact paths
from the guard's implementation inventory. Neither constructs the live provider.
The registered failures remain raw failures; corrected external full-oracle
results are separate evidence against unchanged frozen release inputs.

## Acceptance

- AC1: Add only the two reviewed implementation paths to clause A and document
  their ownership. Preserve the full binary/object unreachability scan, required
  fake and coordinator positive controls, and exact factory-site/count checks.
- AC2: Exercise the full corrected oracle against the frozen candidate's native
  build contexts. Add focused controls proving the two permitted implementations
  are admitted, nearby unlisted implementations fail, and either permitted path
  calling the live factory still fails. Retain original failures separately.
- AC3: Run focused guard contracts and documentation checks, commit the allocated
  counter and receipt with this follow-up, and leave the commit reviewable for
  later main integration without advancing the frozen publisher or producers.

## Current evidence and integration boundary

The original N8 macOS and Windows registered rows both fail clause A on exactly
these two paths. Source inspection confirms the application decorator owns an
injected provider and the test implementation returns scripted results; neither
adds a live factory site. The frozen candidate remains D9, and all 30 published
producer asset hashes remain unchanged.

A corrected external full oracle passes on the actual macOS Release manifest:
498 non-production images, all three predicates, 11 permitted implementation
paths and the unchanged three factory sites. Its original network wrapper,
environment projection, working directory and 300-second budget are retained.
The original 29 hostile/positive contracts also pass in that context. The new
future-main guard has the same complete AST as that external oracle.

The expanded contract suite passes 32 tests. It admits both exact diagnostic
paths, refuses adjacent unlisted paths, and refuses live-factory construction
from either added path. Two in-memory removal controls each make the new
positive test fail on the removed entry. The documentation structure gate
passes. Additional native build contexts remain pending and will be recorded
separately; this card is not closed by the macOS Release result.

External receipts: `main-refresh-r3-live-mutation-oracle-validation-r1/receipt.json`
and `bpr1834-missing-inventory-hostile-controls-r1.json` under the declared
external state. Original registered failures remain failed; no new waiver,
producer change or cohort is implied. This follow-up must not advance main or
the frozen publisher until the stable operation's compatibility guard is
reconciled.
