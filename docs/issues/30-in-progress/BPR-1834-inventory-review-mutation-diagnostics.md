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
