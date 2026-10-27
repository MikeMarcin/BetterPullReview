---
schema: "implement-rfc.card.v1"
id: "BPR-1765"
title: "Investigate the Windows Debug 200% diff workbench timeout"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"Owner's v0.3.3-only timeout waiver and follow-up request, 2026-10-24","reservation":"docs/issues/_reservations/BPR-1765--BPR-1765--65cc7e762a78.json","related_cards":["BPR-1694"],"release_candidate":"b2801f2aaa858fd5e7d18d76253834d34024abc7","diagnostic_candidate":"ddc43c10ed87e958c9a23a5b9887371d2928ea4c","external_evidence":"/Users/jaeger/Documents/Codex/2026-10-21/release-v032","triage_20261026":{"date":"2026-10-26","disposition":"stale","reason":"Superseded by the owner's standing Debug timing policy (2026-10-26); the row is slow, not hung."}}
---

# Investigate the Windows Debug 200% diff workbench timeout

## Owner decision and scope

On 2026-10-24 the owner explicitly waived the known
`qt_diff_workbench_scale_200` Windows Debug timeout for v0.3.3 and requested
this investigation. The release coordinator relayed that amendment and stopped
the repair work. The release product candidate remains
`b2801f2aaa858fd5e7d18d76253834d34024abc7`; the passive diagnostic commit is
retained separately. This card and its allocation are a separate documentation
commit, outside that release candidate.

The waiver applies only to this named test, Windows Debug and v0.3.3. It does
not turn the failed or interrupted suite into a complete pass, extend to another
failure or release, or accept a larger timeout. Release receipts must retain
the actual timeout and the owner's exception. This card remains unfinished.

## Observed evidence

The registered target is
`test_better_pull_review_qt_diff_workbench_tests`, with the row declared in
`tests/adapters_qt/CMakeLists.txt`. Its original timeout is 120 seconds and its
200% environment selects `QT_SCALE_FACTOR=2.0` and
`BPR_EXPECTED_QT_SCALE=2.0`. The course also writes its configured private render
outputs. Preserve its exact registered argv, environment, scrubs, working
directory, resource locks and scheduling when comparing observations.

| Run | Exact source | Observation |
| --- | --- | --- |
| R99, Windows comprehensive Debug | `c68c7d629020d709c6cee2470ae7dc372ae1d4fe` | The row times out at 120.04 s. |
| R168, Windows comprehensive Debug | `b2801f2aaa858fd5e7d18d76253834d34024abc7` | The row times out at 120.05 s; the later partial suite was interrupted and its remaining rows were not qualified. |
| R187, focused Windows Debug diagnostic | `ddc43c10ed87e958c9a23a5b9887371d2928ea4c` | The unchanged row completes in 115.62 s, close to the same 120 s limit. All phase boundaries complete; this observation does not close either timeout. |

R187 changes only flushed phase begin/end output and elapsed-time reads around
the existing course. Independent review recovered the original test/helper
bodies and main call order by reversing those wrappers. Instrumentation and
the focused run can affect timing, so its pass is diagnostic evidence rather
than acceptance by retry.

The four largest recorded R187 phases are:

| Phase | Elapsed |
| --- | --- |
| `test_keyboard_minimal_reveal` | 29,916 ms |
| `test_the_gutter_sizes_to_its_longest_number` | 20,459 ms |
| `test_divider_drag_keeps_fractional_viewport` | 19,005 ms |
| `test_a_text_selection_reads_on_every_line_kind` | 6,388 ms |

These observations locate substantial cumulative cost; they do not establish
a stuck loop, a scheduler defect, or a particular production rendering cause.
The keyboard case retains 1,080 arrow actions across its existing variants and
checks each resulting selection and visible row. The divider case retains
120 pointer moves across its three fixtures and observes rows during pending
geometry turns. The gutter course includes its 100,000-line document and
checks complete pane contents, exact widths, final-line painting, font changes
and one-pane columns. Do not remove those checks to fit the budget.

Raw evidence remains outside the repository under the metadata evidence root:

- `windows-v033-r99/windows-dev-comprehensive.stdout.log` and its source-bound
  pipeline and suite receipts;
- `windows-v033-r168`, including the retained partial Debug suite, and
  `debug-diff-r168.stdout.log`;
- `windows-v033-r187/LastTest.log`, `debug200-diagnostic.receipt.json` and
  `pipeline.receipt.json`;
- `r168-debug-workbench-diagnostic-handoff.json` and
  `full-gate-repairs.checkpoint.json`.

## Investigation and acceptance

- AC1: Preserve the failed runs and bind a bounded investigation to exact native
  source, binary, Debug configuration, Qt version, scale, fixtures and registered
  execution properties. Attribute the repeated work or cumulative cost with
  direct measurements; do not infer a cause from the focused pass or unrelated
  macOS/Release repairs.
- AC2: Implement a scoped efficiency repair and a deterministic counterexample
  or work-count control that distinguishes the previous mechanism from the
  repaired one. Preserve all original semantic, geometry, rendering, selection,
  action-count and bounded-work assertions. Any reuse must invalidate correctly
  when its actual inputs change.
- AC3: Run the original Windows Debug 200% row with its unchanged 120-second
  timeout on the exact corrected candidate, and the relevant scale/configuration
  and integration checks justified by the changed surface. Record failures and
  complete results. Do not gain acceptance by extending the timeout, dropping
  assertions, reducing actions, silently skipping the row, or retrying until it
  passes. One station's timing is not a universal performance guarantee.
- AC4: Remove temporary diagnostic-only instrumentation when it is no longer
  needed, retain meaningful regression controls, review any affected source/input
  seals against the final bytes and actual evidence, and run the documentation
  gate. State explicitly that the v0.3.3 waiver has not been renewed for a later
  release.

## Closed in triage, 2026-10-26

Closed as stale. On 2026-10-26 the owner made Debug performance timings
permanently nongating: "From now and until the end of time I won't care
about debug timings, we dont even have to run those in debug"
([standing policy](../../development/release-test-profiles.md#standing-debug-performance-policy)).
AC3 asks for this Windows Debug row to pass inside its 120-second limit,
which is now a Debug timing criterion that the policy retires. R187
completes every phase in 115.62 s, so the row is slow, not hung. The policy
still requires investigation for a real hang, and there is no evidence of
one. The 200% workbench rows pass in Windows Release. The row's v0.3.3 and
v0.3.4 timeouts stay recorded as failures. If the row ever fails to
complete under a larger bounded watchdog, that is a hang and gets a new
card.
