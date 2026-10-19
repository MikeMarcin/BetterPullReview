---
schema: "implement-rfc.card.v1"
id: "BPR-1631"
title: "Measure fourteen Windows containment registrations"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"18dff0d12623520da114392f92d18c4c21536c67","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1631: Measure fourteen Windows containment registrations

The coordinator explicitly added this bounded repair to the same pending release
repair milestone. The shared native slot permits focused Ubuntu then Windows
work; full normal suites, packaging, signing and publication remain coordinator
work. The frozen Mac baseline stays untouched. No credential changes or signing
are in scope.

## Acceptance

- AC1: Exercise all14 actual current direct and contained commands with exact manifest arguments/environment/deadlines, preserve results, and replace only observed finite reasons or retire unnecessary exemptions.
- AC2: Repair only reproduced portability failures without relaxing product policy, assertions, deadlines, network capabilities or cleanup ownership; rerun affected actual courses.
- AC3: Pass verifier unit tests and mandatory native manifest, retain original failures, self-review/document/commit and return quiescent source to the coordinator.

Evidence and remaining qualification are bound by the ignored
`.mask/sessions/release-v0.3.0/repair-handoff.json`.

## Reproduced direct-course portability defects

The first actual `c5ca34be1` measurement retains three direct failures:

- SQLite reset: `profile-runtime.lock` appears after the initial snapshot. The Windows lifetime lease deliberately creates a persistent private lock file; POSIX locks the root directory. The fixture now acquires/releases an initial genuine lease before populating/snapshotting, and uses the shared random owner-private scratch root rather than its old clock-based directory.
- Composed reset: the Windows identity stand-in returned `1` for every existing store, so a removed/recreated store could never differ. The test now uses the existing independent platform volume/object identity helper, and refuses an unreadable identity for an existing file; exact preserved/replaced comparisons remain.
- Recovery: a healthy startup refuses the registry because `chmod` does not write the product's Windows private DACL. The Python course now uses the existing shared `bpr_narrow_owner_private` helper on its owned scratch and profile roots. The later POSIX-only process cleanup remains unchanged pending actual execution after this first refusal is repaired.

These are test setup/oracle changes only. All original assertions, production
storage/admission policy, UI, deadlines and network grants remain unchanged.
Focused corrected native courses and final containment classification remain
pending.
