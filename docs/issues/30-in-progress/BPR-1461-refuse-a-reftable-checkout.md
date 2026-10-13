---
schema: "implement-rfc.card.v1"
id: "BPR-1461"
title: "Refuse a reftable checkout under the owner's Q2 answer"
status: "In Progress"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1461: Refuse a reftable checkout under the owner's Q2 answer

The owner answered Q2 of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md)
on 2026-10-13 with option (a): refuse a reftable checkout now under fix 1, and
decide later, under C3, whether to read it. The refusal is implemented with fix 1
in [[BPR-1454]]; this card holds AC07's evidence. Reading reftable, if the
evaluation chooses it, is a C3 verdict in [[BPR-1460]] and would need a follow-up
card.

Before the fix, Git 2.50.1 wrote `HEAD` as `ref: refs/heads/.invalid`, made
`refs/heads` a regular file and set `extensions.refStorage = reftable`; capture
then succeeded with an empty `HeadCommit` that looked like an unborn branch.

## Acceptance criteria

- **BPR-1461:AC1:** A reftable checkout is refused with the named reftable reason by capture, the rebind material check and outgoing reference resolution, and never produces an empty `HeadCommit` that looks like an unborn branch; a workspace contract test built with real Git (2.45 or newer, skipped with the version found otherwise) shows it, and fails on the readers at RFC base `9a4f61976`.

## Continuation

Done on `ae60c0250` (macOS, Git 2.50.1) with [[BPR-1454]]; evidence in the
[verification ledger](../../verification/RFC-BPR-0083.md). Reading reftable stays
with C3's evaluation in [[BPR-1460]].
