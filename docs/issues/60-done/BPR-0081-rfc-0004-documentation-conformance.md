---
schema: "implement-rfc.card.v1"
id: "BPR-0081"
title: "Complete RFC-BPR-0004 documentation and conformance"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0080"]
---

# Complete RFC-BPR-0004 documentation and conformance

Reconcile implemented core/host/Qt/composition APIs, state authority,
threading, limits, diagnostics, build/test guidance, fixture catalog,
rendered/manual protocol, product UX, downstream contracts, agent
instructions, and every direct claim. Classify immutable-base documentation
impact from `243295a70756b2e63a785ed11b14647b6b0a4c6d`.

Run dependency/architecture scans proving core and workers remain Qt/provider
free, Qt alone owns measured pixels/current scroll, and no old follower/fake-
row/competing-mapping design survives. Preserve GitHub Actions counts at 0/0.

## Implemented surface

- Reconciled the README, architecture/conformance plan, product UX, build/test
  guide, fixture catalogs, rendered/manual protocol, RFC ledger, tracker, and
  composition-root instructions with BPR-0072 through BPR-0080.
- Classified direct documentation impact from predecessor
  `243295a70756b2e63a785ed11b14647b6b0a4c6d` while preserving the exact post-
  import immutable RFC gate from
  `0ec1136a7ce418b11480d7ae90faee2ec59f96d1`.
- Added an explicit worker-executor-infrastructure Qt/provider boundary and
  hostile contract cases for directly owned widgets, Qt models, provider and
  transport/profile objects, and relative adapter includes. Host-owned effect
  closures remain the only capability execution; the compiled worker fixture
  proves accepted work runs off the calling thread.
- Added exact serial/headless commands, markers, and artifact paths for the
  selected scale and public-composition evidence so the suite cannot fan out
  redundant visible application instances.
- Kept target-native Windows, the owner-activated System-theme transition,
  final two-direction rendered replay, and RFC-BPR-0005 presentation polish
  explicitly open and assigned the final native/replay gates to BPR-0083.

## Direct verification

- `core_diff_{model,compute,semantics,transition}` and
  `application_diff_host`: 5/5 pass from the Qt-free preset.
- `core_dependency_boundary`, `core_boundary_contract`,
  `architecture_boundaries`, `architecture_boundary_contract`, and
  `application_worker_executor`: 5/5 pass; the hostile architecture contract
  itself passes 105/105 cases.
- `VerifyCoreBoundary.cmake`, documentation validation, `git diff --check`,
  and the immutable RFC-tree diff pass.
- Tracked/on-disk `.github/workflows` counts remain 0/0. No GitHub Actions and
  no GitHub/GHES mutation were used.
- Seven existing final-source selected workbench/link-map/public rasters are
  nonempty with the recorded dimensions; none were regenerated. This slice
  launched no application instances and made no new UI claim.
- Independent closeout review reports no blocker, nonblocking risk,
  verification gap, or unsupported suspicion.
