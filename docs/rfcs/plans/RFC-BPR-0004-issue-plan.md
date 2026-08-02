---
provisional_id: PACK-ISSUE-BPR-0004-REVISION-2
rfc: RFC-BPR-0004
status: Imported
live_ids: [BPR-0071, BPR-0072, BPR-0073, BPR-0074, BPR-0075, BPR-0076, BPR-0077, BPR-0078, BPR-0079, BPR-0080, BPR-0081, BPR-0082, BPR-0083]
milestone: M1 Native Read-Only Workbench
depends_on_rfc: [RFC-BPR-0003]
rfc_base_sha: 243295a70756b2e63a785ed11b14647b6b0a4c6d
replacement_pack: betterpullreview-rfc-bpr-0004-diff-alignment-scroll-replacement-v1
---

# RFC-BPR-0004 Replacement Issue Plan

The live board allocates BPR-0071 through BPR-0083 for these minimum slices;
`docs/issues/NEXT_ID` now advances past that range. This imported plan remains
source material, while folder placement on the live issue board is status.

| Live ID | Slice |
|---|---|
| BPR-0071 | replacement import and reconciliation |
| BPR-0072 | comparison identity and canonical alignment records |
| BPR-0073 | diff canonicalization, secondary pairing, and intraline ranges |
| BPR-0074 | mapping, grouping, navigation, and detail semantics |
| BPR-0075 | host request/cache/cancellation/stale-result lane |
| BPR-0076 | Qt pane measurement and max-extent axis |
| BPR-0077 | shared comparison scroll controller and scrollbar |
| BPR-0078 | custom-painted `DiffLinkMap` |
| BPR-0079 | composed workbench integration |
| BPR-0080 | scale and high-frequency interaction evidence |
| BPR-0081 | documentation and architecture conformance |
| BPR-0082 | simplification |
| BPR-0083 | final direct verification and closeout |

## 1. Import and reconcile the replacement

**Final closeout depends on:** RFC-BPR-0003 Auto-Verified. Product-owner
direction permits implementation to proceed while RFC-BPR-0003's explicit
nonblocking closeout cards remain open; it does not permit an out-of-order
Auto-Verified transition.

Scope:

- record the immutable RFC-BPR-0004 base SHA;
- replace the live RFC in place;
- update RFC indexes and combined documents;
- reconcile old RFC-BPR-0004 cards and verification rows;
- import architecture, UX, migration, fixture, and claim obligations;
- commit the planning slice.

Acceptance:

- exactly one active RFC-BPR-0004 exists;
- no downstream RFC is renumbered;
- no old card still permits follower-scroll or synthetic blank-row behavior;
- live paths, board lanes, and next IDs are correct.

## 2. Implement comparison identity and canonical alignment records

Scope:

- exact comparison/cache key;
- half-open line spans and line/boundary references;
- alignment segment, line pair, change group, and move relation records;
- canonical stable IDs;
- validation of coverage, bounds, and monotonicity.

Tests:

- table-driven edge cases;
- generated/property sequences;
- deterministic replay and stable identity;
- invalid segment rejection.

## 3. Implement deterministic diff canonicalization and secondary pairing

Scope:

- preserve or adapt the existing line-diff engine;
- canonical segment output;
- monotonic secondary line pairing;
- replacement refinement at internal anchors;
- intra-line ranges;
- move metadata without nonmonotonic primary alignment.

Tests:

- exact, insertion, deletion, unequal replacement, repeated lines, whitespace, final newline, Unicode, and move fixtures;
- deterministic tie breaking;
- weak-pair rejection.

## 4. Implement mapping, grouping, navigation, and detail inputs

Scope:

- exact/similar/span/boundary/move/unavailable mapping results;
- deterministic change groups;
- previous/next navigation;
- selected semantic line/change state;
- lower line-over-line detail projection.

Tests:

- no false exact counterpart for one-sided lines;
- group and link-map identity agreement;
- navigation wrap/end policy according to workbench command design;
- selection preservation by stable ID.

## 5. Implement host diff request, cache, cancellation, and stale rejection

Scope:

- blob materialization and text/binary status;
- immutable worker inputs;
- exact cache keys including options and algorithm revision;
- tagged completion events;
- cooperative cancellation and stale generation rejection;
- shutdown behavior.

Tests:

- deterministic manual executor with reordered completions;
- repository/PR/epoch/file/options switch before completion;
- cache hit/miss identity;
- cancellation, failure, and shutdown;
- forbidden Qt dependency scan.

## 6. Implement the Qt pane-geometry adapter and measured alignment axis

Scope:

- line-boundary document-coordinate measurement for the live pane implementation;
- measured max-extent segment construction;
- indexes and geometry cache;
- invalidation on file, font, zoom, DPI, wrap, fold, and viewport changes;
- presentation end caps.

Tests:

- empty-side projection is constant;
- unequal replacement ratios;
- continuity at segment boundaries;
- top/bottom reachability;
- high-DPI and differing viewport sizes.

## 7. Implement the shared comparison scroll controller

Scope:

- retained subpixel `q`;
- synchronization rail;
- one shared vertical scrollbar;
- wheel/trackpad translation from either pane and link map;
- Page/Home/End/scrollbar actions;
- guarded pane-origin application;
- independent horizontal scroll;
- paint coalescing without motion loss.

Tests:

- left and right input produce the same `q` path;
- insertion/deletion pinning;
- proportional unequal replacement;
- direction reversal without drift;
- no recursive feedback;
- no dead travel.

## 8. Implement the DiffLinkMap divider

Scope:

- preferred wide custom splitter handle or behaviorally equivalent center widget;
- cubic filled ribbons and wedges;
- theme, selected, hovered, and unavailable states;
- visible-run culling and retained document geometry;
- hit testing, selection, reveal, and resize threshold;
- move markers/temporary overlay.

Tests:

- rendered geometry at multiple DPI scales;
- edge seam and clipping checks;
- path hit tests by stable identity;
- no permanent crossing ordinary ribbons;
- visible-set instrumentation.

## 9. Integrate the composed workbench

Scope:

- connect accepted diff results to both top panes, shared scroll, link map, navigation, and lower detail;
- connect epoch/file switching and stale-result behavior;
- connect command/action state;
- preserve drawer and workbench layout ownership.

Evidence:

- real application or closest deterministic rendered integration harness;
- wheel over left and right;
- pure insertion/deletion pinning;
- unequal replacement;
- selected ribbon/detail/navigation agreement;
- file/epoch switch with late completion.

## 10. Add scale and high-frequency interaction evidence

Scope:

- representative large file and many-segment fixtures;
- structural counters for segment lookup, visible paths, axis rebuilds, allocations, and semantic recomputes;
- benchmark or trace smooth wheel/trackpad sequences;
- document platform/hardware context without overclaiming universal frame rates.

Acceptance:

- ordinary scroll performs no semantic diff/group recomputation;
- paint visits visible/near-visible changes rather than the entire file;
- layout invalidation rebuilds are explicit and bounded.

## 11. Complete documentation and architecture conformance

Scope:

- system architecture and state ownership;
- product diff/scroll UX;
- public/internal API documentation;
- agent scope docs;
- verification and fixture catalog;
- downstream RFC impact;
- documentation ledger and validators.

## 12. Simplify

Run the repository's code-simplifier or a focused conservative simplification pass. Preserve behavior and evidence. Commit changes separately or record a direct no-change result.

## 13. Verify and close RFC-BPR-0004

- re-read every acceptance criterion;
- run clean relevant builds and tests;
- run architecture/dependency scans;
- run the complete direct claim matrix after final docs and simplification;
- capture rendered interaction evidence;
- run `git diff --check` and repository validators;
- confirm all required cards are done;
- update RFC status, indexes, combined documents, and closeout ledger;
- commit closeout before beginning RFC-BPR-0005.
