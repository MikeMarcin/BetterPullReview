---
schema: "implement-rfc.card.v1"
id: "BPR-1078"
title: "Implement conversation partition and explicit resolution coverage"
status: "Done"
rfcs: ["RFC-BPR-0047", "RFC-BPR-0049"]
depends_on: ["BPR-1077"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-04","milestone":"semantics","acceptance":["R21","R22","R23","R24","R25","R26","R43"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"For 4 declared scopes x 2 modes x all 4,096 column subsets (32,768 projections) the real core projection equals the test's own brute force over the fixture facts, and dedicated plus catch-all equals the independently summed included population; both declared cases match every expected field; 24,000 generated and 24,576 10,000-unit projections conserve. macOS arm64 AppleClang Debug -Werror; negative controls failed (ledger).","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"AC2","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"Private notes, drafts, pending publications and deleted conversations are excluded and tallied by reason; a deleted root in a surviving thread stays as unclassified catch-all with its resolution; identical repeated pages collapse for posts (refresh_feedback_annotations Collapsed) and conversations; a disagreeing copy is refused (duplicate_post, contradictory_duplicate).","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"AC3","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"Unknown resolution (resolution_unknown), missing root (classification_partial), lost anchor (attribution_unknown on every file/folder) and partial/stale/unavailable collection each add a limit and the count is not Exact, even at zero; a complete empty scope is an exact zero; the pack cases are never exact.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R21","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"Disjoint exact partition for every subset and declared scope (AC1 evidence); catch-all parts (unselected, plain, custom, unclassified) sum to the catch-all. Core share only; Overview composition is BPR-1087/1089.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R22","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"One conversation counts once by root identity: repeated pages collapse, reply labels change no count (composed with the real annotation refresh). Core share only.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R23","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"default_feedback_resolution_mode is unresolved; all mode includes every state; the four-state breakdown is reported in both modes. Core share only; UX naming is RFC-BPR-0050.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R24","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"AC3 evidence; partial classification is counted apart from a truly plain root. Core share only.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R25","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"File rows plus the unattributed remainder equal the pull request; folders count whole path components once; outdated anchors count; unanchored and lost anchors stay pull-request wide; plain tour root catch-all, labelled tour root in its bucket. Core share only.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R26","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"Root edit moves one bucket, reply edit nothing, resolve/reopen move one unit between states, deletion removes it, a short page is qualified, counts carry the set's epoch and stale refreshes are refused upstream. Core share only; application deltas are BPR-1087.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}},{"criterion":"R43","outcome":"pass","revision":"84057d935001ecba4f40d077cf87289528c057be","evidence":{"kind":"command","detail":"The pack's counter cases (all labels, identity and coverage cases) drive real C++ core tests in core_feedback_partition and core_feedback_conversation. Core share only.","command":"ctest --test-dir build/core-only -R core_feedback_partition (tests/core/feedback_partition_tests.cpp against the pack's fixtures/counter-cases.json)"}}]
---

# BPR-1078: Implement conversation partition and explicit resolution coverage

Pack alias: **PACK-CC-04** (live `BPR-1078`, allocated at import).

## Outcome and work

Implement a pure normalized conversation accounting policy with unresolved/resolved/unknown/not-applicable lanes, exact deduplication, root labels and coverage. Adapt the supplied oracle cases to independent core tests. Track partial classification separately from a truly plain catchall body.

## Acceptance

- Dedicated counters plus catchall equal the included population for all 4,096 subsets.
- Draft/private/deleted units and duplicate pages obey the documented inclusion rules.
- Missing resolution/root/anchor and incomplete collection cases never produce a false complete clean result.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation evidence, 2026-10-05

Product commits `3f9658b81` (identical repeated posts collapse in the annotation
refresh), `2103dee10` (`feedback_partition.hpp`/`.cpp` and the row
`core_feedback_partition`) and `84057d935` (tour-stop roots in that test), all in
the Qt-free core. The [execution ledger](../../verification/RFC-BPR-0047-0051.md)
records the decisions (unit, repeats, inclusion, four states, catch-all parts,
attribution, limits) and the full evidence, including fourteen negative controls.

- **Every subset (R21).** The pack's 28 counter records, the repeated page among
  them, are classified as real posts and counted. For the 4 declared scopes, both
  modes and all 4,096 column subsets, the projection equals the test's own brute
  force over the fixture facts, and dedicated plus catch-all equals the included
  population: 32,768 projections. Both declared cases match every expected field.
  24,000 generated projections and 24,576 projections of a 10,000-conversation
  population conserve too.
- **Once per conversation (R22).** Repeated pages collapse for posts and for
  conversations, and a disagreeing copy is refused. Reply labels never count.
  Private notes, drafts, pending publications and deleted conversations are left
  out and tallied by reason. A deleted root in a surviving thread stays, as
  unclassified catch-all, with its own resolution.
- **Modes (R23).** Unresolved is the default mode. All states keep the
  four-state breakdown, and that breakdown is reported in both modes.
- **No false clean result (R24).** Unknown resolution, a missing root, a lost
  anchor and a partial, stale or unavailable collection each add a named limit,
  and the count is not `Exact`, even at zero. A complete empty scope is an exact
  zero. Partial classification is counted apart from a truly plain root.
- **Attribution (R25).** File rows plus the unattributed remainder equal the pull
  request. Folders count whole path components once. Outdated anchors count, and
  unanchored and lost anchors stay pull-request wide. A plain tour root is
  catch-all and a labelled one counts in its bucket.
- **Updates (R26).** Composed with the real annotation refresh: a root edit moves
  one bucket, a reply edit nothing, resolve and reopen move one unit between
  states, and deletion removes it. A short page is qualified. Counts carry the
  set's epoch, and stale refreshes are still refused upstream.
- **Fixtures and real tests (R43).** The pack's counter cases drive real C++ core
  tests, with all labels, identity and coverage cases.

Limitations: macOS arm64 AppleClang core-only Debug only. Linux and Windows runs
of `core_feedback_partition` and `core_feedback_conversation` are owed by station.
Nothing calls the partition yet: read-model adaptation and deltas are BPR-1087,
settings BPR-1088, and Overview rendering, sorting and drill-down BPR-1089. R21-R26
and R43 are met here for the core decisions only.
