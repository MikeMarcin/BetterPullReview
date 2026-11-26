---
schema: "implement-rfc.card.v1"
id: "BPR-2416"
title: "The next release's notes cover every owner-visible change since 0.5.3 and say what an earlier version cannot read"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"release","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"f035b196c","reservation":"docs/issues/_reservations/.json","origin":"coordinator, 2026-11-26: draft the next stable release's notes after 0.5.3","related_cards":["BPR-2377","BPR-2379","BPR-2396","BPR-2401","BPR-2395"],"acceptance":["AC1","AC2","AC3"]}
---

# BPR-2416: Release notes for the build after 0.5.3

## Why

Stable 0.5.3 (tag `0.5.3`, build 99) was published 2026-11-24. `main` has
233 commits since, carrying BPR-2368..BPR-2414 (Done) and BPR-2415 (Parked,
RFC-BPR-0130 closeout). The next release needs its user-facing change list
and the downgrade limits three of those cards recorded.

## Change

- `docs/releases/v0.5.4.md` (the version `CMakeLists.txt` on `main` already
  names): `## Changes since [0.5.3]` with New, Changed and Fixed, written to
  `.agents/skills/publish-release/references/release-notes.md`, and a
  `## Going back to an earlier version` section that the release body's
  notice links.
- The release body draft (notice, download table with `{{…}}` URL slots,
  Ubuntu line and the same change list) is kept outside source for the
  publisher; its slots are filled at publication.

Sources read: every card BPR-2368..BPR-2415 in `docs/issues/60-done` and
`40-parked`. BPR-2366 and BPR-2367 shipped in 0.5.3 (both in the 0.5.3 tag's
tree and its published body) and are not repeated.

Left out as not user-visible: BPR-2374, 2380, 2386, 2403, 2408 (test and
boundary-rule repairs), 2407, 2409..2412 (one-transcript restructuring with
no visible change). BPR-2387 (BetterPullReview's own Git isolated from a
retained workspace's configuration) is not listed as a security fix: the
exposure arrived with BPR-2379 (575a992ea), which no published tag contains.

## Acceptance

- AC1. Every owner-visible outcome of BPR-2368..BPR-2414 has an entry, in the
  standard's shape (no internal vocabulary, one outcome an entry).
- AC2. The downgrade limits are stated in plain words: agent sessions in
  their own store (BPR-2377), settings schema 40 refused by 0.5.3
  (BPR-2396), moved and compacted workspace ownership records unreadable by
  earlier versions (BPR-2379, BPR-2396).
- AC3. No version bump, tag, release or push.

## Evidence

- Settings: `workbench_settings_schema_version` is 39 at `0.5.3` and 40 on
  `main`; `establish_schema` returns `schema_mismatch` for a newer file.
- `scripts/verify_documentation.py --root .` could not run in the task
  worktree (MASK-deployed `board_model` absent); run it on `main` at landing.

## Owner judgement

- Version: 0.5.4 follows `CMakeLists.txt`. The release adds several features
  and a downgrade limit; choosing 0.6.0 instead renames the file and links.
- The list has 21 entries against the standard's "about fifteen"; the
  coordinator asked for every owner-visible change. Cut candidates, least
  noticeable first: the inline-code colour, the findings chart position, the
  proposed tour stop row, the filter toggle look.
