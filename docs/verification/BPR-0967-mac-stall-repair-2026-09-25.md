# BPR-0967 Mac Qt stall repair — 2026-09-25

Code candidate `f3f2c6eed52c364e716f3157d1dba6f05af3ba35`, continuing
`75044f93a6ffdb4a0bd4367d1f05b864935250f5` under coordinator assignment
`/root/bpr0967_stall_repair`, mandate `r4-standalone-mac-20260925`.
Apple M4 Pro, 48 GiB, macOS 26.6.2, arm64 Debug, Qt 6.11.2,
Fusion/offscreen, 1600 × 1000 at scale 1. The network-denied public
`large-review-v1` composition retains 515 files, 1,000 threads, both exact
20,000-line source panes, installed Git and private SQLite state.

## Reproduction and owning repairs

The unchanged complete-phase responsiveness course reproduced the three
reported gaps: metadata/Git load **231.222 ms**, first large source
**110.085 ms**, and revisit **104.895 ms**. Its measurement code is the prior
`9d1cfa62` candidate; the starting documentation head is `75044f93`.
The earlier [measurement evidence](BPR-0967-mac-responsiveness-2026-09-25.md)
and its synchronous/terminal timing controls remain applicable.

Temporary local timing around the same public acceptance path found two
distinct Qt owners:

- The conversation detail initially measured the entire thread forest before
  hiding unrelated roots. Initial `refresh_projection` took 132.194 ms in one
  instrumented run. Selection now uses the selection model without implicit
  scrolling, scopes the detail rows first, then permits native layout. All roots
  remain in the incremental model; the previously selected exact post survives
  insertion and refresh without a model reset.
- Two-pane source publication also built a hidden unified document. One trace
  spent 30.403 ms in the left document, 31.128 ms in the right, and 30.056 ms in
  the hidden unified document. Unified source text and geometry are now populated
  only while one-pane presentation is active. Toggling preserves the exact
  semantic reading anchor before building or clearing that presentation.

All temporary timers were removed. The production changes are confined to the
two Qt owners; no core, application-host, authority or persistence policy changed.
The first uninstrumented repaired course observed **148.039 / 75.378 / 74.000 ms**.
No threshold was raised.

## Failing-before controls

The new controls were compiled against the original product and both failed:

- `qt_github_conversation_panel`: a wrapper around the real delegate observes
  which root identities native layout measures. With 1,000 threads plus posts,
  initial detail must measure only the selected root. Later selecting the last
  post and inserting a new root must retain exact selection and zero model resets.
- `qt_diff_workbench_scale_100`: ordinary two-pane publication must leave the
  unified source empty. Existing one-pane rows, anchors, map, selection and
  scroll-roundtrip assertions then exercise constructing the active presentation;
  leaving it must release its document and link geometry.

Both controls pass with the repair. The composed source check also requires an
empty inactive unified document, alongside the unchanged every-line/blob/anchor
assertions. The responsiveness course now gates complete first-source and
revisit intervals at the original 100 ms input limit, taking its separately named
fixture boundaries from 18 to 20.

## Remaining initial-load obligation

The repaired **148.039 ms** load interval still fails the 100 ms target. Temporary
timing localized parts of that acceptance turn to conversation projection
(56.659 ms, including 36.098 ms card relayout), window PR publication (70.675 ms,
including the conversation work), first Overview publication (48.700 ms), and
host handoff (29.031 ms, including 17.998 ms review-ledger composition and two
roughly 9 ms read-state observations). These measurements overlap and must not
be added as independent totals. They demonstrate aggregate synchronous work,
not a proven storage-only cause or an accepted scheduling repair.

A further bounded repair needs its own ownership/ordering analysis and
failing-before control on this public path. This milestone leaves that obligation
explicit rather than changing its target or calling a passing course a complete
responsiveness pass.

## Exact-candidate verification

The final uninstrumented responsiveness run passes all **20/20** named fixture
limits. Its complete phase intervals are:

| Phase | Baseline maximum ms | Final maximum ms | Input target ms |
| --- | ---: | ---: | ---: |
| Initial metadata/Git load | 231.222 | **142.388** | 100 |
| First large source | 110.085 | 79.443 | 100 |
| Large-source revisit | 104.895 | 83.142 | 100 |

`InputBudgetFailures` contains only `paged_review_and_git_load`. This remains
unaccepted. Load completion is 9,996.390 ms against 12,000; source completion is
1,934.347 ms against 2,500; revisit completion is 636.146 ms against 1,000.
Other named limits retain their original values. The bound is fixture-specific,
with no guarantee inferred for other hardware or native desktop rendering.

The cache-byte course admits 70,353,760 bytes across 20 selections, evicts under
the unchanged 64 MiB ceiling (peak retained bytes 66,839,408), and verifies exact
revisit plus eight source observations during supersession. The entry course
selects 65 files under the unchanged 64-entry ceiling and verifies released
diff/index owners and exact revisit. Loaded-close starts with the comparison
alive, drains its diff/index, destroys window/documents, and retains epoch storage.

| Resource course | Lifetime footprint growth MiB | Unchanged bound MiB |
| --- | ---: | ---: |
| Cache bytes | 140.875 | 184 |
| Cache entries | 83.656 | 160 |
| Loaded close | 127.407 | 160 |
| Prune/delete resources | 125.750 | 160 |

All four resource courses pass their existing lifetime-footprint and cleanup
live-heap bounds. Prune clears the projection and releases diff/index owners;
whole-cache delete and destruction release their exact owners as before.
The 32-epoch history course verifies every retained epoch with zero model resets.
Its worst refresh interval is 69.978 ms against 100, maximum refresh completion
8,658.235 ms against 12,000, and maximum source selection 1,733.863 ms against
2,000. Initial load remains listed as an input failure in these reports.

The focused integration command passes **16/16** in **855.67 seconds**:

```sh
ctest --preset dev -R '^(architecture_boundaries|qt_github_conversation_panel|qt_diff_workbench_scale_(100|125|200)|qt_composed_github_scale_100|application_large_review_(timing_control|responsiveness|benchmark|cache_bytes|cache_entries|history|loaded_close|resources)|application_(unavailable|approximate)_diff_public_path)$' --output-on-failure
```

This includes architecture boundaries, the real Qt composition, conversation
control, diff scales 100/125/200, the prior timing control, ordinary and
unavailable/approximate public paths, and all affected resource courses. Both
documentation gates pass (64 documentation tests), as do scoped board validation,
selected C++ format and diff checks. Final Conversations-drawer and dark
large-source-revisit captures were inspected. No source changed after the exact
candidate build; subsequent changes are evidence, architecture and tracker docs.

Final results and report identities are recorded in the
[durable handoff](../rfcs/plans/bpr0967-mac-responsiveness/handoff-stall-repair.json).

## Acceptance boundary

BPR-0967 remains unfinished. Initial load, true process/Qt-to-first-frame launch,
separate initial metadata, full active-work input/cancellation, generated or
pathological fallback, peak live heap and Windows/Linux measurements remain open.
Windows/Linux stations are explicitly deferred. Widget capture is not native
compositor presentation or OS input delivery. No human, independent, native-style,
packaged or release acceptance is supplied. The protected RFC0052 14-question
attended session is untouched, and this slice starts no review server.
