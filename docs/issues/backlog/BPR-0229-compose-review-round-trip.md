---
id: BPR-0229
title: Compose review synchronization and the mutation coordinator
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0228]
---

# Compose review synchronization and the mutation coordinator

The core, coordinator, synchronization, and Qt editor components exist, but
the shipped application constructs none of them.

## Required final state

- The shipped editor dispatches semantic draft and publish intents and holds no
  provider.
- The composition constructs the coordinator with an explicit designated
  fixture target for automated evidence; no test gains a live target.
- Publish, retry, stale-head refusal, capability fallback, and refresh return
  through typed completions and GitHub remains the shared authority.
- A public fixture trace proves QAction/gesture to proposal, coordinator,
  provider fixture, result event, refresh, and Qt projection.
- The standing live-mutation-isolation gate remains green and non-vacuous.

Live web-UI, mixed-client, and acting-identity acceptance remain in BPR-0142,
BPR-0143, and BPR-0144.
