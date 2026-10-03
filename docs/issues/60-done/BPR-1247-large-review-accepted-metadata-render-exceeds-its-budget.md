---
schema: "implement-rfc.card.v1"
id: "BPR-1247"
title: "Large review: accepted_metadata_render takes 382 ms against its 250 ms Mac fixture budget"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-0967","BPR-1211"]}
acceptance: [{"criterion":"Both rows pass on macOS, or the card records the measured cause and the owner's budget decision","outcome":"pass","revision":"d3f05b94b","evidence":{"kind":"observation","detail":"Measured cause recorded: bisect over 5a26d6b1b..main names f44c95dd7 (+110 ms Debug, +11 ms Release); the 250 ms limit measured 284 ms in Debug when BPR-0967 set it from optimized measurements; Release at main 68-73 ms with every budget enforced; lowered-limit Release control fails. Candidate d3f05b94b: quiet serial Debug, both rows pass and report accepted_metadata_render 386.8 ms as not enforced. Owner decision: BPR-1247-debug-limits-v001 Accept (session cbd48b91000b00f1e72e1ad62c7c384c, decision dc9c8014-1bab-4121-b684-abfab6565ac4, fingerprint 84068f25...46b0, hand-back 869), no note, so f44c95dd7's Release cost is not recovered. Scoping still present at 21f6e6461 after BPR-0967's cef11b982/f61340e17/9aa3df3bb (source read; rows not rerun in Debug for this closeout)."}}]
---

# BPR-1247: Large review: accepted_metadata_render takes 382 ms against its 250 ms Mac fixture budget

## Observation

At `main` 83edd361b on macOS arm64 (dev preset, Debug), `application_large_review_responsiveness`
and `application_large_review_launch` fail with
`large-review-benchmark-failed: Mac fixture responsiveness budget exceeded`
(`tests/end_to_end/large_review_benchmark.cpp:1127`). The one failing budget
in the report is `CompletionBudgets` `accepted_metadata_render`: measured
382.2 ms, limit 250 ms. No input budget failed. BPR-1211's implementer saw
this row red at its baseline d25f17b69.

Not measured: when it started, whether a quiet machine passes (this run shared
the machine with a full ctest), and whether the Release build passes.
[[BPR-0967]] owns the optimized hardware budgets; this is the Debug Mac
fixture budget the row enforces.

## Outcome

Either the render is brought back under its budget, with the regressing change
named, or a measured, owner-accepted budget change is recorded. The budget is
not raised to make the row pass.

## Direct acceptance

- Both rows pass on macOS, or the card records the measured cause and the
  owner's budget decision.

## Measured (red-test backlog milestone, macOS arm64, 2026-09-30)

Quiet machine, serial, dev preset (Debug): both rows fail at 386.8 and 386.7 ms,
the same as under load. So load was not the cause.

| Revision | Build | accepted_metadata_render |
|---|---|---|
| 5a26d6b1b (BPR-0967 adds the 250 ms limit) | Debug | 283.9 ms |
| cba9064cc (parent of f44c95dd7) | Debug | 294.9 ms |
| f44c95dd7 | Debug | 406.6 ms |
| main (a70698b52 + milestone work) | Debug | 384–389 ms |
| cba9064cc | Release | 64.0–64.9 ms (3 runs) |
| f44c95dd7 | Release | 74.6–75.8 ms (3 runs) |
| main | Release | 68.4–72.9 ms; every budget passes |

The Release builds use BPR-0967's recorded configuration (`cmake --preset dev
-B … -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0`).

- `git bisect run` over `5a26d6b1b..main` (paths `src`, the benchmark and its
  fixtures, boundary 330 ms, best of two runs per step) names **f44c95dd7**,
  "Reconstruct review tours from paged raw GitHub objects": about +110 ms in
  Debug and +11 ms in Release. Its new `refresh_tour_imports` scan is not the
  cost (the same 385 ms with it short-circuited); the growth comes with the
  paged read carrying each comment's full body, URL, dates, commits and
  review.
- The limit never passed in Debug: 284 ms the day it was set. The benchmark's
  own comment says BPR-0967's optimized measurements "justify the new startup
  and metadata limits. Debug measurements are diagnostic only", and BPR-0967's
  handoff records the owner's policy: "User requires optimized Release. All
  Debug observations remain diagnostic/correctness history only; no Debug
  budget justification." The dev row nevertheless enforced them.

## Candidate

`a8174f743`. The six limits BPR-0967 set from optimized measurements alone
(`qapplication_startup`, `main_to_home_render`, `provisional_metadata_render`,
`accepted_metadata_render`, `main_to_first_source_render`,
`generated_source_render`) are measured and reported in a Debug build with
`Enforced: false`. Every optimized build enforces them, and the historical
limits gate every build. No limit value changed.

| Check | Result |
|---|---|
| Both rows, Debug, serial | Pass; the report shows accepted_metadata_render 386.8 ms, `Passed: false`, `Enforced: false` |
| Release benchmark at main | Pass, 72.9 ms; every budget `Enforced: true` |
| Control: Release with the accepted limit set to 50 ms | Fails with "Mac fixture responsiveness budget exceeded" |

f44c95dd7's Release cost (11 ms of the 250 ms limit) is not recovered here.
The owner's Accept or Reject of the Debug scoping is attended item
`BPR-1247-debug-limits-v001` in session `cbd48b91000b00f1e72e1ad62c7c384c`
(no server started).

Published: `git pull --rebase` renamed the candidate. `a8174f743` above is `d3f05b94b` on main; the attended item names `d3f05b94b`.

## Owner verdict at hand-back 869, and closeout — 2026-10-03

The owner accepted `BPR-1247-debug-limits-v001` in session
`cbd48b91000b00f1e72e1ad62c7c384c`, handed back at event 869:

| Candidate | Fingerprint | Decision | Verdict |
|---|---|---|---|
| `BPR-1247-debug-limits-v001` | `84068f25…46b0` | `dc9c8014-1bab-4121-b684-abfab6565ac4`, saved 2026-10-03T05:23:37Z | Accept, no note |

So the six limits BPR-0967 set from optimized measurements alone stay measured
and reported in a Debug build, marked `Enforced: false`. Every optimized build
enforces them, and the historical Debug limits still gate every build. No limit
value changed. The question also offered recovering f44c95dd7's 11 ms Release
cost. The owner left no note, so it is not recovered.

The acceptance criterion's second branch is met. The card records the measured
cause (f44c95dd7, by bisect, and a limit that measured 284 ms in Debug the day
it was set) and the owner's budget decision.

The candidate is `d3f05b94b`. Since then [[BPR-0967]]'s `cef11b982`,
`f61340e17` and `9aa3df3bb` rewrote the benchmark's platform table and kept
this scoping. At `21f6e6461`, `tests/end_to_end/large_review_benchmark.cpp`
still records the six limits through `optimized_record` and `phase(…, optimized)`.
Its header comment reads "A Debug build enforces nothing it lacks a Debug
justification for: on macOS the historical Debug limits ([[BPR-1247]])".
`cef11b982` records all 16 large-review rows passing in Mac Release.
The two rows were not rerun in Debug for this closeout. The accepted behaviour
was read in the source, and no build was made, because other writers hold
`src/`. `BPR-1247-debug-limits-v001` is released by candidate name.
