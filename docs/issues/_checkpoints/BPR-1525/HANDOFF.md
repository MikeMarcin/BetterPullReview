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

## Active native implementer continuation (2026-10-15 UTC)

Owner `/root/bpr1525_implementer`, native role `rfc-implementer`, configured
`gpt-6.1-sol` / `xhigh`; no differing effective runtime value was exposed.
Mandate remains `authorized-release-continuation`, selection and milestone unchanged.
Fresh binding is `implementer-binding.json`; adopted source `a8abc5451`.
No ownership of a running command was inherited. No subagents spawned.

Original Windows gate is still active (most recent observation 372/705); its
source, build, task and processes remain untouched. No new Windows/WSL tests or
builds, and no Mac compilation, have been launched by this implementer.
Original log snapshot and CTest manifest are retained outside Git at
`/tmp/bpr1525-original-windows.log` (UTF-16) and `/tmp/bpr1525-manifest.json`.

Additional bounded repairs in the worktree, native verification pending:

- `tests/workspace_contract/git_metadata_oracle.hpp`: use native directory file
  IDs on Windows. 85 of the original 89 unclassified GMR findings are C4; the
  old Windows oracle always returned no identity while K returned actual IDs.
- `tests/workspace_contract/{review_cycle_git_reader_tests.cpp,git_metadata_harness.cpp}`:
  retain conservative capture exclusion for `config/local**/*.pem` matching
  `config/local/a/key.pem`. Apple Git 2.50.1 ignores it; the original Windows
  Git 2.55 oracle keeps it (four C5/C5-capture findings, both ignoreCase settings).
  Upstream `dir.c::match_pathname` changed from stripping the entire literal
  prefix in v2.50.1 to retaining one byte in v2.55.0. The coordinator confirms
  existing RFC83 H1's documented deliberate-choice rule covers preserving this
  confidential-capture policy. Classification binds candidate/component/case,
  exact subject and both actual answers; unknown/reversed findings still fail.
  The reader row asserts either known Git result independently and retains the
  existing capture exclusion assertion. Exact native corrected course pending.
- `cmake/WindowsTestIsolationSetup.cmake`: configure-time contained Python
  descendant probe; only if it fails, grant the named test container RX to the
  canonical selected interpreter runtime directory and re-probe. No capability
  or broad machine ACE changes. The real CTest interpreter is the per-user
  `C:/Users/mikem/AppData/Local/Python/pythoncore-3.14-64/python.exe`; its image
  ACL has neither package nor named-container access. Program Files Python has
  package RX but is not the selected interpreter. This corrects the previous
  handoff's incomplete image diagnosis. Native negative/control evidence pending.

Primary source evidence for Git's compatibility difference:
`https://raw.githubusercontent.com/git/git/v2.50.1/dir.c` and
`https://raw.githubusercontent.com/git/git/v2.55.0/dir.c` (`match_pathname`).

`gh_cli_transport` has a test-composition mismatch: RFC78's new strict acquisition
courses expect sensitive dispatch on Windows, while production deliberately
refuses pinned Windows launches before the fake port (POSIX-only private account
directory). No production auth change is authorized. Inspect/qualify exact
Windows unavailable behavior and preserve portable compiler/parser checks.
`github_provider` timeout cause remains unproven and needs native tracing after
producer quiescence. Original full gate may reveal more failures.

Next: keep native gate intact; after it exits, collect complete results, reproduce
selected-Python contained descendant failure, verify narrow runtime permission
repair and nine genuine contained measurements, then build/test exact corrected
candidate in a separate owned Windows repair checkout. No acceptance or release
source freeze is claimed. Formatter 22.1.8 applied to new workspace C++ edits;
`git diff --check` passes. All changed/new code still requires focused native
and portable verification before canonical finish.

## Quiescent checkpoint authorized by coordinator

Root instructed this implementer to finish static/doc checks, commit an
intentional checkpoint and return while the original native gates retain their
stations. Resume SAME implementer when root releases the first native slot.
Source ownership is preserved here; root will not edit this worktree. No
attended wait or human decision is being handed off.

Implemented test-only CLI correction: portable configuration/request compiler,
inventory/help parsers and changed-executable refusal always run. Windows has
actual cloud/GHES pinned read/capability/mutation tool-refusal checks with no
credential/API/witness/private-config effects; successful private-account
strict dispatch/recovery courses report an explicit POSIX-only skip. No
production authentication/profile changes. Native compile and acceptance pending.

Latest original Windows observation418/705 adds `qt_diff_workbench_scale_100`
timeout120.01s; output only offscreen size-hint warnings, scale125 then started.
Cause unproven. Collect final inventory before repair/scope decisions; do not
change timeout. Existing `github_provider` timeout and nine containment pins
also remain pending. Both full gates remain untouched and active.

Verification now completed: documentation gate passes; verifier tests67/67 pass
(their intentional negative-control ERROR messages are expected); format22.1.8
checks all8 touched C++ files; diff whitespace clean. Self-reviewed synchronized
matcher wait, native file-handle lifetime, exact answer-bound classification,
conditional selected-runtime ACL scope and Windows zero-effect fixture path.
No C++ compilation/tidy, corrected native pass or classification/race negative
control is claimed. Original portable fixture/CRLF controls from a8abc5451 are
unchanged and retained.

Exact next native commands, target names, environment/token requirements and
pending controls are in `NATIVE_NEXT.md`. Disposable measurement script
`/tmp/bpr1525-measure-native.py` is syntax checked only. Keep it, original log
snapshots and manifest outside Git for resumption. No task-owned process,
build, test, server or remote mutation is left running. No original producer
or native station process was stopped. No integration to main or release-source
freeze has occurred.

Actual runtime/mandate binding is `runtime-binding.json`, paired with the strict
shared `implementer-binding.json` schema. The shared checker requires a separate
structured observation; the initial invocation omitted it and was refused.
The corrected pending observation is retained under this worktree's private
`.mask/sessions/BPR-1525/` runtime directory after the checkpoint commit.


## Authorized combined native repair scope, revision 2

The coordinator explicitly expanded this same single-implementer milestone to
include separately tracked BPR-1526, allocated by root commit 4cc67de6836271c77a3e4e5bd7bfc719cc5c65ba
and adopted here as bbbe57e96. This is an authorized scope/requirement
reconciliation, not a grouping-only amendment: the shared plan amendment checker
intentionally refuses changed scope/requirement atoms. Preserve the prior plan
and binding in 0445d7e0afb1d75e3f695e4e282c6c5e37b8cce6 history. All original BPR-1525
criteria and exits remain; BPR-1526 adds its own action acceptance unit and exit.
Mandate revision: `authorized-release-continuation-native-gates-r2`.

Both native slots are now released. Root stopped and preserved the original
failed/incomplete gates: Windows 426/705 with 11 failures/timeouts; Mac 579/734
with workshop press failure and recorded cache skip. Root verified the recorded
process trees absent. We stopped neither original gate. We may reuse
`C:/Users/mikem/bpr-v021` only on a clearly named repair branch after preserving
its clean baseline, and build Mac only in this worktree. No auth/signout/reboot,
release redispatch or main integration is authorized by station availability.

The first Mac focused build compiled 832/838 steps and failed only because the
Windows-only refusal helper was intentionally discarded but lacked an explicit
maybe-unused annotation. Correct that scoped compile issue and continue native
validation. Selected Windows Python diagnosis now reproduces the contained
WinError 5 with the actual per-user executable; inside it sys.executable points
to the inaccessible per-user image while sys.prefix points to Program Files.
No positive ACL repair or focused corrected native pass is claimed yet.
