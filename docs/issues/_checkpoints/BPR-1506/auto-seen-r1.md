# BPR-1506 automatic Seen checkpoint

- Assignment: `BPR-1506`, plan `bpr1506-auto-seen-v1`, milestone `automatic-seen-on-open`, mandate `owner-testing-2026-10-14`, recovered implementer `/root/bpr1506_recover`.
- Exclusive source/workflow root: `/Users/jaeger/.codex/worktrees/bpr1506-auto-seen/BetterPullReview`, branch `codex/bpr1506-auto-seen`. Coordinator owns integration; no main write/rebase or delegated agent was used.
- Baseline `3e8793add`; existing claim `850ad0c74`; authorized allocator metadata `5a105579c` adopted as `d4a2af14a`; product candidate `348bf36ee000b7f8ac60ae0ae31d0aa5387b6bf0`. Preserve unrelated writers in their own worktrees.
- Source paths: `src/app/src/github_application.cpp`, `tests/end_to_end/application_review_ledger_composition_contract.cmake`, `tests/end_to_end/accepted_file_in_overview_public_path_tests.cpp`, `docs/architecture/review-ledger.md`. Upkeep adds the live card, authored plan/source and this checkpoint.

## Candidate behavior

The successful ledger material answer supplies the exact selected publication and receipt. Automatic Seen requires the same current selected request, file, material, generation, ledger authority, epoch context, visible Review comparison and result alignment; pending tab/reading restoration is excluded. Revealing material loaded under Overview uses that successfully observed retained receipt once the exact tab is ready. A request is attempted once per open; explicit Mark unread therefore stays until reopening. Existing core policy decides the command and SQLite persistence precedes its accepted projection. Read/Accept/Exclude are not inferred.

## Commands and evidence

- Reconciled `ps axo pid,ppid,etime,command`: no prior BPR-1506 build descendant or generic cmake/ninja remained. Other ctest processes belonged to BPR-1518/1520 and were left alone.
- Reused existing `build/dev`, built with `cmake --build --preset dev --target better_pull_review better_pull_review_review_ledger_core_tests better_pull_review_review_ledger_host_tests better_pull_review_sqlite_review_ledger_tests test_accepted_file_in_overview_public_path test_overview_double_click_public_path --parallel 2`; final changed targets rebuilt successfully. Initial target spelling `BetterPullReview` was rejected and corrected before testing. Logs `/tmp/bpr1506-recovery-*-build.log`.
- `ctest --preset dev --output-on-failure -R '^(application_review_ledger_composition_contract|core_review_ledger|application_review_ledger_host|sqlite_review_ledger_storage|architecture_boundaries)$'`: four foundation checks passed. The added third unchanged-material run exposed the old PrivateWrites==1 expectation. Corrected it to zero only when Seen was already loaded.
- `ctest --preset dev --output-on-failure -R '^application_review_ledger_composition_contract$'`: passed (9.66 s), including hidden Overview, automatic Seen, stale/file/material/missing-result refusal, changed-head carry, unchanged Seen restart with no write, unavailable store, actionability and private record preservation. Log `/tmp/bpr1506-recovery-composition-tests.log`.
- `ctest --preset dev --output-on-failure -R '^(application_accepted_file_in_overview|application_overview_double_click)$'`: 2/2 passed (8.86 s). Explicit Read/unread/reopen and independent Accept behavior pass. Log `/tmp/bpr1506-recovery-integration-tests.log`. Full-window render `build/dev/accepted-overview-renders/review-automatic-seen.png` inspected offscreen.
- `python3 _coding-standard/scripts/format.py --dry-run src/app/src/github_application.cpp tests/end_to_end/accepted_file_in_overview_public_path_tests.cpp`: passed with clang-format 22.1.8. `git diff --check`: passed.
- `python3 tests/scripts/verify_documentation_tests.py`: 67 passed, `/tmp/bpr1506-recovery-doc-tests.log`.
- `python3 scripts/verify_documentation.py --root .`: known unrelated Ready dependency mismatch BPR-1521 -> BPR-1520. Log `/tmp/bpr1506-recovery-docs.log`. `issue_board.py validate --include BPR-1506 --json`: passed.

## Ownership, pending obligations and next action

All task build/test subprocesses complete before handoff; no server, live GitHub effect, owner desktop/profile access or candidate protection is retained. Frontend-design was read for visible progress behavior. No required human/native/independent acceptance is specified by this card; no attended session is needed. No temporary files/build output are committed.

Coordinator: integrate the product and canonical card finish commits, skipping `d4a2af14a` if `5a105579c` already exists in its history. Reconcile the separate P0 board dependency and run the global documentation gate on integration. Other source changes and file navigation/hotkey slices remain coordinator-owned.

## Final upkeep

Canonical finish `c8bffd47189d17370b049173b31437369846d80b` moved BPR-1506 to Done with all four passing acceptance records against product candidate `348bf36ee`. Scoped board validation remains valid and the working tree was clean. The final global gate still reports only the unrelated BPR-1521/BPR-1520 Ready dependency.

The original milestone check detected the card path/body fingerprint change caused by canonical finish and evidence additions. The acceptance text and plan remain unchanged. Rebound the same mandate/milestone as segment `auto-seen-r1-recovery`; [milestone-binding.json](milestone-binding.json) retains that post-finish binding. Runtime final observation and check live at `/tmp/bpr1506-recovery-observation.json` and `/tmp/bpr1506-recovery-check.json`.
