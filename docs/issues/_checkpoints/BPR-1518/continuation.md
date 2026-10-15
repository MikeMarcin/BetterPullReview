# BPR-1518 engineering checkpoint

Assignment `/root/bpr1518`, mandate `bpr1518-r1`, milestone
`bpr1518-next-change/cursor-relative-change`, recipe coordinator-implementer r2.
Source/tracker/runtime root is the isolated `bpr1518-next-change/BetterPullReview`
worktree, branch `codex/bpr1518-next-change`, baseline `583eb4689`.
Plan `85d527a52`; claim `dfaa68f69`; [binding](binding.json) records the complete
AC1–AC4 and all three milestone exits. Frontend-design was loaded before UX edits;
MASK reviewed registration and [preflight](preflight.json) report ready_pinned,
without claiming runtime host loading.

## Diagnosis and candidate

A clicked source line already reaches the correct exact semantic selection.
`diff_interaction` and composition's `observe_change_navigation` discarded an
unchanged line's position by reducing it to an absent group. Group navigation
then entered at the first/last hunk. The candidate adds an indexed source-line
navigation query and bounded `AtUnchangedLine`/ordinal facts, preserving exact side,
comparison, group order and existing explicit stop/wrap semantics. Stop bounds
retain the source selection. No Qt geometry/resize path changes.

Changed paths: core diff_semantics header/source, diff_interaction source,
workbench_interaction header/source; composition's observe_change_navigation;
core semantic/command tests; existing composed change-arrows test; owning
workbench UX contract; issue and this checkpoint directory. Parallel workers own
BPR-1516 geometry, BPR-1517 file navigation and BPR-1519 layout. Their changes are
not present or reverted here; coordinator owns integration.

## Verification frontier

Product/UX candidate `64b76b673d78bc3ad3594a21962e4702a676bf30`; final composed
endpoint-click extension `c703ed292` adds no product changes. Shared tracker
metadata `5a105579c` is applied as `26bb690ae` per coordinator instruction.

Documentation gate and all 67 documentation-verifier tests pass. The eight
changed C++ header/core/test files pass clang-format 22.1.8. App formatting is
limited to the edited function because unrelated existing formatting is retained.
All three changed core translation units pass clang-tidy 22 with warnings as
errors. Dev architecture_boundaries passes (23.84s), architecture_boundary_contract
passes (205.56s); the unchanged contract evidence is reused for the Qt-free graph.
Dev/core-only configure succeed with C/C++ Debug flags `-g0` and the designated
shared CPM source cache. Qt-free core semantics, workbench commands and selected
graph boundaries pass 3/3 (15.42s).

The positive composed course passes (38.87s): 17 files, 37 bounds, 8 ordinary
change steps, 32 clicked origins, both sides/directions and unequal mappings;
it also replaces the same file with base/base and rejects the stale selection.
[Negative control](negative-control.json) switches only the core transition back
to group-only origin: the course fails at the exact hunk/source identity assertion
(10.97s). The fixed source restores byte for byte and its rebuild succeeds.
Final candidate `c703ed292` additionally clicks inside both endpoint groups.
Final positive focused checks pass 3/3 (50.83s), including the composed course
with 46 origins (50.14s). All task-owned commands are complete; no command is active. No descendant, review server, owner desktop action or remote effect.

Two corrected development observations are retained: the first build refused the
draft optional-position representation because core state grew to 136 bytes; the
candidate now tags the existing Ordinal with AtUnchangedLine and retains the
128-byte static assertion. The first viewport assertion exposed a bad test setup
that directly moved a hidden native scrollbar; test setup now uses the shared
controller before the actual click. No Qt geometry product edit was necessary.

Pending: acceptance-driven finish and final checkpoint consistency check. No human/native/independent criterion is required.
Next action: consume the final focused result, finish BPR-1518 and return exact
commits to the coordinator for integration with parallel work.
