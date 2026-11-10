# BPR-1982 legacy destination repair checkpoint

Assignment `legacy_destination_fix`, milestone `legacy-destination-fix`, owner `/root/legacy_destination_fix`, mandate `tour-destination-repairs-r1-legacy-destination-fix`. Baseline `992ebba9c6ea35822a24ea6c315676a865fb5f65`. Scope: one saved-receipt P2 upgrade regression.

Plan: claim follow-up; add actual SQLite/session regression; demonstrate failure; implement bounded compatibility and future-retarget preservation; focused tests/static/docs; explicit source and guarded finish commits. No remote writes, desktop effects, server, protection or worktree. Foreign `.codex-attachments/` untouched. All commands currently exited.

Legacy state: Destination A, Original B, no Mapped, exact_original, confirmed Remote at B. New receipts retain independent Original/Mapped and confirmed Destination. Centralize effective published subject and retain legacy confirmed subject before a local mapping transition.

Independent reviewer reproduction: `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1981-review-o69kane5`; previous BPR-1981 acceptance stays historical. Next: claim and bind, then regression.

## Active correction frontier

- Allocation/plan commit `7762afb61`; guarded claim `b99ec8da26016b7839955228c2c0944d2189ae8c`. Binding covers AC1–AC3 and all exits. Target-scoped discovery reuses the prior workaround for unrelated unsupported legacy plan amendments; normal live linkage/fingerprints still run.
- Permanent SQLite/session regression builds on baseline and fails at the new maintenance assertion after exact save/reopen equality passed. `/tmp/bpr-legacy-destination-fix/baseline.log`: CTest exit8, child exit1, 2.62seconds. No remote writes occur in that failed maintenance.
- Core helper selects Original only for the completed legacy shape (Remote, Original, no Mapped, exact_original); otherwise Destination retains precedence. Session and core update/source-delta comparisons share it. Core edit/observe-head retain the effective subject before mapping changes, preserving original/mapped ranges.
- Tests cover legacy and modern actual SQLite reopen, seven zero-write mismatches each, exact one-body maintenance, original/mapped/remote preservation; core covers later retarget C, confirmed-B no-op, source deltas and head changes.
- First fixed dev/public build and focused LLVM22 tidy are active; all commands owned by this implementer and must finish before return. No server/protection/worktree/remote or desktop effect exists. Next: consume fixed checks, core-only and boundaries, document exact candidate, commit and guarded finish.

## Committed source candidate

Source/test `4e21d72a5` commits the seven changed C++ paths. Dev and core-only builds pass. Final session passes2.39seconds; fourteen source/identity negatives refuse without writes, both saved receipt modes edit one body, all existing session regressions pass. Twelve other focused integration courses pass24.71seconds; their relevant inputs are unchanged after the modern negative fixture correction. Full actual failures are retained in the verification record.

LLVM22 format7files/tidy6TUs and docs/67documentation regressions pass. Core-only five functional courses and both architecture courses are running under owned exec session11660; architecture_boundaries already passed20.07seconds, contract pending. No other owned command remains active. Next: consume required boundary result, finish verification/evidence and guarded closeout.

## Verified correction

Exact source/test candidate `4e21d72a50135e7226e4c0a3ab389cebff03bf20`. Changed paths: `src/core/include/bpr/core/review_tour.hpp`, `src/core/src/review_tour.cpp`, `src/core/src/review_tour_update.cpp`, `src/core/src/review_tour_revision.cpp`, `src/adapters/github/src/staged_review_session.cpp`, `tests/core/review_tour_update_tests.cpp`, `tests/github_contract/staged_review_session_tests.cpp`.

Final core-only run passes all seven courses, including both architecture boundaries (`core-tests-final.log`). All task-owned build, CTest, tidy and documentation commands have exited; results consumed. No writer/server/protection/worktree or external/desktop effect remains. Foreign `.codex-attachments/` is untouched. All AC1–AC3 and milestone exits pass against this candidate; no additional independent/human/native acceptance is required or claimed. Historical terminal acceptance and unrelated live/native gaps remain intact.

Next: guarded tracker finish with verification/checkpoint/binding, post-move docs/board/consistency check, then coordinator report.

## Final guarded closeout

Guarded finish `98268c729b82e5a3628d7a8c9d87b7a095f200f1` moved BPR-1982 to Done with passing AC1–AC3 records against source/test `4e21d72a50135e7226e4c0a3ab389cebff03bf20`; no card requirement or historical acceptance was rewritten. Allocation/plan `7762afb61` and claim `b99ec8da26016b7839955228c2c0944d2189ae8c` precede it.

Core-only7/7 passed195.14seconds, including architecture_boundaries20.07seconds and architecture_boundary_contract173.36seconds. The13dev courses have passing evidence across the twelve-course integration successes and final2.39second session recheck. All six changed TUs pass LLVM22 tidy, all seven changed C++ files pass format, docs pass and documentation regressions remain67/67. Actual failed commands and exact selectors are in `docs/verification/BPR-1982-legacy-tour-destination.md`.

The consistency helper returns `report-closeout`; `engineering_acceptance: not-evaluated` means it checks consistency, with actual engineering judgment in the criterion ledger. Current-HEAD observation and check are external at `/tmp/bpr-legacy-destination-fix/{observation,check}.json`. All build/test/tidy and failed interim commands exited and results were consumed. Only final documentation/board/checkpoint upkeep follows this entry and is completed before return. No source writer, review server/session/protection, task worktree, mount or remote mutation remains; foreign `.codex-attachments/` is unchanged.

Next action: coordinator reports this bounded P2 repair complete. No selected-scope correction, owner decision or new review round remains. Separate live/native gaps retain their original scope/status.
