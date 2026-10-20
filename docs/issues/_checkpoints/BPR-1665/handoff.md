# BPR-1665 Qt6.12 upgrade checkpoint

Mandate `qt-6-12-r2`; owner approved macOS14.4+ and requested full macOS suite.
Assignment: `/root/qt612`, single milestone `qt612-upgrade`, sole task-branch
writer on `codex/qt-6-12`. Baseline `1a7d74f003b656469f5c6822c655a314fb58f663`.
Source pins/preset14.4/docs and fixtures are implemented. All three source
archives and the new ICU archive match published SHA256; the ICU73.2 raw
trio is unchanged. Pinned supplied-root configure and focused app build pass.
Full dev build and fresh-cache provider course are active; complete full811
CTest suite next. Runtime evidence and logs: external `/tmp/bpr-qt612-run`.

Target-only `milestone_checkpoint.py bind`/plan discovery refuses because an
unrelated existing shared-action-buttons plan lacks boundary fields in later
milestones. Explicit `milestone_plan.py bind --plan` resolves this one-milestone
plan and its obligations. `checkpoint.json` uses that exact explicit binding
and the canonical checkpoint schema/repository-state helper; final consistency
check will use `milestone_checkpoint.py check`. Unrelated plan bytes remain
intact. The original refusal is in external `checkpoint-bind-attempt.json`.

Earlier preview build had one transient missing KSyntax header because the
implementer overlapped its reconfigure. Sequential pinned build passes; no
product source repair was needed. No release/version/publication changes or
owner desktop operation. Windows/Linux native runtime and package evidence,
trust/vulnerability/native device qualification and v0.3.1 production belong
to the coordinator's subsequent release milestone.
