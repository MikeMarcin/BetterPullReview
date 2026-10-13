---
schema: "implement-rfc.card.v1"
id: "BPR-1454"
title: "Fix the Git metadata readers: repository format, config syntax, ignore case, POSIX classes, index checks and HEAD"
status: "In Progress"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0083"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
---

# BPR-1454: Fix the Git metadata readers: repository format, config syntax, ignore case, POSIX classes, index checks and HEAD

Immediate fixes 1–7 of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md), filed against
[RFC-BPR-0054](../../rfcs/RFC-BPR-0054-workspace-source-capture-and-revision-material.md)
and not gated by the evaluation. They restore RFC-BPR-0054's "truthful handling
rather than a lossy fallback" for the divergences observed on 2026-10-13.

**Excluded, waiting for the owner's Q2:** the reftable part of fix 1. A checkout
with `extensions.refStorage = reftable` is detected and named by the reader, but
its handling (refuse, refuse indefinitely, or capture with a marker) is not
chosen here; [[BPR-1461]] owns it.

Platforms: the change is portable and its tests run in every preset. This card is
verified on macOS; [[BPR-1462]] and [[BPR-1463]] record the Windows and Linux runs.

## Acceptance criteria

- **BPR-1454:AC1:** Capture and reference resolution read `core.repositoryformatversion` and every `extensions.*` key with Git's rules and refuse as `unsupported`, with a named reason, a version above 1, an unknown extension in a version-1 repository, a version-1-only extension in a version-0 repository, and an unrecognised `objectFormat` or `refStorage` value. The allowlist is confirmed against Git's documentation and setup code; `refStorage = reftable` is reported, not refused, pending Q2.
- **BPR-1454:AC2:** The keys the readers use are parsed with Git's config syntax: sections, subsections, case rules, quotes, escapes, inline `#` and `;` comments and continuation lines; a config Git refuses to read is refused. Includes stay unread.
- **BPR-1454:AC3:** `core.ignoreCase` from the repository config (and `config.worktree` under `extensions.worktreeConfig`) folds case in ignore and `filter` attribute matching the way Git's wildmatch does.
- **BPR-1454:AC4:** The twelve POSIX bracket classes match as in Git, and an unknown class or unterminated bracket makes the pattern match nothing.
- **BPR-1454:AC5:** The index trailer is verified with the repository's hash algorithm; an all-zero trailer (`index.skipHash`) is accepted.
- **BPR-1454:AC6:** An index extension whose signature does not start with `A`–`Z`, other than `link` and `sdir`, is refused.
- **BPR-1454:AC7:** Capture reports a loose-reference failure other than `missing` instead of consulting `packed-refs`, and follows one level of symbolic reference, sharing one resolver with outgoing resolution.
- **BPR-1454:AC8:** Each fix is accepted by a workspace contract test that builds its repository with real Git and fails on the readers at RFC base `9a4f61976`; core unit tests, the `core-only` preset, the architecture checks, format and changed-line tidy pass.

## Continuation

Imported 2026-10-13 under mandate `git-metadata-readers-r1`; nothing is measured or accepted yet.
