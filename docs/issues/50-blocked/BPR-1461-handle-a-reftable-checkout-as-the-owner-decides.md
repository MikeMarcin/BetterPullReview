---
schema: "implement-rfc.card.v1"
id: "BPR-1461"
title: "Handle a reftable checkout as the owner decides under Q2"
status: "Blocked"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P1"
platforms: ["mac", "win", "linux"]
blocked_by: "owner-reftable-decision"
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1461: Handle a reftable checkout as the owner decides under Q2

Q2 of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md) is open with the owner: (a) refuse a
reftable checkout now under fix 1 and decide whether to read it under C3
(the RFC's recommendation), (b) refuse it indefinitely, or (c) capture it with an
explicit "head unavailable" marker, which changes the manifest and its codec.

Until the owner answers, [[BPR-1454]]'s reader names `refStorage = reftable`
but capture keeps today's behavior: the loose read of `refs/heads/.invalid`
fails as missing, there is no `packed-refs`, and `HeadCommit` is empty. That is
the divergence the RFC observed, kept only because no option may be picked
without the owner.

## Acceptance criteria

- **BPR-1461:AC1:** A reftable checkout produces Git's HEAD commit, or the refusal or marker the owner chose under Q2, and never an empty `HeadCommit` that looks like an unborn branch; a workspace contract test built with real Git (2.45 or newer, skipped with the version otherwise) shows it.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
