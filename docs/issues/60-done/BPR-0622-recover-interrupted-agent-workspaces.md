---
schema: "implement-rfc.card.v1"
id: "BPR-0622"
title: "Recover interrupted agent workspaces with durable exact ownership"
status: "Done"
rfcs: ["RFC-BPR-0013"]
platforms: ["win","linux"]
depends_on: ["BPR-0621"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"R1 durable bounded ownership/recovery path distinct from session metadata, reusing exact quarantine/removal","outcome":"pass","revision":"c94e02b1 (Windows), integrated in main 2ba02ddb; Linux confirmation at 780d0a1c","evidence":{"kind":"artifact","detail":"Unix production integration sections on this card; docs/verification/BPR-0622-windows-native-evidence.md slices (a)-(d); dsp-002 critical review of 743c59d7..c94e02b1 found no blocker."}},{"criterion":"R2 reclaim provably owned abandoned workspaces, preserve live owners, no name sweep or substitute deletion","outcome":"pass","revision":"c94e02b1 / 780d0a1c","evidence":{"kind":"command","detail":"application_agent_crash_recovery, application_agent_materialization_crash_recovery and agent_workspace_ownership pass on Windows (native evidence file), macOS (build/r2-logs/dsp2-int4-ctest.log, dsp3-int1-ctest.log) and Linux (docs/verification/dsp2-linux-confirmation.md, BPR-0622 slices (c) and (d))."}},{"criterion":"R3 crash points, recovery twice, exact refs and session identity, fresh workspace and monotonic session without replay","outcome":"pass","revision":"c94e02b1 / 780d0a1c","evidence":{"kind":"command","detail":"Both crash-recovery courses and the eight application_agent_invocation_* rows pass on all three platforms (same evidence locations)."}},{"criterion":"R4 missing or untrusted ownership is a visible repair/refusal; bounded admission; no authority for unrecorded roots","outcome":"pass","revision":"c94e02b1 / 780d0a1c","evidence":{"kind":"artifact","detail":"agent_workspace_ownership refusal and substitution cases on Windows (reverted trials in the Windows evidence file) and POSIX; pre-ownership window documented in this card's Unix sections and the Windows evidence limits."}},{"criterion":"R5 reclamation assertion replaces orphan preservation where durable ownership exists; substitution and live-owner negatives through the real path","outcome":"pass","revision":"c94e02b1 / 780d0a1c","evidence":{"kind":"command","detail":"Crash-recovery courses assert reclamation; ownership course negatives pass on Windows, macOS and Linux (evidence above)."}}]
---

# Recover interrupted agent workspaces with durable exact ownership

BPR-0621's composed SIGKILL course retained the interrupted workspace through
two ordinary application reopens. Native identities and content stayed equal;
Git/SQLite recovery and zero agent replay passed. The process guard stops the
agent but cannot reclaim the application's disposable checkout. At that revision,
`github_application.cpp` created a random sibling QTemporaryDir, disabled its
generic removal, and retained the exact parent/workspace capabilities only in
memory. A new application created a different root and could not recover those
capabilities from the session catalog, which intentionally stores no paths.

## Required outcome

- Give disposable workspace lifetime a durable, bounded ownership/recovery
  path in the application host and installed-Git adapter. Keep original
  parent/child native identity, exact scope and ownership authority distinct
  from session metadata. Reuse the existing exact quarantine/removal machinery.
- Reclaim provably owned abandoned workspaces after application death, before
  reporting complete resource recovery. Preserve workspaces owned by another
  live application; use actual lifetime/locking evidence, not PID existence
  alone. Do not run a directory-name sweep or recursively delete a same-path
  substitute, symlink target, user checkout or retained Git cache.
- Crash points must include durable ownership preparation, materialization,
  running agent and interrupted cleanup. Run ordinary recovery twice to prove
  idempotence and retain exact Git refs/SQLite session identity. New invocation
  must use a fresh workspace and monotonic session ID with no automatic replay.
- Treat missing/untrusted old ownership evidence as a visible repair/refusal
  case. Bound admission and retained ownership records so crashes cannot create
  unbounded new workspaces. Do not infer removal authority for historical
  unrecorded roots. Document any unavoidable pre-ownership crash window and its
  bounded admission/repair behavior.
- Replace BPR-0621's explicit orphan-preservation observation with the stronger
  reclamation assertion where durable ownership exists. Add substitution and
  simultaneous-live-owner negatives through the real selected effect path.
  Keep safe preservation evidence for roots without durable authority.

## Scope before claim

Inspect `installed_git_process.hpp/.cpp` workspace identity and quarantine
operations, `agent_workspace` ports/host lifecycle, and application construction
and shutdown. Establish exact owned files and persistence format before source
changes. Use Glaze if JSON is chosen; do not add workspace paths to the immutable
agent-session catalog. Keep decisions in core/host and filesystem effects in
adapters. No credentials, live agent, paid service, GitHub Actions or push.

BPR-0617 remains the closeout anchor at RFC base
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`. This card stays open until the complete
recovery/resource outcome is demonstrated; BPR-0621 alone cannot close it.

## Implementation plan and ownership

First establish lifetime exclusion at the process boundary. A bounded optional
host value names an exact regular guard-file identity; the Unix launcher opens
it read-only without following links, checks native identity/private ownership
and retains a shared file lock. The bundled supervisor receives a duplicate of
that same locked open file description and keeps it through group teardown;
the external agent receives no guard descriptor. Recovery will need an
exclusive lock, so application death cannot release exclusion before the
supervisor stops the child. Unsupported platforms must refuse a requested
guard until their lifetime transfer is implemented, never silently discard it.
No production caller requests this guard until the durable workspace owner is
wired; that later integration is required for this card to finish.

Own `agent_launch.hpp`, the agent launcher/supervisor and their focused canary,
parent-death runner and architecture allowance. Then own the workspace host/port,
installed-Git workspace operations, application composition and focused Git/
composed recovery tests for durable records and recovery. Use existing native
identity/quarantine operations. Bound registry records and use a versioned
Glaze record if JSON is selected; reserve ownership before directory creation,
persist identity before materialization, and record a chosen quarantine path
before rename. Keep all scans and recursive cleanup on the effect lane. Root
construction, materialization and cleanup need crash checkpoints. This plan
is not acceptance of an unwired lifetime guard or incomplete journal.

## Lifetime prerequisite — implemented, card still In Progress

Source `e50edf77cc0af477bdfdc7d0ef269108e7f61722` adds the optional host-owned guard reference and Unix
launcher/supervisor retention described above. The adapter checks canonical
path, exact regular-file identity, private ownership and single link, and
retains a shared lock through a read-only descriptor. The supervisor inherits
only the designated duplicate and closes it in the external child before exec.
A requested guard on other platforms refuses; no production workspace owner
supplies this reference yet. Durable registration, restart reclamation and
non-Unix guard transfer remain unimplemented, so this card is not complete.

The real parent-death test freezes the supervisor with SIGSTOP, then kills the
application by PID or isolated process group. An independent exclusive lock
must still fail while the paused supervisor owns the running agent. Resuming
it stops the agent/descendant; exclusive access and a fresh launch then succeed.
This test initially failed: application death orphaned the stopped supervisor's
process group, causing a hangup/continue and supervisor exit. The launcher now
starts the supervisor in its own session, avoiding that orphan transition.
The fixture's failure cleanup was hardened to tolerate an already-dead
supervisor; the two exact leaked fixture processes from the first attempt were
identified, killed and independently confirmed absent.

### Local verification — 2026-09-05

- Dev canary target builds. `agent_parent_death` passes in 25.15s (25.16s total),
  including live and dead-parent lock exclusion, both PID/group kill courses,
  post-teardown release and fresh launches. Its preparatory public-launcher
  course also passes busy-lock, same-path substitution and symlink refusals
  before the external child side effect; successful guarded execution exposes
  no guard descriptor to the agent.
- Final `qt_agent_launch_canary` and `architecture_boundaries` pass 2/2 in
  79.46s (71.46s and 7.99s). The isolation manifest passed in 3.38s before the
  session-only repair; its registration inputs are unchanged.
- Tests-off `app-release` application builds after the session repair. Built
  and bundled supervisor SHA-256 both equal
  `58a20eee115f95505844123074c8178a9f02ff7a334f2e1202d5ee99ec650fb3`.
  `otool -L` lists only libc++ and libSystem. The application was not launched.
- Documentation gate, changed C++ clang-format checks and `git diff --check`
  pass. No GitHub Actions, push, live credentials, live agent or paid service.
  Task build logs are removed after recording results; test roots are consumed
  by their contained runners. No release application sign-out was performed.

## Journal plan before implementation

Implement the durable registry and integrate its lifecycle on the effect lane.
Carry its exact guard reference from the owned workspace into the process
request; the native primitive above is ready for that handoff. Retain the
owner's shared lock during preparation, materialization and normal cleanup;
restart needs exclusive access plus durable original ownership and scope.

Use the installed-Git native identity/quarantine operations. The current
`quarantine_and_remove_exact` chooses a random destination and changes only an
in-memory path; a durable cleanup record must publish its chosen quarantine
path before rename and recognize both original and quarantine identities on
restart. Do not substitute a post-crash pathname capture for original ownership.
Reserve bounded journal capacity before creating a parent/root, persist exact
identity before materialization, and leave unknown pre-ownership artifacts
preserved with bounded refusal/repair. Recovery scans and recursive removal
must not run on the Qt/decision lane. Preserve the immutable SQLite session
catalog format; use a separate versioned Glaze record if choosing JSON.

Add preparation/materialization/cleanup kill cases, live-owner and substitution
negatives, then upgrade the composed BPR-0621 orphan observation to prove actual
reclamation of a durably owned workspace. Run repeated ordinary recovery and
session #2 with unchanged Git/SQLite identities. Keep safe preservation for
historical roots without ownership receipts. Only then consider Done.

## Unix ownership journal prerequisite — historical checkpoint

Source `77651b10` adds `agent_workspace_ownership.hpp` and the native implementation
beside the installed-Git identity/quarantine helpers. No production caller uses
this API yet. The existing application still loses its in-memory workspace
capabilities; this prerequisite does not establish composed reclamation.

Each workspace uses one private append-only Glaze JSONL journal, bounded to
64 KiB, whose original regular-file identity also serves as the supervisor guard.
The owner keeps a shared lock; recovery requires an exclusive lock and validates
original registry, neighbourhood, journal, parent and child identities. Complete
records preserve the application/agent session, full authority IDs/fingerprints
and pinned head across legal phase transitions. Ready ownership additionally
requires the expected Git marker. Cleanup journals its phase before renaming to
a fixed quarantine name, then checks the original native identity before removal.
Destructor release never implicitly deletes a directory.

Admission permits at most 64 records, counts live locks, and examines at most
4096 direct neighbourhood entries. A workspace-shaped name without a journal
causes repair/refusal; a name never authorizes removal. Oversized, malformed,
substituted or missing records stop allocation. A partial final JSON line is
ignored only after complete earlier records validate. An exclusive mkdir before
its identity receipt is the remaining ambiguous preparation window: retain the
artifact and journal, refuse further allocation, and require repair. Unknown
historical roots remain outside this API's deletion authority. The caller must
select a canonical private neighbourhood outside the retained cache and user
checkout. Non-Unix implementations return unavailable.

### Local journal verification — 2026-09-05

- Production `better_pull_review_adapter_git` and the dedicated native fixture
  build successfully, including Glaze with strict missing/unknown-key and
  trailing-input handling. No production fault checkpoint is compiled in.
- Final `agent_workspace_ownership` passes in 3.34s (3.35s total). Fourteen actual
  SIGKILL courses cover every committed phase, both quarantine renames, both
  mkdir-before-identity windows, a live owner and substituted quarantines.
  Recovery runs twice after each recoverable checkpoint. Normal cleanup and
  failed-exclusive-lock/shared-lock restoration pass. The 64-live-owner limit,
  65th-allocation refusal, released-slot reuse, 4096-entry inventory bound,
  cancellation, missing journals, partial tails, hardlinks, symlinks, replaced
  parent/child/neighbourhood/journal identities, malformed scope/authority/phase,
  missing keys, trailing input and oversized records are exercised. An external
  checkout reached through a workspace symlink remains unchanged.
- Before the final missing-journal accounting and extra fixture negatives,
  `git_process_contract` (84.24s), `architecture_boundaries` (10.13s), isolation
  manifest (3.68s) and the journal course (2.77s) passed 4/4 in 100.85s. Existing
  Git execution paths and test registration are unchanged by those final native
  journal checks; that focused Git/architecture/registration evidence is reused.
- Documentation gate (`DOCUMENTATION_OK`), changed C++ clang-format checks
  and `git diff --check` pass. Verification is
  local, network-isolated fixture work; no GitHub Actions, push, paid agent,
  production sign-in or release application launch occurred.

### Integration plan at the journal checkpoint

Replace the unrecorded QTemporaryDir lifecycle with ownership prepared on the
installed-Git/cache effect lane. Materialize into the journal's exact empty
child, retain its owner through execution and cleanup, and carry `layout().Guard`
into the launch request. Mark ready only after materialization and the original
marker validate. Recover on ordinary startup through that same effect lane;
translate ambiguous ownership into a visible safe repair/refusal. Keep the
SQLite agent-session catalog unchanged.

Then upgrade the composed BPR-0621 course to require reclamation, with two
ordinary reopens, unchanged Git/SQLite identities, explicit session #2 and no
automatic agent replay. Verify a simultaneously live application/supervisor
excludes cleanup and that selected-path substitution preserves data. Supported
non-Unix lifetime/registry integration still needs implementation and native
evidence. This card and RFC-0013 remain open until those requirements pass.

Task-created journal build/test logs and the consumed insertion fragment were
removed after retaining these results. Fixture temporary roots and children
were consumed by the bounded runners. No production workspace was removed.

## Unix production integration — current slice

The installed-Git/cache configuration now selects the private versioned journal
for the Unix application. Preparation and materialization use its original
parent/child identities; the cache retains the exact owner through execution
and cleanup. The host carries the journal guard into agent launch. Ordinary
startup recovers on the existing effect lane without opening a pull request or
starting an agent, and shutdown waits for that work. Ambiguous ownership gets a
visible repair/refusal. The SQLite session catalog remains unchanged. The
stable private workspace neighbourhood is retained; completed disposable
parents and their journals are removed.

Independent review found that the initial integration still allowed Git to
outlive application death during materialization, with only the dying
application holding the journal lock. Every materialization Git subprocess now
uses the bundled supervisor and an independently opened exact shared guard.
The supervisor closes that descriptor in Git and retains it through group
teardown and child reap. Cancellation closes the owner channel and waits;
an unresponsive helper remains excluded and quarantined instead of being killed
to release its lock early. The normal Git command, stream and timeout bounds
remain in effect.

The new composed test stops the selected Git writer before init and checkout,
freezes its supervisor, kills the application, and starts ordinary recovery
twice. Both the original workspace and its journal must remain unchanged until
the supervisor resumes and stops/reaps Git. Two later ordinary startups must
reclaim the workspace with exact Git refs and SQLite session identity retained.
A separate cancellation course uses the public Cancel action while Git is
stopped and requires terminal/quiescent state, no agent launch and cleanup.

This test exposed a second defect: on this Mac, `waitid(WEXITED | WNOHANG |
WNOWAIT)` reported a stopped child (`si_code=CLD_STOPPED`, status 17). The
supervisor had treated any reported child identity as exit and killed it.
It now requires CLD_EXITED, CLD_KILLED or CLD_DUMPED. The composed course leaves
Git stopped through several helper poll intervals before freezing the helper,
so the old behavior fails before the crash checkpoint. A small local native
probe confirmed that OS observation; its temporary source and executable are
removed after use.

The running-agent crash course now also checks a simultaneous ordinary app,
a paused supervisor after owner death, same-path substitution with safe repair,
ordinary startup without PR selection, two no-replay reopens and explicit
session #2. Fixture-created wrapper and storage roots stay inside the runner's
owned temporary directory. This is local, network-isolated fixture work only.

Source `0427dc76` contains the complete Unix integration and supervisor repair.
BPR-0622 remains In Progress: non-Unix durable ownership/lifetime integration
and the remaining target-native evidence are still required.

### Local integration verification — 2026-09-05, macOS arm64

- Final `qt_agent_launch_canary`, `agent_parent_death` and
  `application_agent_materialization_crash_recovery` pass 3/3 in 142.86s
  (72.74s, 24.45s and 45.66s). The materialization course covers init and checkout
  owner death plus cancellation of stopped Git, exact live-lock exclusion,
  repeated startup reclamation, no agent launch and preserved Git/SQLite state.
  Earlier runs failed on the stopped-child event described above; only the
  repaired final course is acceptance evidence for that behavior.
- `application_agent_crash_recovery` passes in 43.41s with the integrated
  registry and Git supervisor, including paused-agent-supervisor exclusion,
  safe substitution refusal, ordinary startup reclamation, two no-replay
  reopens and explicit session #2. Its running-child course does not stop the
  supervisor's child before teardown; the later event-type repair leaves that
  passing execution path unchanged.
- The normal composition, cancellation, cleanup-retry and sanitized workspace
  diagnostic contracts pass in 34.63s, 20.68s, 19.39s and 12.93s. These were the
  four passing tests in the run whose fifth materialization test exposed the
  stopped-child defect. Their unchanged normal-child paths reuse that evidence;
  the final launcher and materialization courses directly check the repair.
- `git_process_contract` (64.65s), `agent_workspace_ownership` (2.74s),
  `application_agent_invocation_host` (0.07s), `architecture_boundaries`
  (8.37s) and `test_network_isolation_manifest` (3.16s) pass. Those inputs are
  unchanged by the supervisor's later exit-event filter and extra fixture
  cancellation course. The new fixture is registered for network isolation.
- Dev application/fixtures and the tests-off `app-release` application build.
  Built and bundled release supervisor SHA-256 both equal
  `8c9ebca76da58f47b1c9f8fcb4f56d84462a073cb3e1fd5098e2c93721997c51`;
  `otool -L` still lists only libc++ and libSystem. No release app was launched.
- Documentation gate, documentation verifier's 62 tests, changed C++ format
  checks and `git diff --check` pass. Test stdout/stderr and the native waitid
  probe were consumed before removing their exact task-created files. The
  contained fixture runners reclaimed their storage and children. No production
  workspace was removed and remembered sign-in was not changed.
- No GitHub Actions, workflow dispatch, push, paid CI, live AI fixture or remote
  mutation was used. All reported runtime results are local Mac evidence;
  they do not establish Windows or Linux acceptance.

### Remaining frontier

Implement non-Unix journal identity/locking, process-lifetime transfer and
application integration before claiming this card complete, then obtain the
required target-native evidence. Retain safe refusal for unsupported requested
guards. On the Mac, this slice now demonstrates recovery of durably owned
workspaces through preparation/cleanup journal checkpoints, actual Git
materialization and running-agent application death. Unknown historical roots
and the documented pre-identity mkdir window remain preserved repair cases,
not deletion candidates. RFC-0013 closeout remains open for its other criteria.

### Available-platform handoff

The scheduling platforms now name the remaining Windows/Linux work; the Mac
implementation and its evidence above are retained unchanged. This does not
reduce acceptance or mark the card Done. The integrator on a Windows host must
supply implementation plus native verification; a Linux host must supply the
native verification of the selected Unix path. No remote run or CI is started
by this handoff.

Read-only scouting at `2a5fc976` identified these Windows boundaries:

- `windows_agent_process.cpp` creates its job with kill-on-close, creates the
  agent suspended, assigns it, then resumes. The only job owner is the app;
  app death initiates teardown but does not retain a recovery guard through
  confirmed completion. The explicit inherited-handle-list machinery is reusable.
- The Git QProcess backend assigns the Windows job after process start. Guarded
  materialization must gain assignment and lifetime transfer before writes can
  begin, as well as the external-agent path.
- `installed_git_process.cpp` already has handle-based identity, reparse refusal,
  single-link regular-file checks and write-through non-replacing rename.
  Its existing Windows private-directory check does not prove current-user ACL
  ownership. The new registry needs that proof rather than inheriting the
  weaker check silently.
- A trusted holder outside the child job is a candidate for retaining the
  exact guard through owner death and confirmed job-empty state. Duplicating
  the job into that holder prevents last-handle kill-on-close, so explicit
  termination and verified empty-state handling are required. This is a design
  candidate, not implemented or tested Windows behavior. Confirm the actual
  native wait/notification semantics before selecting it.

The existing native launcher canary covers assignment-before-resume refusal
and descendant timeout teardown. The Unix-only parent-death, journal and
composed materialization tests identify the missing Windows test boundaries.
Keep all live scopes, unknown-artifact preservation, capacity and session
identity requirements from this card. Do not replace them with PID checks.

## Windows slice (a) integrated, 2026-09-17 (dsp-002)

`bpr-win` measured job and lock semantics before design. The measurement
table is in
[the Windows evidence file](../../verification/BPR-0622-windows-native-evidence.md).
The design mirrors the Unix supervisor: `better_pull_review_agent_supervisor.exe`
runs outside the child job, opens and locks the guard itself, and releases
the guard only after the job is confirmed empty. The integrator raised no
objection, with seven constraints; the station reports how it met each one in
the evidence file. Slice (a), guarded Windows agent launches through the
supervisor, is `e11f19eb` on `windows-lane`. It was merged into main
`--no-ff` as `9767145d`.

Two limits are recorded there, not claimed:

- killing both the owner and the supervisor leaves an unguarded window, the
  analogue of SIGKILLing the Unix supervisor;
- no run used an elevated Windows token.

Mac run of the slice's portable sources at `9767145d`
(`build/r2-logs/dsp2-int1-ctest.log`): `agent_parent_death` (26.38 s),
`qt_agent_launch_canary` (78.02 s) and `architecture_boundaries` pass.
`test_network_isolation_manifest` reports only [[BPR-0802]]'s five
`gh_cli_git_smart_transport` findings, so the slice adds none.

Still open on Windows: (b) the journal and ownership course, (c) guarded Git
through the supervisor and the materialization course, (d) app wiring and
the crash-recovery course. `windows-lane` is not fast-forwarded while the
station builds on it.

## Windows slice (b) integrated (dsp-002)

`dd88b9c9` on `windows-lane` makes the ownership journal run on Windows.
It keeps the same limits, with a handle identity, an owner/DACL privacy
check, and a one-byte `LockFileEx` range: shared for live owners, exclusive
for recovery. Evidence is in "Slice (b)" of
[the Windows evidence file](../../verification/BPR-0622-windows-native-evidence.md).
The merge into main (`a3ee3c72845016b1df7c0384a178cd0d911f67e1`) conflicted with BPR-0817's Linux change in
`tests/scripts/agent_workspace_ownership_tests.py`. The resolution keeps
both, and runs BPR-0817's mode-bit parent rule only on POSIX, because Windows
proves its untrusted-parent refusal through the DACL course.

Mac run of the slice's portable sources at `a3ee3c72845016b1df7c0384a178cd0d911f67e1`
(`build/r2-logs/dsp2-int3-ctest.log`). These pass:
`agent_workspace_ownership`, `git_process_contract`, `agent_parent_death`,
`qt_agent_launch_canary`, `application_agent_crash_recovery`,
`application_agent_materialization_crash_recovery`,
`application_agent_invocation_host`, `architecture_boundaries` and the
round-trip composition contract. The isolation manifest again reports only
[[BPR-0802]]'s five findings. Linux coverage of the POSIX branches goes with
the next `bpr-linux` envelope.

## Windows slices (c) and (d) integrated; critical review (dsp-002)

`c94e02b1` on `windows-lane` completes the Windows implementation, and main
merged it `--no-ff` as `2ba02ddb`.

- **(c)** Guarded Git runs inside the supervisor through a QProcess
  create-process modifier. The supervisor assigns Git to its job before any
  write or stdin byte. The launch specification is now v2 and carries the
  working directory.
- **(d)** The app enables the ownership registry, the persistent
  `<root>-agent-workspaces-v1` neighbourhood and startup recovery for
  `Q_OS_UNIX || Q_OS_WIN`. The parent rule now also accepts OWNER RIGHTS.

The station's native run is in "Slices (c) and (d)" of
[the Windows evidence file](../../verification/BPR-0622-windows-native-evidence.md):
both crash courses, the seven invocation courses, the canary, parent death,
ownership, the Git contracts and `architecture_boundaries` pass. Its reverted
trials show that the supervisor, the recovery path and the Windows conditions
are each load-bearing.

**Mac run at `2ba02ddb`** (`build/r2-logs/dsp2-int4-ctest.log`): these pass:

- `git_process_contract`, `git_cache_contract`, `git_anonymous_public_read`;
- `agent_workspace_ownership`, `agent_parent_death`, `qt_agent_launch_canary`;
- both crash-recovery courses;
- `application_agent_invocation_host` and the six other
  `application_agent_invocation_*` courses;
- `architecture_boundaries`.

`application_agent_invocation_composition_contract` fails on macOS exactly as
the station reported for Windows (`application_session 2`, expected 1). That
failure predates the Windows slices: [[BPR-0826]].

**Independent critical review** of `743c59d7..c94e02b1`, by source reading
plus the recorded native evidence, found **no blocking finding** across the
five named risks:

1. lifetime exclusion through confirmed job-empty;
2. destructive recovery through reparse points, hard links and substitution,
   and the OWNER RIGHTS parent rule;
3. handle leakage to the agent and Git child, and Git starting before job
   assignment;
4. strict, bounded v2 launch-spec parsing;
5. behaviour-preserving POSIX branches.

Its non-blocking hardening items (supervisor error branches, parent-rule ACE
types, descriptor coupling with the SQLite root, a junction-inside-quarantine
course) are [[BPR-0825]]. The limits it recorded stay recorded: exit
accounting versus handle rundown, the inheritable-pipe creation window, the
post-hoc bypass refusal, no elevated run, and the double-kill window.

**Remaining before this card is Done:** Linux coverage of the POSIX branches
that slices (a)–(d) changed, at a head that contains `c94e02b1`. Slices (a)
and (b) are in `task-20260917-lnx-verify`; (c) and (d) go with the next Linux
envelope. The Windows criteria are otherwise implemented and verified.

## Linux coverage of slices (c) and (d); finished (dsp-003)

`bpr-linux` ran `task-20260917-lnx-confirm` at `780d0a1c`, which contains
`c94e02b1`, and main merged it as `48b6422b`. All 15 rows pass: both POSIX
Git contracts, `agent_workspace_ownership`, `agent_parent_death`, both
crash-recovery courses, `application_agent_invocation_host` and all eight
`application_agent_invocation_*` rows ("BPR-0622, slices (c) and (d)" in
[the dsp-002 Linux confirmation](../../verification/dsp2-linux-confirmation.md)).
Slices (a) and (b) were already covered on Linux and macOS.

Every platform now has its evidence: Windows native (the Windows evidence
file, slices (a) to (d), with the critical review), macOS
(`build/r2-logs/dsp2-int4-ctest.log`, `dsp2-int7-ctest.log`), and Linux. The
hardening follow-ups are [[BPR-0825]]; they are not criteria of this card.
At main `8a03a5be` after the windows-lane merge (BPR-0825's supervisor and
ACE changes), `agent_workspace_ownership` and `git_process_contract` still
pass on macOS (`build/r2-logs/dsp3-int1-ctest.log`).

## Windows follow-up: this card's `c94e02b1` introduced BPR-0875

`c94e02b1` added `Q_OS_WIN` to the guard around the workspace recovery post in
`start_services`, and `dd88b9c9` put the ownership journal on Windows. From
then on, a Windows profile that roots its data in a directory whose parent
another principal can write gets `repair_required` from recovery at startup,
which the profile reports as a cleanup failure at the quit. That made thirteen
Windows rows red and turned an ordinary quit into exit 1 with this card's
repair presentation in every window. See [[BPR-0875]]: the product rule is
correct and unchanged, and the harness was given an owner-private parent. A
real installation was never affected, which is measured there.
