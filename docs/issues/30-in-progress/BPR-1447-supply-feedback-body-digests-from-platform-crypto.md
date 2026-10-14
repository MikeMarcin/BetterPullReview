---
schema: "implement-rfc.card.v1"
id: "BPR-1447"
title: "Supply feedback body digests from platform crypto behind a host port"
status: "In Progress"
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

## Built and verified (macOS arm64, 2026-10-13)

Implementation `004910e4b`. **Decision: adopt on macOS** (the RFC's
host-port rule: CommonCrypto 7.11x to 8.54x core on 1 MiB or more; at 10k the
first observation's window-thread time falls from about 33 ms to about 17 ms
and no body is hashed on the window thread). Full tables are in the report's
[Host port](../../verification/RFC-BPR-0080.md#host-port) section.

**As built.** `application::body_digest_port` (no OS types) with
`core_body_digest`; `adapters::github::platform_body_digest` uses `CC_SHA256`
on macOS and the core digest elsewhere until [[BPR-1448]] and [[BPR-1449]].
The window read host digests a fetched snapshot's feedback bodies in the
provider completion callback on the provider's effect worker, before the
snapshot reaches the decision lane; `github_feedback_body_digests` rides on the
snapshot (compares equal, so snapshot comparison is unchanged) and
`observe_feedback_sources` attaches a digest only to the post id and body
length it was taken from. The core's `recognize_feedback_body(body, segment,
digest)` uses a supplied digest and hashes anything without one; the counters
add `BodiesDigestSupplied` and `BytesHashed`. A snapshot restored from cache
carries no digests and keeps core hashing. `architecture_boundaries` allows
CommonCrypto SHA-2 only in the adapter file and the two existing
designated-live identity files.

**Checks.** `application_body_digest_port` (every backend against the core for
FIPS answers, lengths 0 to 200, block and 1 MiB edges, seeded inputs; supplied
digests give identical units and counts; missing or mismatched digests fall
back), 18 feedback rows, `core_dependency_boundary`, `core_boundary_contract`,
`architecture_boundaries`, `architecture_boundary_contract`, a `core-only`
build of core and application, format and changed-line clang-tidy, and the
documentation gate pass. The round-trip composition contract is resealed and
passes. `application_first_tab_comparison` passes before and after with phases
flat or lower; `application_large_review_responsiveness` fails before and
after on main's keyboard reveal, with its phases recorded (Overview column
phases 3 to 8 ms before, 7 to 8 ms after, within the before spread, on a code
path this change does not touch). Windows and Linux backends are not built
here.
