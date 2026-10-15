# Native continuation commands

Read [CURRENT.md](CURRENT.md) first for the active task and exact candidate.
The previous separate-checkout/no-slot instruction is superseded: root released
both original producers and authorized reuse of the clean Windows/Ubuntu
producers on named repair branches. Keep their original 8f0d25418 history.

Windows commands use Interactive Limited session 1 through the task-owned
PowerShell wrappers in `C:/Users/mikem/AppData/Local/BetterPullReviewRelease`.
They import the configured VS 18 vcvars64 environment and preserve ordinary
Python/Git/Qt discovery. SSH session zero is only dispatch/observation.
The current r18 wrapper owns its entire build/test/restore sequence; inspect
its receipt and process tree before any new Windows/WSL command. Its source is
`bpr1525-native-focused-r18.ps1`; the encoded SSH helper is
`/tmp/bpr1525-remote.py`. Never launch a competing build or suite.

Normal focused gate commands, from the producer root after its MSVC setup:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev --parallel 8 --target test_better_pull_review_qt_diff_workbench_tests
ctest --test-dir build/windows-dev -C Debug --output-on-failure -j1 -R '^qt_diff_workbench_scale_(100|125|200)$'
```

No gate deadline, input size, assertion or case count may be relaxed. Functional
profiling under an extended diagnostic process bound is not normal acceptance.
Any disposable source mutation is backed up outside Git and restored/touched,
then its target is actually recompiled before positive acceptance.

After Windows quiescence, inspect the prepared Ubuntu 26.04 WSL producer at
`/home/mmarcin/bpr-v021`, preserve clean/history state, adopt the repair candidate
on an owned branch and run one focused compile/affected-path pass. Record the
actual distro, compiler, preset, sandbox and result; do not infer a native
result from Mac or Windows. Root owns the later full release matrix/packages.

No auth, signing, publication, signout, reboot or mutation of the separate
`C:/code/git/BetterPullReview` station belongs to this milestone.
