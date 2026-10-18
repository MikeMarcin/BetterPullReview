# Codex ordered handoff progress — 2026-10-17

Mandate: `codex-handoff-2026-10-17.md`, direct execution, no delegated agents.
The resolved selection is `codex-handoff-2026-10-17.selection.json`.
Existing writers were stopped before handoff; process inspection found no builds
or test runners active. Existing worktrees and WIP remain authoritative.

The installed MASK readiness check reports `project.identity_changed` for the
main checkout; no global registry or skill configuration was changed. The
installed direct recipe and board helpers are usable. No authored JSON milestone
plan is attached to BPR-1577; discovery reports absent and bind refuses it. The
owner’s ordered Markdown handoff remains the execution and exit contract.

## Frontier

1. BPR-1577: Done and pushed in `376a4190b`. Six Overview rows and switch
   checks at 100%, 125% and 200% pass, as do format22/docs/diff checks. Session
   released through event 6; server PID absent; frozen root, worktree and branch
   removed. The stop request returned connection refused; process inspection
   established the recorded PID was no longer alive before removing resources.
2. BPR-1578: Done and pushed in `c38432adf`; 15 focused rows pass at
   `49b3c0f57`, with the later main allocation-only update merged. Format22,
   documentation and diff checks pass; existing composition pins match.
   All sessions released/stopped; frozen roots, worktree and branch removed.
3. BPR-1597/BPR-1568: Done and pushed in `19a3392d2` and `93ebf4eef`.
   All 28 focused rows pass at `c6c4eeff3`; format22/docs/diff pass.
   No schema addition. The accepted owner verdict closes BPR-1568 into
   BPR-1597, without changing the earlier Rejects. Session released, server PID
   absent, frozen root, worktree and branch removed.
4. BPR-1600: inherited WIP/card committed at `324653f18`, current main merged
   at `db2cd82d1` without conflict. Three affected targets build. All 14 focused
   key/catalogue/typing/Home/composed/refresh-feedback tests pass (30.69 s),
   as do format22/docs/diff. Focused clang-tidy22 passes all four touched translation units. Removing
   only `apply_refresh_key(home)` made the key test fail; exact restoration,
   rebuild and rerun pass. Finished/pushed in `ca55c0a71`; worktree, build and
   branch removed. No src/app delta.
5. BPR-1572: current main merged and composition resealed at `ce38d487e`.
   Round-four approvals and completed conditions confirmed. All 35 touched C++
   files pass format22; documentation and diff checks pass. Affected targets
   build and all 20 focused rows pass (239.48 s + 343.22 s), including both
   composition contracts. Prior unchanged tidy/live/negative controls retained.
   Finished/pushed in `57f335411`; worktree, build and branch removed.
6. BPR-1598: main `57f335411` merged at `e569d67aa`. Inherited timing
   implementation and negative controls are present; affected benchmark and
   replay targets build. The native tidy helper now selects documented LLVM 22
   and supports bounded read-only concurrency (`f579a02ca`); its CTest contract
   and documentation gate pass. The 899-unit inventory is complete; 121 runs reported findings. Fixes
   are in place, 74 changed test targets plus focused consumers build, full
   format22 and documentation checks pass, and replay’s five-second CPU
   negative control fails as intended. The initial complete native gate exposed follow-on findings; those are
   fixed and six direct rechecks pass. Source slices are `9e73d212d` and
   `b1b416c17`, with final residual fixes in `0bea4e26d`. All large-review rows
   pass (15 executed, one documented Debug skip), with the bounded retry used
   by responsiveness and history. All 84 focused rows pass (1,212.24 s). Nine final
   checks pass on `0bea4e26d`, covering the follow-on edits and composition
   reseal (201.40 s). The final full tidy gate passes all 899 translation
   units, and format passes all 1,628 files. Finished/pushed in `4305b7210`;
   worktree, branch and 12 GB build removed.
   No full-suite run per card.
7. BPR-1599: inherited fix preserved at `5721c7061`, main through BPR-1598
   merged at `68c701ea5`. The product loses a modality chosen during loading
   when arrival reapplies the opening preference. Source history identifies the
   older arrival reset and the loading view introduced by `9c670d886`;
   BPR-1588 is absent from the original failing base. The current-main negative
   control fails the deterministic Qt assertion at the lost Review choice;
   the native course passes on that timing (7.09 s). Exact source restoration
   is verified by SHA-256. Positive rebuild passes; all seven focused rows
   pass on `69a477a05` (96.21 s), as do focused tidy22, format22,
   documentation and diff checks. Composition pins match BPR-1598.
   The new regression also covers cached arrival and a later load.
8. Task-owned housekeeping: pending.
9. One final integrated configure/build/full ctest and format/tidy: pending.

Do not touch paused RFC-BPR-0085 work BPR-1589–1595 or its worktree.
