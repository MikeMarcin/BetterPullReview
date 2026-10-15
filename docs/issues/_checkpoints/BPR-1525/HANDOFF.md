# BPR-1525 engineering handoff

Authorized release source baseline: `8f0d25418cd4278065d5a043dabaa728feb202f1`.
Worktree: `/Users/jaeger/.codex/worktrees/v021-windows-gates/BetterPullReview`.
Branch: `codex/v021-windows-gate-fixes` (created on handoff; binding renewed
on that branch immediately before the WIP commit).

Allocation main commit `5148da5a8`, cherry-picked here as `7b21e4c5b`.
Plan `603c7aa27`; shared issue claim `bde16e769776a98a7b1e592d1e6e219a94bf3002`.
Card remains In Progress. Plan discovery reports found, structurally valid.
Initial binding JSON is beside this file; continuation selection supplied by
root lives at main `.mask/sessions/release-v0.2.1/continuation/strategy-selection.json`.
It records current owner-selected coordinator-implementer continuation only,
not historical remote route readiness. Root requires fresh native
`rfc-implementer` engineering ownership after this handoff. Previous generic
worker releases all source/build ownership and has no running task commands.

## Active producers: preserve

Windows task `BPR-v021-native-production-8f0d2541` is still running the full
normal gate in `C:/Users/mikem/bpr-v021`. Latest own observation ~260/705;
root later observed #282 failure. Do not edit that tree, stop/restart task,
launch competing tests/builds/WSL or assume the old candidate is final.
Read log through SSH alias `aito-main-win`:
`C:/Users/mikem/AppData/Local/BetterPullReviewRelease/v021-windows-production.log`.
Mac authoritative full gate also active in separate producer; inspect it before
new build/test work. No signing/auth/publication changes are authorized to this
engineering slice. Source corrections invalidate packaging of baseline8f0d as
final source; all platforms will ultimately need one corrected source commit.

## Current repairs (WIP; native verification pending)

1. `scripts/build_source_find_fixture.py` and
   `scripts/build_source_pr_search_fixture.py`: explicit UTF-8 and LF for
   construction JSON writes and PR Search's `today-only token` source file.
   Original default Windows text translation produces CRLF and changes both
   comparison bytes and later Git commit SHA.
2. Four application files approved explicitly by coordinator:
   `src/application/include/bpr/application/diff_line_matcher.hpp`,
   `src/application/src/diff_line_matcher.cpp`,
   `src/application/src/diff_application_host.cpp`,
   `src/application/src/review_cycle_comparison.cpp`.
   Old matcher waits `condition_variable::wait_for(1ms)` and polls stop;
   Windows can round to its ~15.6ms timer period. Original full native run
   `application_diff_host` reports stop16.2308ms against unchanged10ms budget.
   Required constructor stop_token is passed from both host construction sites.
   Scoped stop_callback locks shared call mutex and notifies; condition wait
   predicate sees Finished or token stop; waiter unlocks then invokes existing
   core meter.poll before accepting result. Callback registration happens
   without mutex held; callback destruction occurs after waiter unlocks.
   Detached matcher state remains shared and independently owned. No core API,
   threshold or existing assertion changed. Review races/lifetime and verify.

## Evidence already collected

- Full portable fixture contract `python3 tests/scripts/recorded_pull_request_fixture_tests.py`
  in isolated worktree passes `RECORDED_FIXTURE_OK`.
- Negative control emulating Windows default CRLF by overriding Path.open
  (explicit newline arguments preserved): original8f0d Find and PR Search
  small construction byte comparisons False; repaired comparisons True.
  Original later head under emulation564883f1fc22f88cd7599a5af3e6c6ae2b3dc829;
  repaired/committed headf7de4366ea00c840b15ece6b445eae7a320ef363.
  Large generation is covered by portable full contract, still needs Windows.
- Pinned format22.1.8 applied to four application files; `git diff --check` clean.
- `PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts python3 scripts/verify_documentation.py --root .`
  passes DOCUMENTATION_OK. Unqualified first attempt failed import board_model
  because this worktree has no .agents managed skill link (environment limitation).
- A disposable standalone matcher race harness at `/tmp/bpr1525-matcher-wakeup.cpp`
  compiles the changed production TU but **link failed** for core diff_components
  virtual methods/typeinfo; no harness executed and no concurrency pass claimed.
  Its planned250 stop/abandon races, normal completion and pre-stopped assertions
  may be reused after linking existing core library or focused target build.
- No native builds, tests, source changes or auth changes were launched by this worker.

## Remaining original failures

### Nine containment measurements (no fix yet)

`scripts/verify_test_network_isolation.py` intentionally fails Windows until each
unmeasured row is measured contained and actual refusal named:
`sqlite_remembered_connection_storage`, `review_cycle_git_reader_scale`,
`review_cycle_git_readers`, `git_metadata_harness`,
`application_appearance_older_theme_contract`,
`application_cpp_identifier_appearance`, `application_source_find`,
`application_source_find_scale_1_25`, `application_source_find_scale_2_0`.
Do not substitute an assumed reason, remove containment checks or weaken pins.
After original full native gate finishes, obtain exact CTest commands from its
manifest and prepend the existing sandbox wrapper to each currently uncontained
row for bounded direct measurements. Retain actual output/refusal.

### Child probe WinError5 (no fix yet)

`test_network_isolation_child` fails at
`tests/scripts/network_isolation_counterexamples.py:686`, subprocess.run child
CreateProcess. External, loopback and concurrent-launch probes pass. Per-command
read-only `icacls` confirms both `C:/Program Files/Python314/python.exe` and its
directory already grant ALL APPLICATION PACKAGES and ALL RESTRICTED APPLICATION
PACKAGES RX, so missing image package ACE is ruled out. Historical BPR-0903
passed same Program Files interpreter; old per-user image lacked ACE. Do not pin
this probe uncontained or count process creation denial as success: it must start
its grandchild, which proves its own AppContainer token and WSAEACCES10013.
Investigate executable spelling/path/cwd/runtime/stdio handles after gate quiesces.

### Fifth failure review_cycle_git_readers (diagnosis pending)

Full run #282 fails7.09s with generic `FAILED: fixture git command` after last
successful marker `NATIVE: core.ignoreCase folds ignore and filter matching,
config.worktree included`. Next row is `matches_posix_classes` in
`tests/workspace_contract/review_cycle_git_reader_tests.cpp:226–244`.
It calls make_repository(init/add/commit), config core.ignorecase=false, then
Git oracle `check-ignore --no-index config/local/a/key.pem` with ignore rule
`config/local**/*.pem`. Last command is likely failing, but not proven.
`workspace_fixture.hpp:140` suppresses stderr and prints generic failure.
After gate, use binary `--row posix-classes` and expose exact fixture command/
status with disposable diagnostics before any product change. Native installed
Git version may differ. Root will split if materially outside this slice.

## Next

Inspect native task/log before any run. Complete bounded native measurements,
child-launch diagnosis, cancellation public-host tests and race review. Preserve
all original failure evidence and assertions. Finish BPR1525 only after every
AC and exit is supported; full release gates/signing/publication remain root.
