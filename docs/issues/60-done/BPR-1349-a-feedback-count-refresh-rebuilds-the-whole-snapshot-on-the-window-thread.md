---
schema: "implement-rfc.card.v1"
id: "BPR-1349"
title: "A feedback count refresh rebuilds the whole snapshot on the window thread"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"performance","found_by":"bpr-main-linux, BPR-1094's measurements, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","related_rfcs":["RFC-BPR-0049"],"related_cards":["BPR-1094","BPR-1087","BPR-1089"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"15c86f2ed","evidence":{"kind":"command","path":"tests/application/feedback_counter_cost_tests.cpp","command":"ctest --test-dir build/app-release-tests -j1 -R '^(application_feedback_counter_costs|application_large_review_responsiveness|application_first_tab_comparison)$'","detail":"i9-10900KF 10C/20T, 62 GiB, WSL2 Ubuntu 26.04, GCC 15.2.0 Release -O3, idle, -j1, measured at 4320e0701 (landed 15c86f2ed). 10k: first observation 93.5 ms; unchanged 97.6 -> 49.1 (Stage 1) -> 32.1 ms; root edit 90.6 -> 51.9 -> 34.0 ms; reply edit 90.0 -> 50.2 -> 32.0 ms; resolve 93.1 -> 53.4 -> 32.9 ms. 1k: unchanged 8.6 -> 1.9 ms, root edit 9.4 -> 1.8 ms. Before, after-Stage-1 and after-Stage-2 tables on the card."}},{"criterion":"AC2","outcome":"pass","revision":"15c86f2ed","evidence":{"kind":"command","path":"tests/application/feedback_counters_tests.cpp","command":"ctest --test-dir build/dev -R '^(core_feedback_conversation|application_feedback_counters|application_feedback_counter_costs|application_overview_feedback|application_overview_feedback_public_path|application_overview_labels_public_path)$'","detail":"Parsed, reused, re-tallied, changed and units are as before; the overview, drill-down and label public paths pass. The 400 seeded rounds compare every carried refresh with the general path (231 carried, 169 general), and every kept tally with the core's oracle. New structural assertions: hashed 0 for unchanged and resolve, 1 for an edit; carried and no index rebuild for unchanged, edits and resolve. Controls: the digest rule, no fast path, a stale carried classification, always and never re-indexing each fail; BPR-1094's C1-C8 rerun and fail as recorded."}},{"criterion":"AC3","outcome":"pass","revision":"15c86f2ed","evidence":{"kind":"command","path":"tests/end_to_end/large_review_benchmark.cpp","command":"ctest --test-dir build/app-release-tests -j1 -R '^(application_large_review_responsiveness|application_first_tab_comparison)$'","detail":"Both 100 ms rows pass in the optimized tree after Stage 1 and after Stage 2 with no input budget failure; composed chip changes 18.6 / 30.9 / 29.9 ms, 24 publications for 24 changes. Also pass in build/dev at 15c86f2ed."}}]
---

# BPR-1349: A feedback count refresh rebuilds the whole snapshot on the window thread

## Finding

[[BPR-1094]] measured the label counters, at `269f7ccff`. The setup:
- i9-10900KF, WSL2, GCC 15 `-O3`.
- Row `application_feedback_counter_costs`.

An unchanged refresh, a root label edit and a resolve each cost about as much
as the first observation, even though they parse 0 or 1 body and re-tally at
most 5 scopes:

| Work | 1k conversations | 10k conversations |
|---|---|---|
| First observation | 8.5 ms | 90.9 ms, 415k allocations / 59 MB |
| Unchanged snapshot | 8.2 ms | 91.1 ms, 52.5 MB |
| Root edit | 9.4 ms | 92.6 ms, 56 MB |

The per-snapshot pass in `publish_overview` runs on the window's thread. It
copies every body into the adapter's posts, rebuilds the annotation set post
by post, rebuilds the population, compares every unit and re-tallies the pull
request scope over every unit. 10k is the read model's own limit.

## Outcome

A refresh whose snapshot changed little costs work in proportion to what
changed, or the remaining linear pass moves off the window's thread, so that
a 10k-conversation pull request refreshes within the input budget.

## Acceptance

- Re-measure with `application_feedback_counter_costs` on the same workloads.
  The unchanged-snapshot and root-edit rows drop clearly below the first
  observation at 10k, with the hardware and build recorded.
- Counts, drill-down and annotations stay identical; BPR-1094's structural
  assertions and controls still hold.
- The 100 ms rows (`application_large_review_responsiveness`,
  `application_first_tab_comparison`) pass.

## Boundaries

No live GitHub. Only worth doing if pull requests near 10k conversations
matter to the owner; at 1k the refresh takes 8 to 9 ms.

## Measured on Linux, and stopped at the design gate (bpr-main-linux, 2026-10-08)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high). Nothing
here was landed in product source. The coordinator's design gate for this
card says a change to the core's annotation model, a snapshot diff or new
threading is proposed here and not landed. The cost turned out to be the
core's reuse rule, so this card stops at the proposal below.

**Machine and build.** Intel Core i9-10900KF (10 cores, 20 threads),
62 GiB RAM, WSL2 (kernel 6.18.33.2) on Ubuntu 26.04, GCC 15.2.0, Qt 6.11.2,
offscreen.

- The optimized tree was `cmake --preset app-release -B
  build/app-release-tests -DBUILD_TESTING=ON -DCMAKE_TEST_BUILD_DEPENDS=ON`
  (Release, `-O3 -Werror`), at `4d850e218`.
- Rows ran with `ctest -j1` once the load average was under 1.0.
- The tree was deleted afterwards.

### Before

`application_feedback_counter_costs`, BPR-1094's workloads. Each cell gives
the median time, then C++ `new` calls and bytes of one run:

- first observation: median of 5 runs;
- observations: median of 11;
- chip projections: median of 21.

| Work | 1k (2,015 posts) | 10k (20,015 posts, 8.0 MB of text) |
|---|---|---|
| First observation | 8.6 ms; 45,909 / 5.4 MB; parsed 2,015 | 105.4 ms; 415,215 / 59.3 MB; parsed 20,015 |
| Unchanged snapshot | 8.6 ms; 37,626 / 4.5 MB; parsed 0 | 97.6 ms; 360,280 / 52.5 MB; parsed 0 |
| Root edit | 9.4 ms; 39,760 / 4.7 MB; parsed 1; 5 scopes | 90.6 ms; 381,149 / 56.2 MB; parsed 1; 5 scopes |
| Reply edit | 8.2 ms; parsed 1; 1 scope | 90.0 ms; parsed 1; 1 scope |
| Resolve | 8.3 ms; parsed 0; 5 scopes | 93.1 ms; parsed 0; 5 scopes |
| Chips 0 / 3 / 12, Open | 0.45 / 0.51 / 0.57 ms; 0 parsed, 0 units | 2.8 / 2.9 / 3.5 ms; 0 parsed, 0 units |
| Twelve chips, sorted | 1.4 ms | 7.4 ms |

A second idle run gave 90.7 ms (unchanged) and 93.9 ms (root edit) at 10k,
so the spread between runs is about 5 to 10 %. BPR-1094 measured 90.9 ms,
91.1 ms and 92.6 ms. Composed `large-review-v1` (Release, 1,000 threads)
chip changes took 25.0 ms (3 chips), 16.9 ms (12) and 24.7 ms (0). Both
100 ms rows pass: `application_large_review_responsiveness` and
`application_first_tab_comparison`, with no input budget failure.

### Where the linear cost goes

These stage times come from temporary timers in
`feedback_counter_model::observe` and `core::refresh_feedback_annotations`
(scratch only, reverted). They are 10k medians in ms; the timers add a
little.

| Stage | First | Unchanged | Root edit |
|---|---|---|---|
| Adapter, `observe_feedback_sources` (ids, `map_tour_stop` per body) | 9.8 | 7.1 | 6.7 |
| Copy of the kept annotation set (`current = Annotations`) | 2.1 | 4.6 | 3.8 |
| **Core `refresh_feedback_annotations`** | **81.5** | **78.7** | **69.0** |
| Population build | 2.7 | 2.2 | 2.2 |
| Unit comparison | 0.0 | 1.5 | 1.4 |
| `index_units` | 1.8 | 3.8 | 3.6 |
| Scope sets | 0.9 | 0.7 | 0.7 |
| Tallies (every scope when first, then the pull request plus touched scopes) | 11.0 | 0.6 | 1.8 |

The refresh is the cost, and within it the cost is **SHA-256 of every
observed body**. To decide that a kept annotation can be reused, the core's
`reusable()` compares `BodyDigest` with `digest_of(observed body)`. Every
observation therefore hashes all 8.0 MB, changed or not. Measured on its own
with a scratch program linked against the optimized core, `core::sha256`
hashed the same body mix (7.98 MB) in 69 to 78 ms, about 110 MB/s, because
it pushes one byte at a time. `published_feedback_segment` over the same
bodies took 1.9 to 2.4 ms.

The candidates the card named are small next to it: copying bodies into
posts (bodies are `string_view`s and are not copied), rebuilding
annotations (each is reused) and comparing units (1.5 ms). The pull
request's re-tally takes 0.6 ms.

Parsing is also cheap. A first observation parses all 20,015 bodies, and
its refresh costs only about 3 to 12 ms more than an unchanged one, because
both hash every body.

With the hash gone, the rest of the refresh at 10k is a whole-set rebuild
of about 26 ms:

- sorting the next posts by conversation: 9 ms;
- building posts, three strings each: 7 ms;
- `classify_all`: 5 ms;
- two identity index sorts: 4 ms.

That rest was measured in the prototype below.

### Why this is past the gate

To make an unchanged or edited snapshot cost less than the first
observation, the core must stop hashing bodies it already knows. It needs
another proof that a body is unchanged. Every such proof changes a type or
rule outside the counter and adapter layer:

- the core's annotation model (what a kept post holds and how reuse is
  decided);
- a caller-supplied digest, which moves the identity proof out of the core;
- a body revision or a diff from the read model.

The local fixes alone (no copy of the kept set, cheaper ids, no index
rebuild) save about 10 to 15 ms of about 95. They leave the rows near the
first observation, so they would not meet the outcome. They are listed in
the design and were not landed separately.

## Proposed design

**Goal.** At 10k, an unchanged snapshot, a root edit, a reply edit and a
resolve cost well under the first observation. Counts, drill-down,
annotations and BPR-1094's structural assertions stay identical. No
threading, no read-model or snapshot change, and no `src/app` change, so no
pin reseal.

### Stage 1: reuse by kept bytes, in the core (recommended, prototyped)

**Core** (`bpr/core/feedback_conversation.hpp/.cpp`):

- `feedback_post` keeps its body:
  `std::shared_ptr<const std::string> Body`. It is immutable and shared
  between successive annotation sets, so a reused post copies a pointer and
  not its text.
- `reusable()` compares the kept bytes with the observed body exactly (size,
  then bytes). The other conditions stay: not deleted, same segment, same
  parser revision. Exact bytes is a stronger test than equal digests, and
  it reads memory at memcmp speed instead of hashing.
- A recognized body still gets `BodyDigest` from `recognize_feedback_body`,
  since the header and draft edits rely on it. Its body is copied into the
  set once.
- `feedback_annotation_refresh` gains a count of bodies hashed, which the
  counter model adds to `feedback_counter_work` (for example
  `BodiesHashed`).

**Application** (`feedback_counter_model::observe`): refresh against the
kept set by reference (`const auto&`, with an empty set when the scope is
replaced) instead of copying it.

**Other consumers** of `feedback_post` follow the type: the review
preflight (`core/review_preflight`, `application/review_preflight_observation`)
and the existing core tests. Nothing persists the annotation set (no SQLite
or adapter type holds it), so nothing migrates.

**Measured with a scratch prototype of Stage 1** (exactly the two changes
above, reverted afterwards). The cost row's own structural expectations all
held.

| Work | 1k before → Stage 1 | 10k before → Stage 1 |
|---|---|---|
| First observation | 8.6 → 8.5 ms | 105.4 → 99.5 ms (447,245 / 68.6 MB) |
| Unchanged snapshot | 8.6 → 3.2 ms | 97.6 → 49.9 ms (288,721 / 42.8 MB) |
| Root edit | 9.4 → 4.3 ms | 90.6 → 49.4 ms |
| Reply edit | 8.2 → 3.7 ms | 90.0 → 51.6 ms |
| Resolve | 8.3 → 3.8 ms | 93.1 → 52.7 ms |
| Chips, drill-down | unchanged | unchanged |

**Memory.** The kept set holds one copy of the body text: 8 MB in the 10k
workload and at most 16 MiB at the read model's ceiling. A first
observation allocates about 9 MB more.

### Stage 2: no whole-set rebuild when little changed (estimate, not prototyped)

**Core refresh fast path.** When the observed post identities arrive in the
kept order (the usual case), the refresh can:

- skip both identity index sorts and the sort of the next posts;
- carry over the classification of every conversation whose posts were all
  reused.

The same output is produced in a different way, and the slow path stays for
any other order. Expected saving: 15 to 20 ms of the remaining 26 ms.

**Counter model.** Rebuild `index_units` and the present-scope set only
when a unit's anchor changed (4 to 5 ms). Build the adapter's prefixed ids
once per post identity, or keep them, instead of anew per observation
(part of the 7 ms).

**Expected at 10k: about 15 to 25 ms** for an unchanged snapshot, a root
edit, a reply edit and a resolve. A first observation stays about 95 ms,
because it parses and hashes every body.

### Optional, separate: a faster SHA-256 in the core

`core::sha256` handles one byte per call. Hashing whole 64-byte blocks in
portable C++ is typically 2 to 3 times faster (an estimate). That would
shorten the first observation and every recognized body, but it changes a
primitive with many security-relevant callers. It needs the standard test
vectors and its own card. It is not needed for this card's outcome.

### Not proposed

- **Moving the observation off the window's thread.** That needs new
  threading and shared snapshot lifetime. The Overview's rows and counts
  would also stop coming from one snapshot (BPR-1089's
  `publish_overview` order). Keep it as a fallback if Stages 1 and 2 fall
  short.
- **A snapshot diff or incremental snapshots from the read host.** Larger,
  and the measurements show it is not needed.

### Risks

- **Changing a core contract type.** `feedback_post` is shared by the
  counters and the review preflight, so every consumer and its tests move
  together. Reuse must stay keyed by scope, segment and parser revision as
  now; only the body proof changes.
- **Shared body lifetime.** The bodies are immutable
  `shared_ptr<const std::string>`, standard C++ and core-pure. Two sets
  never write through them.
- **Stage 2's fast path is a second route** to the same output. It must be
  checked against the slow path on random rounds.
- **Timing** is one machine's (WSL2, i9-10900KF). The card would record it,
  never turn it into a threshold. Debug is about ten times slower.

### Tests, each with a failing-before control

- **Core** (`core_feedback_conversation` and neighbours):
  - same bytes are reused;
  - one changed byte at the same length is parsed again;
  - a changed segment, parser revision or deletion is parsed again;
  - a reused post shares its kept body (no copy);
  - the hashed count is 0 for an unchanged refresh and 1 for one edit.

  Control: `reusable()` back on the digest fails "an unchanged refresh
  hashes no body".
- **Application** (`application_feedback_counter_costs`): new structural
  assertions at 1k and 10k:
  - an unchanged snapshot hashes 0 bodies;
  - a root or reply edit hashes 1;
  - a resolve hashes 0.

  These sit alongside the existing parsed, reused, re-tallied and changed
  checks. Control: the digest rule fails them. Times are reported against
  the before table above.
- **Stage 2:** the 400 seeded rounds of `application_feedback_counters`
  (seed 1087) also compare the fast path's annotation set and update with
  the slow path's. Control: a fast path that keeps a stale classification
  fails.
- **Unchanged evidence, rerun:**
  - BPR-1094's controls C1 to C8;
  - `application_overview_feedback`, `_public_path` and
    `application_overview_labels_public_path` (counts, drill-down,
    annotations);
  - the preflight rows;
  - both 100 ms rows in the optimized tree;
  - `type_read_binding_contract`, `architecture_boundaries` and the
    round-trip contract (no `src/app` change, so no reseal).

### Decision needed

Approve Stage 1, Stage 1 and 2, or another direction. Until then the
outcome is not met and the card stays In Progress with no product change.

## Coordinator decision (2026-10-08)

The coordinator approved Stage 1 exactly as proposed, then Stage 2 on this
card. The faster SHA-256 is [[BPR-1358]]; `core::sha256` is not changed here.

## Stage 1 landed: reuse by kept bytes (bpr-main-linux, 2026-10-08)

Product commit `a4c04d31a`. It was built and measured as `cc0570685`; the
landing rebase over `5c0ff44a7` brought only an attended-review note.

**What changed.**

- **Core** (`feedback_conversation`):
  - `feedback_post::Body` is a `std::shared_ptr<const std::string>`: the
    exact bytes the post's annotation was read from. It is absent for a
    deleted post. A body is copied once, when it is recognized, and reused
    posts share it from set to set. It is never written, and there is no
    hidden global.
  - `reusable()` compares those bytes with the observed body. Segment,
    parser revision and not-deleted still apply. The body is no longer
    hashed to prove it unchanged.
  - `feedback_annotation_refresh::Hashed` counts bodies hashed: one for each
    recognized body, whose annotation carries its digest.
- **Application.**
  - `feedback_counter_model::observe` reads its kept set in place instead of
    copying it.
  - `feedback_counter_work::BodiesHashed` adds up `Hashed`.
- **Memory.** The kept set now holds one copy of the posts' text. That is
  about 8 MB in the 10k workload and **at most 16 MiB** at the read model's
  ceiling (`MaximumTotalTextBytes`). A first observation allocates about
  9 MB more (68.6 against 59.3 MB at 10k). Every later observation
  allocates less, because the kept set is no longer copied.

**After Stage 1.** Optimized tree at `cc0570685` (landed as `a4c04d31a`), idle, `ctest -j1`. Same
machine, build and workloads as the before table above.

| Work | 1k before → after | 10k before → after |
|---|---|---|
| First observation | 8.6 → 8.6 ms; hashed 2,015 | 105.4 → 93.8 ms; 447,245 / 68.6 MB; hashed 20,015 |
| Unchanged snapshot | 8.6 → 3.3 ms; hashed 0 | 97.6 → 49.1 ms; 288,721 / 42.8 MB; hashed 0 |
| Root edit | 9.4 → 3.4 ms; hashed 1 | 90.6 → 51.9 ms; hashed 1 |
| Reply edit | 8.2 → 3.3 ms; hashed 1 | 90.0 → 50.2 ms; hashed 1 |
| Resolve | 8.3 → 3.3 ms; hashed 0 | 93.1 → 53.4 ms; hashed 0 |
| Chips 0 / 3 / 12, Open | 0.47 / 0.51 / 0.56 ms | 2.9 / 3.2 / 3.5 ms |

Parsed, reused, scopes and units are exactly as before. The composed
`large-review-v1` chip changes took 27.6, 27.6 and 26.3 ms, with 24
publications for 24 changes. Both 100 ms rows pass with no input budget
failure.

**Tests.**

- `core_feedback_conversation`, `reuse_by_kept_bytes`:
  - each parsed body is hashed once; an unchanged refresh hashes none and
    shares the kept bytes;
  - one changed byte at the same length is parsed and hashed alone;
  - the same bytes are reused even under a kept digest that no longer
    matches them, and other bytes are parsed again even under a kept digest
    that matches the observed body;
  - a deleted post keeps no bytes, and is parsed again when it comes back.
- `application_feedback_counter_costs` at 1k and 10k:
  - the first observation hashes every body once;
  - an unchanged snapshot and a resolve hash 0;
  - a root edit and a reply edit hash 1.

**Failing-before controls.** The control restores the digest rule in
`reusable()` and counts each digest it takes as a hash. It was built, run
and reverted. Each test stops at its first failure, so later assertions
were shown by removing the earlier ones in turn (scratch copies):

| Assertion | Under the digest rule |
|---|---|
| core "an unchanged observation hashes no body" | fails |
| core "one changed byte parses that body again and hashes it alone" | fails |
| core "the same bytes are reused whatever the kept digest says" | fails |
| core "other kept bytes are parsed again, though the kept digest matches" | fails |
| cost "1k: an unchanged snapshot hashes no body" | fails, hashed 2,015 |
| cost "1k: a root edit hashes only the edited body" | fails, hashed 2,016 |
| cost "1k: a reply edit hashes only the edited body" | fails, hashed 2,016 |
| cost "1k: a resolve hashes no body" | fails |

**BPR-1094's controls rerun** against `cc0570685` (landed as `a4c04d31a`), each built, run and
reverted in `build/dev`. All fail at the same checks as recorded:

- C1 (`tally_of` ignores the kept tally): "1k: a chip or mode change parses
  no body and tallies no conversation".
- C2 (no kept annotations) and C3 (every scope re-tallied): "1k: an
  unchanged snapshot parses nothing and tallies only the pull request
  again".
- C4 (folders not re-tallied): "1k: a root edit tallies again exactly the
  pull request, the file and its three folders, and they are what changed".
- C5/C6 (a chip request re-observes):
  `application_overview_feedback_public_path` fails "ticking two chips in
  Settings parses no body and tallies no conversation", and
  `application_large_review_responsiveness` fails "chip changes read nothing
  from the provider, parse no body and tally no conversation".
- C7/C8 (a fresh model observed per chip request): the same two checks.

**Rows.**

- `build/dev`: 43 of 43:
  - core: every `core_feedback_*` row and `core_review_preflight`;
  - application: the counter, cost and overview rows, the review
    submission rows and `application_label_authoring_contract`;
  - Qt: the overview and settings rows and `qt_feedback_label_*`;
  - composed: both 100 ms rows, the overview public paths and nested
    folders;
  - gates: `architecture_boundaries`, `test_network_isolation_manifest` and
    `application_review_round_trip_composition_contract`.
- `core-only`: 14 of 14, including `type_read_binding_contract`.
- `/usr/bin/clang-format` 22.1.8 dry run clean on the six changed files;
  `git diff --check` clean.
- **Pins.** No `src/app` source changed. The round-trip contract passes
  without a reseal.

## Stage 2 landed: carry the kept order over (bpr-main-linux, 2026-10-08)

Product commit `15c86f2ed`. It was built and measured as `4320e0701`; the
landing rebase brought BPR-1346, BPR-1347 and BPR-1355, which changed none
of this card's files. No read-model, API-signature or threading change.
Two observability fields were added, matching Stage 1's `Hashed`.

**What changed.**

- **Core** (`refresh_feedback_annotations`):
  - **When the fast path applies.** The observation names exactly the kept
    posts, each in its kept conversation with its kept reply link, no page
    repeated one, and the kept set is in its documented order.
  - **What it does then.**
    - It pairs observations with kept posts in one walk of the two identity
      indexes, with no binary search per post.
    - It builds the next posts in the kept order, so nothing is sorted and
      no removal scan runs.
    - It classifies again only the conversations that have a changed post;
      every other keeps its kept classification.
    - If the kept conversations ever fail to line up with the posts' runs,
      it classifies everything.
  - `feedback_annotation_refresh::Carried` says the fast path ran. The
    result is exactly the general path's, which the tests below check.
- **Counter model.** `index_units` and the present-scope set are rebuilt
  only when a unit arrived, left or changed anchor, or when a change reaches
  every scope. Otherwise the units keep their places, in identity order.
  `feedback_counter_work` gains `RefreshesCarried` and `UnitIndexes`.

**After Stage 2.** Optimized tree at `4320e0701`, idle, `ctest -j1`. Same
machine, build and workloads as the tables above.

| Work | 1k: before → Stage 1 → Stage 2 | 10k: before → Stage 1 → Stage 2 |
|---|---|---|
| First observation | 8.6 → 8.6 → 8.7 ms | 105.4 → 93.8 → 93.5 ms; 447,245 / 68.6 MB |
| Unchanged snapshot | 8.6 → 3.3 → **1.9 ms** | 97.6 → 49.1 → **32.1 ms**; 241,240 / 39.7 MB |
| Root edit | 9.4 → 3.4 → **1.8 ms** | 90.6 → 51.9 → **34.0 ms**; 262,114 / 43.4 MB |
| Reply edit | 8.2 → 3.3 → **1.8 ms** | 90.0 → 50.2 → **32.0 ms** |
| Resolve | 8.3 → 3.3 → **1.9 ms** | 93.1 → 53.4 → **32.9 ms** |
| Chips 0 / 3 / 12, Open | 0.47 / 0.51 / 0.56 ms | 3.2 / 3.2 / 3.4 ms |
| Twelve chips, sorted | 1.3 ms | 7.7 ms |

- **Work per phase.** Parsed, reused, re-tallied and units are as before.
  Every unchanged, edited and resolved observation was carried and rebuilt
  no index. The first observation was not carried and indexed once.
- **Composed.** `large-review-v1` chip changes took 18.6, 30.9 and 29.9 ms,
  with 24 publications for 24 changes.
- **100 ms rows.** Both pass with no input budget failure.

**Against the estimate.** I estimated 15 to 25 ms at 10k and measured 32 to
34 ms, which is a third of the first observation. Per the pre-Stage-2
profile above, the remaining linear work is:

- the adapter rebuilding its posts' prefixed ids and tour-stop segments
  (about 7 ms);
- the refresh copying each post's three strings into the next set (about
  7 ms);
- the two identity index sorts (about 4 ms);
- the population build (about 2 ms);
- the unit comparison (about 1.5 ms);
- the pull request's re-tally (under 1 ms).

Going further means keeping ids between observations or making the kept
posts shared rather than value copies. That is beyond Stage 2's scope; it
is not done here and not proposed as necessary.

**Tests.**

- **The 400 seeded rounds** (`application_feedback_counters`, seed 1087).
  Each round also refreshes a parallel core annotation set twice from the
  same base:
  - as handed over, which the fast path may take;
  - with one identical repeated page, which collapses, changes nothing
    else and always takes the general path.

  The two must agree on the next set's posts, bytes, annotations and
  conversations, and on what was recognized, reused, hashed, removed and
  changed. 231 rounds were carried and 169 took the general path
  (arrivals and departures). The test requires more than 200 and more than
  50 respectively.
- **`core_feedback_conversation`**:
  - the first refresh is not carried;
  - an unchanged refresh is carried and keeps the kept classifications;
  - a carried edit equals the general path's result with a repeated page.
- **`application_feedback_counter_costs`** at 1k and 10k:
  - the first observation builds its set and its index;
  - an unchanged snapshot, a root edit, a reply edit and a resolve are each
    carried and keep the index.

**Failing-before controls.** Each was built, run and reverted in
`build/dev`.

| # | Mutation | Fails at |
|---|---|---|
| S2a | No fast path | core "the kept posts observed again carry their order and classification over"; `application_feedback_counters` "edits, resolutions and moves are carried; arrivals and departures take the general path"; cost "1k: an unchanged snapshot carries the kept order over and keeps the index" |
| S2b | The fast path keeps every kept classification, changed or not | core "root edit touches its own conversation only"; `application_feedback_counters` (first at an earlier check) "the edit changes its file, that file's folders and the pull request, and nothing else is tallied". The seeded comparison alone, run in a scratch copy with the model's own checks removed: "round 2: the carried and the general refresh agree" |
| S2c | The model always re-indexes | cost "1k: an unchanged snapshot carries the kept order over and keeps the index" |
| S2d | The model indexes only a new pull request | `application_feedback_counters` "a file and folder with no conversation left are removed; the pull request and the parent change" |

BPR-1094's C1 to C4 were rerun against Stage 2 and fail at the same checks
as recorded. C5 to C8 are in the window's dispatch, which Stage 2 does not
touch; they were rerun at Stage 1.

**Rows.**

- At `4320e0701`, `build/dev`: 44 of 44, the Stage 1 set plus
  `application_setup_destination_public_path`.
- `core-only`: 14 of 14, including `type_read_binding_contract`.
- At `15c86f2ed` after the rebase, an integration pass of 28 rows: 27 pass.
  `test_network_isolation_manifest` fails on `main` because of another
  card: BPR-1355 (`614c34fa3`) registered
  `application_thread_words_public_path` as `WINDOWS_UNCONTAINED` without its
  `WINDOWS_UNCONTAINED_TESTS` entry. This card does not touch that file.
- `/usr/bin/clang-format` 22.1.8 dry run clean on the seven changed files;
  `git diff --check` clean.
- **Pins.** No `src/app` source changed in either stage, and the round-trip
  contract passes without a reseal.

The optimized tree was deleted.

### For the coordinator

Ready for the board finish: AC1 to AC3 pass at `15c86f2ed`. Stage 1 is
`a4c04d31a` and Stage 2 is `15c86f2ed`; the memory cost is recorded under
Stage 1. Platforms owed: macOS and Windows runs of the changed core,
application and cost rows.

## Coordinator verification (bpr-main-linux, 2026-10-09)

At `20fb3889e`, in `build/dev`, 9 of 9 pass: `core_feedback_conversation`, the counter
and cost rows, the overview rows, `qt_pull_request_overview_page`,
`application_large_review_benchmark`, the round-trip contract and
`architecture_boundaries`. Done on Linux evidence; macOS and Windows runs are
owed.
