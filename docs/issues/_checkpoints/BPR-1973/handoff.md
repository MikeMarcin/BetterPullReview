# BPR-1973 Debug functional watchdog checkpoint

Mandate revision: `coordinator-approved-1973-2026-11-09`. Implementer
`/root/release_fixes_main`, own branch `codex/bpr1973-debug-watchdogs`, based
on exact reviewed BPR-1971 candidate
`970812bfe2a1107625c788a2e87d2a220e4ba93d` including accepted BPR-1972.
Plan/claim commits are `fadb04987` and `0b6840b1b`.

## Source and rationale

Only `tests/end_to_end/CMakeLists.txt` and the run-budget documentation change.
The helper accepts optional positive integer `DEBUG_TIMEOUT`, refuses invalid
or shortening values, and retains the stricter original paired-budget check.
Exactly seventeen existing short functional rows opt into 120-second Debug
outer watchdogs. Optimized outer limits remain 15 seconds, inner budgets
8000 ms, and domain predicates, sandbox, commands and environment stay intact.

The original build43 Windows Debug failures are retained externally: all
seventeen rows were killed by the 15-second CTest watchdog, at 15.07–15.32
seconds. These are lower bounds, not completed timings or semantic passes.
The standing owner policy permits this bounded Debug functional watchdog;
actual hangs and functional failures still require investigation.

## Focused proof

External evidence root:
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/pre-freeze-bpr1973/`.

- `external-preparation.json`: isolated actual CMake single/multi probes select
  Release15/Debug120 and retain argv8000; malformed `abc` and shorter `10`
  refuse configuration. `exact-helper-binding.json` proves the source helper
  exactly matches that probed helper and the reviewed proposal.
- `before-normal-graphs-receipt.json` and `after-normal-graphs-receipt.json`:
  normal source configuration, standard Mac features including CLI, owned
  APFS-cloned dependency cache, three disposable build directories. Required
  KSyntax dependency bootstrap occurs during configure; no application target
  or test is built/run. Neither protected producer nor canonical cache is
  written.
- `normal-graph-comparison.json`: 965 nodes in each single Release/Debug and
  multi Debug/Release/RelWithDebInfo graph. Only the exact seventeen Debug
  `TIMEOUT` values differ (15 to 120). Physical and logical commands, inner
  budgets and every other node/property match. Only CTest backtrace metadata
  is canonicalized. Generated raw CTest files independently match after
  backtrace and approved timeout normalization, covering the one JSON node
  without a resolved command too.
- The initial external raw-file comparator missed multi-config indentation.
  Its actual assertion failure/diff is retained; all five JSON comparisons
  already passed. Correcting that operator regex and reusing the exact saved
  graphs proves raw equality without another configure or test attempt.
- Documentation gate, scoped shared board validation and `git diff --check`
  pass. No C++ changes or broad native matrix is required for this scope.

## Integration and remaining obligation

Source is privately prepared; main integration deliberately waits for the
BPR-1971 source owner's actual Windows proof and normal main handback.
No ancestor correction is published early. Reconcile authoritative main and
counter/cards, then intentionally integrate/push this slice, finish the card
and update this checkpoint. The preserved BPR-1971 Mac proof branch remains
clean and unchanged. No task-owned command/review server is active.

Windows semantic execution with the supported 120-second bound remains the
final common-source preflight obligation. Mac configured graphs and isolated
helper probes supply no Windows execution or full release qualification
credit. Root owns protected R, frozen previews and all release operations;
the Windows implementer owns the shared PC. No producer source or PC command
is touched by this slice.
