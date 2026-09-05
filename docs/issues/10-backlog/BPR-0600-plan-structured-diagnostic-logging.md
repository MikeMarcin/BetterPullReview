---
id: BPR-0600
title: Inventory diagnostic boundaries and plan RFC-BPR-0027
status: Backlog
rfc: RFC-BPR-0027
milestone: Post-Pack v2
depends_on: []
---

# Inventory diagnostic boundaries and plan RFC-BPR-0027

## Objective

Plan the bounded implementation of [RFC-BPR-0027](../../rfcs/RFC-BPR-0027-structured-diagnostic-logging-verbosity-privacy.md),
starting with useful local diagnostics for BPR-0559's GraphQL failure.
The owner requested an RFC, not implementation. This entry is unstarted.

RFC base: `55a3eb6c5d0f37cbcb4e35a33c2c90467a6f13ce`.
Requested RFC set: RFC-BPR-0027; workstream position: 1 of 1.
Planned cards: BPR-0600, BPR-0601.
Closeout: [BPR-0601](BPR-0601-close-rfc-0027.md).

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
