---
schema: "implement-rfc.card.v1"
id: "BPR-0624"
title: "Measure large-review interactions through the composed application"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0623","BPR-0625"]
acceptance: [{"criterion":"Paged composed workload","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"29 exact account requests exhausted with production paging; 515 files and 1000 exact threads/comments, authority/head and installed-Git selection verified. Replay contract passed 0.52s; retained single-page public path passed 13.02s."}},{"criterion":"Exact rendered interaction","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"application_large_review_benchmark passed 27.30s. Both 20000-line panes and blob identities checked, full-window PNG inspected, source-line change navigation and 1000 idle wheel events verified. All 32 selections converge to exact final source; 31 occur with outstanding diff work. Clean shutdown."}},{"criterion":"Measured local baseline and limitations","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"Card retains M4 Pro/48 GiB Mac Debug Qt 6.10.3 offscreen/DPR1 context, phase/timer/gesture/work-overlap/cache observations. Material 387.48/201.09 ms maximum timer intervals tracked by BPR-0627; unavailable-result wording tracked by BPR-0626. No RFC-wide latency/memory/32-epoch/platform claims."}},{"criterion":"Focused integration and documentation","outcome":"pass","revision":"e9da7abcaae6ffc7d058905fb2e1d44cc1e5f27c","evidence":{"kind":"observation","detail":"Six final dev checks pass 6/6 in 166.87s: both architecture gates, stored fixture, network isolation manifest, retained single-page application and reviewed composition source contract. Documentation gate and git diff --check pass after BPR-0625 Done. All runs local with denied network access, no GitHub Actions or paid CI."}}]
---

# Measure large-review interactions through the composed application

Serve the checked-in BPR-0623 workload through the ordinary GitHub provider,
installed Git and application selection path. Establish a measured local
baseline before selecting performance budgets. RFC base remains
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; BPR-0617 is the closeout anchor.

## Bounded acceptance and ownership

- Add a test-only large-review fixture that exhausts six REST file pages and
  the corresponding GraphQL file/thread pages. Assert all 515 files and
  1,000 source threads/comments, exact authority/head, script exhaustion and
  installed-Git transport selection. Keep existing strict single-page fixture
  validation intact; paged replay must match exact compiled requests and let
  the production pager validate response/continuation contracts.
- Exercise real application file selection, display the exact 20,000-line
  comparison, navigate changes and scroll. Render a full application window
  and verify exact displayed source identities. Exercise rapid selection
  supersession and verify the final selection wins after background work.
- Record elapsed load/selection/gesture timings, event-loop observations and
  host/workbench counters with platform, architecture, Qt, scale, input and
  fixture-size context. Distinguish measured work overlap from idle gestures.
  Do not turn this machine's measurements into universal latency claims.
- Own the test-only fixture/composition selection under `src/app/`, a focused
  benchmark course and registration under `tests/end_to_end/`, any required
  large-review wire fixture data/helper, and this card/closeout. Keep provider
  production contracts unchanged unless this selected path exposes a defect.
- Run the focused composed course, a retained single-page application lane,
  fixture contract, isolation manifest, documentation gate and diff check.
  All verification is local and network-isolated; no GitHub Actions, paid CI,
  push, credential use, external AI process or production application launch.

This slice measures the latest head. The pack's 32 heads do not establish
32 application epochs. Memory/cache retention, platform-native budgets,
broader RFC resilience criteria and Windows/Linux runtime evidence remain
open. Record any measured defect and its remaining scope truthfully.

Initial execution exhausts all provider pages and loads installed Git. The
large comparison then exceeds the current full LCS matrix ceiling. BPR-0625
owns the bounded matching fix before this benchmark can pass. Initial harness
waits were corrected to distinguish idle jobs from shutdown-only quiescence.

## Final source and measured result

Source `e9da7abc` includes core fix `10520a43`. The test-only replay uses Glaze
and the stored pack/comment inventory. All 29 account requests, six REST file
pages, GraphQL file/thread pages and capability/bootstrap scripts are exhausted
through the ordinary provider and installed-Git path. Accepted authority,
515 files and all 1,000 thread/comment identities, bodies and anchors match.
The legacy strict single-page validator is retained.

The real application displays both exact 20,000-line source panes and blob
identities. Agent inspection of its full 1600×1000 render confirms the selected
large file, 20 semantic changes, source colors, file drawer and 1,000-thread
summary. Automated navigation selects source lines 1,001 and 2,001. A batch of
1,000 single-pixel native wheel inputs produces 1,000 frames and the expected
position, with zero semantic-index/geometry/pane-measurement rebuilds or new
classification submissions. This gesture batch is explicitly measured at idle.

All 32 rapid real file selections complete with the final exact source selected;
31 inputs occur with outstanding diff work. No fixture diff helper is invoked.
Shutdown completes without cleanup failure. These are correctness and measured
work-overlap results, not an RFC-wide responsiveness claim.

### Mac baseline

Apple M4 Pro, 48 GiB RAM, 14 logical processors; macOS 26.6.2 arm64; Debug;
Qt 6.10.3 Fusion/offscreen, DPR 1, 1600×1000. No concurrent build/test ran during
this benchmark. Pack: 281,166 bytes; compared blobs: 1,409,065 bytes. One latest
application epoch; 32 stored fixture heads are not 32 application epochs.
A precise 5 ms timer samples event-loop intervals and outstanding jobs, not
continuous worker CPU occupancy.

| Phase | Elapsed ms | Maximum timer interval ms | p95 interval ms |
|---|---:|---:|---:|
| Fixture connection | 48.03 | 12.00 | 5.79 |
| Paged review and Git load | 10,518.01 | 37.67 | 5.70 |
| Large-file selection | 2,237.70 | 387.48 | 5.70 |
| Rapid selection supersession | 12,650.85 | 201.09 | 11.53 |
| Shutdown | 4.12 | No timer sample | No timer sample |

Large selection has 186 timer samples with diff work and 34 with workbench work;
rapid selection has 146 and 1,162 respectively. Peak sampled counts are one
per host. Change actions take 3.63/3.20 ms; the idle wheel batch takes 318.03 ms;
after the last rapid input, final work drains in 1,113.70 ms. The diff cache ends
at two entries/3,517,512 bytes. This does not prove resident-memory/pruning
ceilings. BPR-0627 owns the material 387/201 ms UI intervals and active-work
interaction follow-up. BPR-0626 owns misleading unavailable-result wording.

### Verification

- `application_large_review_benchmark`: pass, 27.30s; exact source, rendering,
  gestures, work overlap, final selection and shutdown. Its temporary JSON/PNG
  were consumed into these observations, then removed.
- `large_review_replay_contract`: pass, 0.52s; exact request/response sequence,
  skipped REST page, changed GraphQL cursor, noncanonical body and exhaustion.
- Retained single-page `application_diff_workbench_public_path`: pass, 13.02s.
  `application_review_round_trip_composition_contract`: pass, 50.93s; reviewed
  test-only source inventory/digests, unchanged production mutation boundary.
- `large_review_fixture_contract`: pass, 2.76s; isolation manifest: pass, 4.36s;
  both dev architecture gates pass. These six final checks pass 6/6 in 166.87s.
- Final documentation gate and `git diff --check`: pass. An initial documentation
  gate correctly refused while BPR-0625 was still In Progress; it passes after
  that completed dependency is recorded Done.

All checks ran locally with denied test network access. No GitHub Actions,
workflow dispatch, push, paid CI, credentials or production application launch.
The remembered production sign-in remains untouched. Broader RFC-0013 criteria
and required native Windows/Linux evidence remain open on BPR-0617.
