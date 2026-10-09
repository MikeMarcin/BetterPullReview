---
schema: "implement-rfc.card.v1"
id: "BPR-1349"
title: "A feedback count refresh rebuilds the whole snapshot on the window thread"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"performance","found_by":"bpr-main-linux, BPR-1094's measurements, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","related_rfcs":["RFC-BPR-0049"],"related_cards":["BPR-1094","BPR-1087","BPR-1089"]}
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
