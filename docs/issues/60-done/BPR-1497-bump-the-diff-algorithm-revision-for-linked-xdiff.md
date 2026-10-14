---
schema: "implement-rfc.card.v1"
id: "BPR-1497"
title: "Bump the diff algorithm revision for linked xdiff"
status: "Done"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1485"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r3","source":"Owner answer 2026-10-13 (\"Sure\") to bumping supported_diff_algorithm_revision after BPR-1485 adopted linked xdiff; release-critical for 0.2.0"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1ee3b903a","evidence":{"kind":"observation","detail":"Inventory on the card: no store persists a computed diff (diff host cache is in-memory and keyed by the whole key). Persisted revision-derived identities are the comparison digest and file material in the review ledger (receipts, defects), recovery store (drafts), private review store (notes, threads, binding, agent sessions), review tours and window restoration; Review Cycle store and epoch catalog hold none; the Review Cycle comparison identity is computed per view and never stored."}},{"criterion":"AC2","outcome":"pass","revision":"1ee3b903a","evidence":{"kind":"command","detail":"supported_diff_algorithm_revision = 2. core_diff_transition and application_diff_host now refuse a revision-1 key (unsupported_algorithm; no cache or worker crossing) and pass; both fail on main's core sources. Result validation requires Alignment.Key == request key; epoch host refuses a non-current selected-diff context; review_cycle_comparison writes the live revision."}},{"criterion":"AC3","outcome":"pass","revision":"1ee3b903a","evidence":{"kind":"command","detail":"comparison_digest and file_material keep revision 1's bytes (persisted_algorithm_slot = 1). test_stored_identities_survive_the_algorithm_revision pins both to vectors captured before the bump (fbdfe1e6...88ae, 606e433a...badf) and carries a receipt and a line anchor across; it fails on main and fails on a naive bump. core-only non-perf 269/274 (5 pre-existing, outside the card); coding_format_check passes; changed-line tidy clean. No data lost, no migration needed, no user-visible change."}}]
---

# BPR-1497: Bump the diff algorithm revision for linked xdiff

[BPR-1485](BPR-1485-adopt-linked-xdiff-as-the-primary-line-matcher.md)
replaced the first-party primary matcher with linked xdiff and kept
`supported_diff_algorithm_revision` at 1 pending the owner. On 2026-10-13 the
owner answered "Sure" to bumping it. xdiff aligns differently, so a result
computed by the old engine must not be served as a current one. Stored review
data (drafts, line comments, private notes and threads, file receipts, tours,
Review Cycle records) must stay usable: re-anchored, migrated or clearly
marked, never silently lost or corrupted.

## Acceptance

- **AC1:** Every store and cache that holds an identity derived from the
  algorithm revision is inventoried, with what happens to it at revision 2.
- **AC2:** `supported_diff_algorithm_revision` is 2. A comparison key or a
  computed result at revision 1 is refused by the diff transition, the diff
  host, the epoch host's selected-diff context and result validation, so it is
  recomputed rather than served; the Review Cycle comparison identity carries
  revision 2.
- **AC3:** Stored review data written under revision 1 keeps matching after
  the bump, with tests that fail on the code before it; no data is lost and no
  user-visible behaviour gets worse.

## Built and verified (macOS arm64, 2026-10-13)

Implementation `1ee3b903a` on `claude/bpr-1497`, from `main` at `8ddd55bf0`.

**AC1, inventory.** No store or cache persists a computed diff: alignments,
indexes and intraline results live only in the diff host's in-memory cache
(looked up by the whole `file_comparison_key`, algorithm included) and die with
the process. What is persisted is the comparison digest and the file material,
both of which encoded the algorithm revision:

| Store | What it keeps | At revision 2 |
|---|---|---|
| Review ledger | file receipts (read, seen, accepted, excluded) keyed by `file_material`; defects anchored by `source_line_ref` | unchanged, still matches |
| Review mutation recovery store | unpublished drafts and replies, anchored by `source_line_ref` | unchanged, still matches |
| Private review store | private notes (`local_note_attachment`), threads (file, line, boundary, span, group, segment, move subjects), the binding's comparison, retained agent sessions | unchanged, still matches |
| Review tour store | tour sources' first and last `source_line_ref` | unchanged, still matches |
| Window review restoration | selection and preferred line comparison | unchanged, still matches |
| Review Cycle store | rounds, views and named comparisons by round and pair id; no diff digest | unaffected |
| Epoch catalog | Git objects and epochs; no diff digest | unaffected |
| Review Cycle comparison identity (`BPR-cycle-file-comparison-v1`) | computed per view, never stored | now carries revision 2 |

A line anchor names a file line on one side, not an alignment, so it stays
correct under a different matcher. Segment, group and move identities derive
from the comparison digest plus kind, spans and ordinal; one written under
revision 1 matches only an identical segment under xdiff, and otherwise is
unresolved exactly as it already was after BPR-1485 changed the matcher under
revision 1.

**AC2, revision.** `supported_diff_algorithm_revision` is 2
(`diff_compute.hpp`). Refusal of a revision-1 key or result is the existing
code path, now exercised with revision 1: `diff_transition`
(`unsupported_algorithm`), the diff host's `preparable` and `validate_staged`
(the result's key must equal the request's), the epoch host's selected-diff
context and `review_cycle_comparison.cpp`'s identity, which writes the live
revision.

**AC3, compatibility.** `comparison_digest` and `file_material` no longer
encode the key's revision; the slot keeps the value 1 that revision 1 wrote
(`persisted_algorithm_slot`, `diff_model.cpp`), the same technique BPR-0531
used for the authority mode. No migration is needed because no stored byte
changes, and nothing is lost or marked. New
`test_stored_identities_survive_the_algorithm_revision` (`core_diff_model`)
pins both digests of a revision-1 key to the vectors captured from the engine
before the bump (`fbdfe1e6…88ae` and `606e433a…badf`), carries an accepted
receipt and a line anchor across, and checks the current key still differs
from the revision-1 key. The cache-key variant lists no longer expect the
algorithm to move either digest.

- Fails on old code: with `main`'s core sources the new tests fail
  (`core_diff_model`: "linked xdiff computes algorithm revision 2";
  `core_diff_transition`: "caller cannot select an unstamped algorithm
  revision"; `application_diff_host`: "unsupported algorithm revision cannot
  cross into cache or worker"). With the revision bumped but the digests
  left encoding it, `core_diff_model` fails "a comparison digest stored by
  revision 1 still names the same comparison".
- `core-only` (Debug, `-g0`), excluding benchmark, large and perf tests: 269 of
  274 pass; the five failures are pre-existing and outside this card (three
  from the worktree's missing `.agents` links, `contained_temporary_usage` on
  `tests/hash_evaluation`, `msvc_secure_crt`). `coding_format_check` passes;
  changed-line tidy reports no findings. `application_diff_host` failed once
  in 0.86 s right after a rebuild, under the machine's concurrent release gate,
  and passed six reruns; its output was not captured.
- `src/app` is unchanged: it uses the constant symbolically, so the
  application review round-trip composition contract needs no reseal. Qt
  (`dev`) tests were not built here.
