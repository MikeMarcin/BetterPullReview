---
schema: "implement-rfc.card.v1"
id: "BPR-1447"
title: "Supply feedback body digests from platform crypto behind a host port"
status: "Ready"
rfcs: ["RFC-BPR-0080"]
depends_on: ["BPR-1446"]
priority: "P2"
platforms: ["mac"]
metadata: {"mandate_revision":"hashing-replacement-r1","milestone_plan":{"id":"hashing-replacement-v1","path":"docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-1446--BPR-1453--810e9a5c8803.json"}
---

# BPR-1447: Supply feedback body digests from platform crypto behind a host port

RFC: [RFC-BPR-0080](../../rfcs/RFC-BPR-0080-hashing-replacement.md), milestone
`host-digest-port-macos`.

## Outcome

The first host-port site the RFC names: BPR-1349's feedback bodies. The host
supplies each body's SHA-256 with the observation, computed off the window
thread by platform crypto in an adapter (CommonCrypto on macOS), so the core
classifier does not hash bodies on the window thread. Platforms without a
measured backend keep the core digest. Digests are byte-identical, so nothing
persisted changes.

Adopt only if the RFC's host-port rule passes on this site: inputs of 1 MiB
or more hash at least 3x faster than core on a SHA-extension target, and the
site cuts its window-thread hashing by at least 10 ms at 10k or moves it off
the window thread. Otherwise record the rejection and keep core.

## Acceptance

- **AC1 (RFC AC02):** Port digests equal the core digest for the known answers
  and the differential inputs, and every existing digest-using row passes.
- **AC2 (RFC AC05):** `application_feedback_counter_costs` first observation at
  1k and 10k, `application_large_review_responsiveness` and
  `application_first_tab_comparison` before and after, with the bytes hashed on
  the window thread counted; no row worse than its run-to-run spread.
- **AC3 (RFC AC06):** Platform crypto sits only in an adapter behind the port;
  core and application stay free of OS crypto; `dev` and `core-only` build and
  the architecture checks pass.
- **AC4 (RFC AC04):** The report records the site's adopt or reject decision
  with the measurements that support it.
