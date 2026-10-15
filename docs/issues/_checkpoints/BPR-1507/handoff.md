# BPR-1507 handoff

Mandate `owner-2026-10-14`; assignment source baseline `ae7d22844`; exclusive
worktree `/Users/jaeger/.codex/worktrees/bpr1507-file-accept/BetterPullReview`,
branch `bpr-1507-one-click-file-accept`. Engineering owner `/root/bpr1507`.

The named Accept button sits beside the existing state menu and uses the
canonical acceptance QAction. Its inverse says Revoke acceptance; disabled
states preserve Include in the adjacent menu. No core/host policy changed.
Product candidate `e483ef9997f3e85632e44c8c46c0fb34682cd792`.

AC1–AC4 pass in [acceptance.json](acceptance.json). Eleven focused CTest rows,
67 documentation tests, format22 and diff whitespace pass. Offscreen light/dark
state sheet and full composed source-diff window inspected. The frozen launcher
check also passes. Commands and actual timings are recorded on the
[card](../../60-done/BPR-1507-one-click-file-accept.md).

Canonical finish was attempted with the acceptance file and explicit owned
paths. The guard refuses local NEXT_ID `BPR-1516` against shared historical
high water `BPR-1520`. No manual lane/counter edit or bypass was attempted.
The documentation gate has the same sole reported failure. Coordinator must
integrate its newer testing-note imports, then rerun gate and canonical finish.

No explicit human-only criterion exists. The initial engineer plan added a
generic human gate; the final acceptance audit corrected it while retaining
all owner criteria and the original plan snapshot. The mechanical amendment
checker rejected changed exit text; its refusal was not treated as a verdict
or permission. There are no prior human decisions or protected candidates.
The unused unprotected draft session was removed. No review server or waiter
was started. No owner desktop or browser data was read.

Optional frozen artifact: `/Users/jaeger/bpr1507-one-click-accept-v001.xav8xdr7`,
source `e483ef9997`, executable SHA-256
`97e7edd6763036898d3da35fbf7016f5dfc7b0bd9ec55286ae9cab0cf72cbfda`.
It contains its own source/build/fixtures. Optional launch:
`python3 /Users/jaeger/bpr1507-one-click-accept-v001.xav8xdr7/launch.py`.
Use `scripts/freeze_attended_build.py remove` with that exact root when done.

Changed product paths: `src/adapters/qt/include/bpr/qt/workbench_window.hpp`,
`src/adapters/qt/src/workbench_window.cpp`,
`src/app/src/github_application.cpp`,
`tests/adapters_qt/workbench_window_tests.cpp`.
Owning docs: `docs/architecture/review-ledger.md`,
`docs/ux/workbench-specification.md`; plan/card/checkpoints are scoped BPR-1507.

Source integration: cherry-pick owned plan `d6b3b10b7`, claim `60f4b965f`,
product `e483ef999`, and the following verification/upkeep commits. Exclude
interleaved import-only testing-note commits already present on main.
Coordinate the composed receipt course with BPR-1506’s automatic Seen changes.

Every task-owned configure/build/test/freeze process has finished; no server,
waiter, subagent or background command remains. Writer ownership is released
at return. Runtime evidence is under `build/bpr1507-runtime` (uncommitted).

BPR-1515 is a separate next milestone for a fresh implementer. The visible
button is found as `workbenchAcceptFileButton`; the canonical toggle action is
`toggleSelectedFileAcceptanceAction`. Its new hotkey must accept only and
must not bind repeat presses to the toggle action’s revoke command. Preserve
text input and exact projected file/material eligibility.
