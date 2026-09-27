---
schema: "implement-rfc.card.v1"
id: "BPR-0600"
title: "Inventory diagnostic boundaries and plan RFC-BPR-0027"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: []
milestone: "Post-Pack v2"
---

# Inventory diagnostic boundaries and plan RFC-BPR-0027

## Objective

Plan the bounded implementation of [RFC-BPR-0027](../../rfcs/RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md),
starting with useful local diagnostics for BPR-0559's GraphQL failure.
The owner subsequently authorized the basic foundation. This entry records
that bounded plan; the full product remains incomplete.

RFC base: `55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`.
Requested RFC set: RFC-BPR-0027; workstream position: 1 of 1.
Planned cards: BPR-0600, BPR-0601, BPR-0602, BPR-0603.
Closeout: [BPR-0601](../20-ready/BPR-0601-close-rfc-0027.md).

## Acceptance

- Inventory provider error disposal, credential boundaries, host lifetimes,
  application roots, protected logging guards and existing Qt diagnostic controls.
- Specify typed event/sanitizer/privacy contracts, Quill version/provenance and
  bounded queue/file/lifecycle configuration without relying on library defaults.
- Demonstrate the planned first fixture distinguishes an unfamiliar GraphQL
  error through a useful sanitized record, not a generic enum written to disk.
- Map every RFC criterion to owned slices and focused checks plus a composed
  application path; allocate only required cards and update closeout dependencies.
- Keep Settings/profile integration optional until those systems exist; record
  exact native-platform and attended gates separately.

## Verification record

Not run; implementation planning awaits a later explicit request. Registration
checks only documentation structure, links, IDs and status. No Quill dependency,
source implementation, credential operation or live request was added.

Registration verification: `python3 scripts/verify_documentation.py --root .`
passed; `python3 tests/scripts/verify_documentation_tests.py` passed all 62 tests;
`git diff --check` passed. These are documentation checks only. AllTheThings was
read without edits; no temporary worktree, source copy or runtime log was created.

## Foundation authorized

The owner requested implementation of basic Quill logging now and deferred
rotation, anonymization and Settings controls. The integrator is planning that
bounded phase within RFC-BPR-0027; the full RFC remains open.

## Completed foundation plan

Read-only scouting located capability response disposal in
`github_capability_probe.cpp`, transport-owned credential bytes in
`native_https_transport.cpp`, provider factory/state injection, and the app's
existing data-root and lifetime composition. The implementation keeps Quill in
a dedicated effect adapter, with an application diagnostic port, not in core,
Qt presentation or the transport. The transport may invoke a private pure
summary sanitizer while its active credential is available; it does not log
or forward credentials to the sink. Ordinary provider results carry only the
bounded sanitized summary. The existing response remains unchanged for policy.

Quill v13.0.0's public tag archive was inspected, including its MIT license.
Archive SHA-256 `88b4a1542125577a4d51cf444c51e34d63618c422ba6a4fa9bd23894b49d696b`;
2,061,083 bytes. Reuse the audited CPM acquisition/cache pattern with explicit
upstream tests/examples/install disabled and preserve bundled notices. The
archive in `build/.cpm-cache/quill-v13.0.0.tar.gz` is a dependency cache, not
verification evidence. Do not use a floating package or implicit system Quill.

BPR-0602 owns the bounded implementation and direct adapter/provider-to-file
fixture plus composed startup/shutdown path. BPR-0603 owns the deferred full
criteria, whose planned checks remain on BPR-0601; update these after later
slice allocation. No full matrix or live request is required to close 0602.
Documentation registration checks pass; implementation verification remains
with 0602. This planning completion does not establish logging or fix 0559.
