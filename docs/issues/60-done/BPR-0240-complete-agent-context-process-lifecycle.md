---
schema: "implement-rfc.card.v1"
id: "BPR-0240"
title: "Complete agent context and process lifecycle semantics"
status: "Done"
rfcs: ["RFC-BPR-0011"]
depends_on: ["BPR-0238"]
---

# Complete agent context and process lifecycle semantics

The current launch builder omits required context from child input. The process
adapter waits for completion, reads stdout once, does not drain stderr, and does
not classify nonzero exit status. Those are component gaps, not merely missing
composition.

## Required final state

- Serialize the PR URL, expected base/head OIDs, selected epochs, previous
  upload, last-reviewed head, changed/unread/accepted file state, local defect
  state, current selected file/semantic selection, and only explicitly included
  private notes or drafts. Version the supplement and keep absent fields
  distinguishable from empty values.
- Carry exact host/repository/PR/base/head/epoch/session identity and the
  permitted semantic segments, groups, mapping confidence, and selected source
  text without presentation coordinates or live scroll state.
- Implement and test Codex App Server, Codex JSONL, Claude stream-JSON, and
  external-terminal protocol behavior rather than treating one canary framing
  as proof for all profiles.
- Stream stdout incrementally, drain stderr without deadlock, and preserve
  structured protocol framing across partial reads.
- Distinguish failed-to-start, nonzero exit, protocol failure, cancellation,
  timeout, stale result, and successful completion as typed outcomes.
- Prove bounded output/backpressure and teardown without leaking credentials or
  placing context in argv.
- A canary asserts all required context fields, stderr pressure, partial
  frames, nonzero exit, cancellation, and stale-result rejection.

## Session handoff — 2026-08-11

The operator ended the current implementation session and requested that the
in-flight Windows process work be preserved rather than discarded. The saved
slice adds a private WIN32 backend, launcher/CMake integration, canary support,
and architecture hostile checks for explicit UTF-16 argv/CWD/environment,
allowlisted handles, suspended process creation, pre-resume Job Object
assignment, incremental stdout/stderr draining, cancellation, reaping, and
descendant containment.

This card remains in progress. No compile or target-native Windows run was
completed after these edits, and no Linux/macOS regression matrix, format/tidy,
documentation gate, or independent review was run. The next session must first
build the affected agent/Qt canary targets on a portable host, run the
architecture boundary pair, then obtain the required target-native Windows
compile/runtime evidence. None of this handoff closes BPR-0240, BPR-0245, or
RFC-BPR-0011.

## macOS formatting checkpoint — 2026-08-11

The preserved slice's three format-gate findings were repaired with the
repository's configured `clang-format`, changing whitespace only in
`agent_process_launcher.cpp`, `windows_agent_process.cpp`, and
`agent_launch_canary_tests.cpp`. Direct `--dry-run --Werror` verification now
passes for all three files. On macOS, the adapter and Qt canary targets rebuilt,
and the exact `architecture_boundaries`, `architecture_boundary_contract`, and
`qt_agent_launch_canary` selector passed 3/3 in 128.33 seconds.

This checkpoint is not target-native Windows evidence: the private WIN32
backend remains excluded by the macOS preprocessor and has not been compiled or
executed on Windows. The card therefore remains in progress with its Windows
compile/runtime matrix, complete portable regression matrix, strict tidy,
documentation gate, and independent review still required for terminal
closure.

## macOS portable lifecycle checkpoint — 2026-08-12

The affected agent launch, invocation-host, Qt launch-panel, and Qt canary
targets rebuilt on macOS. The exact `application_agent_launch`,
`application_agent_invocation_host`, `qt_agent_launch_panel`,
`qt_agent_launch_canary`, `application_agent_invocation_composition_contract`,
`architecture_boundaries`, and `architecture_boundary_contract` selector then
passed 7/7 in 157.95 seconds under the repository network-isolation wrapper.

The repository format check passed for `agent_process_launcher.cpp`,
`windows_agent_process.cpp`, and `agent_launch_canary_tests.cpp`. Strict scoped
clang-tidy passed the two macOS compilation-database translation units,
`agent_process_launcher.cpp` and `agent_launch_canary_tests.cpp`; the WIN32-only
`windows_agent_process.cpp` translation unit was reported absent from that
database and remains target-native Windows work.

This checkpoint closes the selected portable macOS evidence gap only. The card
remains in progress pending the target-native Windows compile/runtime matrix,
the remaining portable regression and closeout gates, and independent terminal
review.

## Portable gates and independent review — 2026-08-17

[[BPR-0319]] deferred this card's lane classification until the portable gates
it owed had actually run here. They have now run, and the answer is that the
card is **not** platform-blocked. It stays in progress.

The complete portable `dev` matrix ran at `be42eb7`: 184 of 186 passed. Both
failures were environmental and each passes on its own.
`application_native_notification_attended_contract` failed on
`sh: /bin/ps: Operation not permitted` — a sandbox restriction on the runner,
not the product — and passes with `/bin/ps` reachable.
`application_epoch_maintenance_public_path` failed under contention while other
work shared the machine, and passes alone in 53.56 s. Neither touches this
card's surface. A clean uncontended replay is still the RFC-closeout gate; this
is checkpoint evidence, not closeout evidence.

That run also supplies evidence this card had never recorded. `8886017` rewrote
`agent_context_supplement.cpp`, `agent_profile.cpp`, and both of their test
files, but neither earlier checkpoint ran the core tier it changed. In this
matrix `core_agent_profile` (#52) and `core_agent_context_supplement` (#54)
both passed.

Strict `coding-tidy` passed the whole first-party tree at `4fc9a78` under
`--warnings-as-errors=*`, with zero diagnostics across 884 lines of invocation
log. The one translation unit it cannot reach is still
`windows_agent_process.cpp`, absent from the macOS compilation database, which
is target-native work and correctly deferred.

The independent review returned **PORTABLE-WORK-REMAINS** and it is right. Four
findings, each reproduced directly rather than taken on the reviewer's word:

1. **The Windows pre-start refusal is gone and six documents still assert it.**
   `c6f3d96` replaced the `failed_to_start` guard in
   `agent_process_launcher.cpp:129` with a dispatch into the never-compiled
   WIN32 backend. `RFC-BPR-0011.md:13,109,115,216` and
   `ai-agent-boundary.md:140,335` all still say the adapter refuses before
   `QProcess::start`, and line 109 is a claim-matrix **Result** cell. A seventh
   row, `ai-agent-boundary.md:314`, cites a Windows-conditional canary as proof
   of refusal whose final assertion is now `Outcome == success`. Tracked as
   [[BPR-0332]], which also carries the choice between restoring the guard and
   documenting the real state.
2. **Two of three output ceilings are unproven.** `output_too_large` and
   `frame_too_large` appear four times in `src/` and zero times in `tests/`.
   The 256 KiB stderr canary proves draining without deadlock, not the 1 MiB
   bound. Tracked as [[BPR-0333]].
3. **Core-tier evidence was missing.** Closed by this matrix run, above.
4. **~250 lines of portable protocol logic were duplicated into the
   Windows-only translation unit. Closed by [[BPR-0334]].** One private,
   fake-tested loop now owns decoding, availability-batch causal gating,
   follow-up writes, publish/backpressure, stderr accounting, first-cause
   terminal classification, and exactly-once teardown. POSIX and Windows keep
   only their native process transports. This removes uncompiled policy from
   the Windows session; it does not replace that session's required native
   compile, Job Object, cancellation, and descendant-containment evidence.

The review also corrected an overclaim on this card. The required final state
says "A canary asserts all required context fields"; the canary checks eight
substrings and never checks `base`, `head`, either epoch, `host`, `repository`,
or any segment or group field. The obligation is met — but by
`application_agent_invocation_composition_contract`, whose whole-map equality
covers them exactly. This card credited the wrong artifact.

What is genuinely Windows-bound is unchanged and remains correctly deferred:
`windows_agent_process.cpp` has never been compiled, strict clang-tidy cannot
reach it because it is absent from the macOS compilation database, and Job
Object pre-resume assignment and descendant containment need real Windows. It is
also the only `Q_OS_WIN`-gated source file, so the file-level claim in the
earlier checkpoints was accurate. What was not accurate was treating everything
that remained as Windows work.

## `windows_agent_process.cpp` has now been compiled — 2026-08-22

The Windows session's first successful `windows-dev` build ([[BPR-0412]],
after [[BPR-0428]] and [[BPR-0429]] cleared the syntax provider) compiled
`src/adapters/agent/src/windows_agent_process.cpp` under MSVC 14.51 with zero
diagnostics, as part of the full Qt graph. "Never been compiled by any
toolchain this project has had" is no longer true; what remains Windows-bound
on this card — Job Object pre-resume assignment, descendant containment, and
the target-native lifecycle proof — is unchanged and still owed here. This
note records the fact; it takes nothing else.

## The first target-native run of the canary — 2026-08-23

With the backend compiled, the Qt graph ran it. `qt_agent_launch_canary`
fails on the Windows host, contained and uncontained alike, at

```
FAILED: each selected protocol reaches a typed successful terminal
```

in 20 s. That is this card's Windows half speaking for the first time: the
protocol loop [[BPR-0334]] made portable does not reach a successful
terminal through `windows_agent_process.cpp`. It is recorded here rather
than on [[BPR-0412]], whose matrix it appears in, because the cause is the
backend's and the card that owns the backend is this one. Not yet
diagnosed.

## Diagnosed — 2026-08-26

An instrumented single-protocol run over a scout's candidate map settled
it, and the backend is innocent. Every Windows transport primitive
behaved exactly on every call: three round-trip `write_all`s completed
inside a millisecond, 256 KiB of stderr pressure drained in 65 ms, and
`wait` returned `running` on schedule until the deadline. What the trace
showed instead is the fixture: `write_partial` in
`agent_canary_main.cpp` sleeps one millisecond per three-byte chunk, the
default Windows timer tick stretches that to ~15.6 ms, and the measured
throughput was 723 bytes in four seconds — about 180 bytes a second
against an app-server exchange of roughly 15 KB. The child was alive and
advancing at every deadline; the timeout was arithmetic, not behavior.
Two prior suspects died on the same trace: no framing desync (the
reconstructed stream is byte-exact, and the `_O_BINARY` repair holds),
and no CRLF cause.

Two fixture repairs landed on the way: binary standard streams,
descriptor-guarded and best-effort because some invocation courses
attach no stream (`0a27c282`), and `timeBeginPeriod(1)` so millisecond
pacing means milliseconds on Windows as it already does on the platforms
the fixture passes on (`b33f8a2c`, `ddcde8da`).

Verification is behind the host policy: the relinked fixture's first
rerun failed `failed_to_start` across the family — the reputation wave,
per [[BPR-0036]]'s procedure — and the retry waits for the verdict.
What this card still owes if that rerun converts: Job Object pre-resume
and descendant-containment evidence, and [[BPR-0332]]/[[BPR-0333]]'s
recorded findings. The composed-app course also printed
`agent-authority-and-offerings … profiles=0` where four are expected —
possibly downstream of the same refused launches, to be re-read after
the verdict lands rather than diagnosed under it.

## The canary passes target-native — 2026-08-26, late evening

The verdict landed on the second cycle and the pacing repair held:
**`qt_agent_launch_canary` passed in 107.21 s** — every selected
protocol reaching a typed successful terminal through
`windows_agent_process.cpp` for the first time on any Windows run — and
`application_agent_invocation_composition_contract` (20.57 s) and
`application_agent_invocation_staleness_contract` (16.72 s) passed
through the composed application with it, the composition contract's
first target-native pass.

Three courses remain red with distinct, now-measurable shapes on a
healthy canary:

- cancellation: `running-course failed=status-is-running
  status=Finished: nonzero_exit output-bytes=0` — the child is gone,
  silently and nonzero, where the course needs it still running;
- cleanup-retry: fails at its `cleanup-retry-substitute` step;
- workspace-diagnostic: `EPOCH_CACHE_FAILURE:10`,
  `EPOCH_EFFECT_FAILED:revision-prepare:6`, and
  `agent-authority-and-offerings … profiles=0` where four profiles are
  expected — no longer attributable to refused launches, since sibling
  courses now launch the same images successfully.

These are this card's remaining Windows diagnosis, alongside the Job
Object and descendant-containment evidence it has always owed.

## Two of the three courses are answered — 2026-08-26, night

The scout traced all three to test-side Windows defects, and the fixes
for two are proven at `c868536d` after the relink verdict wave:

- **cancellation passes (16.44 s)**: the driver's mode-suffixed canary
  copies dropped the source's `.exe`, and the canary's CRT re-spawn of
  itself appends `.exe` to an extensionless name, so the descendant
  spawn failed silently (`return 31`) — the copies keep the extension
  now. The canary (106.78 s), composition (22.26 s), and staleness
  (16.64 s) contracts stay green beside it.
- **cleanup-retry** is [[BPR-0465]]: its live-workspace rename is
  inexpressible over a Windows cwd handle; the staging needs designing,
  not patching.
- **workspace-diagnostic** survived its first candidate. The wrapper's
  `_execv` was real and is repaired (spawn-and-wait with quoting), but
  the failure shape is identical before and after, and the wrapper is
  directly proven healthy on this host — `--exec-path`, `version`, and
  `rev-parse` all forward correctly even under a scrubbed environment
  whose `PATH` is only its own directory. The fault therefore sits in
  the adapter's validation or launch of a *substituted* `GitExecutable`
  on Windows — the only configuration this course alone exercises — and
  the `EPOCH_CACHE_FAILURE:10:` line's empty detail is the fingerprint
  under trace.

The trace answered it the same night, and it is the canary's defect a
third time — though not, it turned out, the last one. The wrapper's `--exec-path` reply ends `0d 0a` under the
CRT's default text mode — measured by direct hexdump — and the
adapter's handshake strips exactly one trailing byte, so the dangling
carriage return fails the earliest wire check
(`installed_git_process.cpp`'s bootstrap probe) with precisely the
bare-detail `process_failure` the course prints. Binary standard
streams landed at `3a34cdc4`. What the fix does not yet have is its
passing run: the restaged wrapper image is refused by the host policy
past three and a half hours — beyond every conversion this host showed
today — so the row waits under the owner's recorded no-signing
deferral, and converts on this image's verdict or on the next relink
whose verdict lands, the same disposition the behavior contract took
before today's rebuild converted it. The trace also recorded one latent
non-Windows-specific gap left unrepaired: the wrapper's reply is
ANSI-encoded (`path::string()`) while the adapter compares UTF-8
(`u8string()`), inert for ASCII paths like this host's.

## The fourth fixture defect, and the course's first green — 2026-08-27

The wrapper's verdict landed overnight, its direct probe printed a clean
`\n`-terminated reply — the CRLF repair holds on the wire — and the
Code Integrity log showed no refusal during the fresh rerun. The course
still failed identically, so the CRLF diagnosis was, like the two before
it, necessary and insufficient.

An instrumented clone of the wrapper, substituted through the course's
own argv and logging every invocation, captured the real cause without
touching a registered image. Every cache-open command succeeds through
the wrapper — `init --bare`, `index-pack --stdin`, `cat-file`,
`rev-parse`, `merge-base`, all status 0 — until `fsck
--connectivity-only`, where **real git spawns its own subcommands** —
`git refs verify`, `git commit-graph verify`, `git multi-pack-index
verify` — **with a bare `git` argv[0]** resolved through
`GIT_EXEC_PATH`, which points at the staged wrapper. Those grandchildren
re-enter the wrapper, whose first act was
`std::filesystem::canonical(argv[0])`; a bare relative name cannot
canonicalize, so each child exited 126 before forwarding, fsck returned
56, and the cache open surfaced exactly the bare-detail
`process_failure` this card has chased since the first target-native
run. POSIX never sees it because its children arrive with a resolvable
argv[0].

`ac38d3c6` names the wrapper by `GetModuleFileNameW` instead of argv[0],
and **`application_agent_invocation_workspace_diagnostic_contract`
passed in 6.14 s — its first green on any Windows run.** The
`agent-authority-and-offerings profiles=0` line disappeared with it: the
epoch failure it trailed was upstream of the panel's offerings, not a
separate defect.

The Job Object pre-resume and descendant-containment evidence this card
has always owed is also now answered on the current tree. Matrix-10
(2026-08-27, `8e952928`, recorded on [[BPR-0036]]) ran
`qt_agent_launch_canary` green in 106.66 s — which on Windows includes
`test_windows_assignment_failure_has_no_pre_assignment_side_effect`,
proving a child whose job assignment fails never runs its first
statement, and `test_timeout_terminates_the_descendant_process_tree`,
proving a spawned grandchild's heartbeat stops on teardown — and
`application_agent_invocation_cancellation_contract` green in 16.41 s,
the same descendant proof under explicit cancellation. The success-path
ordering the tests could not see — assignment before resume — is pinned
by a source-order assertion in the architecture suite at `88a8c063`,
verified failing-if-reordered by construction and passing 2/2 focused.

Remaining on this card: the five agent courses' rerun once the relinked
canary image's reputation verdict lands (the workspace-diagnostic row
above already converted; a retry loop is armed), and [[BPR-0332]]'s
recorded documentation finding, which has its own card.

## Every course is green — 2026-08-27, afternoon

The canary verdict landed and the rerun converted all five:
`qt_agent_launch_canary` 107.54 s,
`application_agent_invocation_composition_contract` 20.52 s,
`application_agent_invocation_cancellation_contract` 16.53 s,
`application_agent_invocation_staleness_contract` 16.64 s, and
[[BPR-0465]]'s `application_agent_invocation_cleanup_retry_contract`
16.57 s (`build/logs/retry-long-1.log`). With the workspace-diagnostic
green earlier today, **every agent course and the canary are green
target-native on this host for the first time**.

Nothing on this card is left to run. [[BPR-0332]]'s documentation
reconciliation is its own card's scope. Like [[BPR-0036]] and
[[BPR-0053]], this card stays open deliberately: [[BPR-0245]]'s closeout
matrix cites it in six rows as the anchored owning procedure, and the
documentation gate requires an open closeout card's cited procedures to
stay open until the final replay consumes them.

## The ledger absorbs the conversion — 2026-08-28

The 2026-08-26/27 target-native record above had never left this card. The
RFC-BPR-0011 verification ledger, the agent boundary document, and the
fixture catalog still said the first Windows canary fails before a typed
terminal and that Job Object lifecycle and descendant containment were
unproved. This slice reconciles those lifecycle-proof statements — the
ledger status, both claim-matrix Windows cells, the three marked banners,
the catalog's Windows sentence, and the two stale closure-ordering
sentences left behind by RFC-BPR-0002/0003's 2026-08-28 closes — against
the recorded runs, citing this card's dated sections. [[BPR-0332]]'s scope
is untouched: every pre-start-refusal statement stays marked known-false
rather than resolved, and the guard-versus-document decision stays the
product owner's.

## Lane — 2026-08-28

Moved from `30-in-progress/` to `10-backlog/`, and the 2026-08-26 trace
note's latent gap is carded so it survives this card going dormant: the
wrapper replying `path::string()` while the adapter compares `u8string()`,
inert for ASCII paths, is now [[BPR-0476]] in `21-ready-win/`. Nothing here
is runnable: the remaining event in this card's life is [[BPR-0245]]'s final
closeout replay consuming its six cited procedure rows, which is the closeout
card's work — the lane that says "waiting on another card". [[BPR-0036]] and
[[BPR-0053]] waited out the same rule in the same lane before their RFCs'
closeouts consumed them on 2026-08-28.

## Closed — 2026-09-01

The only reason this card stayed open was a documentation-gate rule that no
longer exists: `scripts/verify_documentation.py` now checks the light
closeout contract (RFC base, one row per criterion) and no longer requires a
closeout card's cited procedures to stay open until replay. Every required
final state above has direct evidence — the version-two supplement and
four-protocol lifecycle on the portable surface, and the Job Object,
descendant-containment, and five-course conversion target-native on Windows
(2026-08-26/27). The 2026-09-01 macOS focused replay recorded on [[BPR-0245]]
reran the whole agent selector set at 16/16. [[BPR-0332]] keeps the
documentation decision; [[BPR-0245]] keeps the closeout.
