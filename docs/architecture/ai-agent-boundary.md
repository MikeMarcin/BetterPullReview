# AI Agent Boundary

## Architectural allocation

- The core owns explicit-invocation legality, Git-host/provider policy, selected context scope, revision binding, staleness, and finding-promotion decisions.
- The host owns executable/workspace/process/session/protocol lifecycle, environment policy, context-supplement construction, cancellation, and typed result events.
- The Qt adapter owns visible provider/scope disclosure, explicit commands, approvals, streaming chat/findings, and error presentation.

## Principle

Codex and the approved Claude/Bedrock harness are separate trusted applications. They use their own authentication, GitHub access, sandbox, tools, and approval policies exactly as they would when launched directly. BetterPullReview does not proxy model traffic or provide agent credentials.

## Provider profiles

A profile declares executable, protocol, supported capabilities, workspace policy, environment allowlist, host policy, and persistence behavior.

The shipped immutable registry contains Codex App Server and Codex JSONL
(`codex`), approved Claude stream JSON (`claude`), and the external terminal
harness (`bpr-agent-terminal`). Merely showing the panel performs no executable
discovery. A missing executable becomes `failed_to_start` only after an explicit
start gesture. GitHub.com permits all four profiles; an explicit GHES host
refuses Codex before workspace or launcher effects and offers only the approved
corporate alternatives.

Initial protocols:

```text
codex_app_server
codex_jsonl_exec
claude_stream_json
external_terminal
```

The external-terminal fallback is mandatory when a corporate harness exposes no supported machine-readable interface.

## Host policy

Git hosts map to allowed or required agent profiles. A corporate GHES profile can require the corporate Claude harness and forbid a personal provider. Unknown hosts have AI disabled until explicitly mapped.

## Workspaces

- Existing checkout: deliberate user selection for repository-wide context.
- Disposable review workspace: default for bounded AI review, pinned to the exact review head and isolated from the active worktree.

The disposable destination is owner-private and outside the retained cache.
The cache actor initializes a new repository, transfers the exact requested head
through Git pack plumbing, checks it out detached, and validates that no
remote, alternate, hook, or linked-worktree topology crossed. It creates no
cache ref and no hardlink. Cleanup requires the marker and the opaque directory
identity captured at creation; a same-path, same-marker substitute is not
authorized. Cleanup runs only after terminal delivery, and cancellation,
staleness, and orderly shutdown use the same bounded path. The child working
directory is assigned only after materialization. Existing-checkout mode
remains a per-invocation choice and discloses dirty or unknown state.

## Context supplement

The agent retrieves PR data through its own GitHub tools. BetterPullReview's
version-two supplement supplies only the approved local state that GitHub does
not represent:

- exact session, profile, host, repository, PR URL and ID, base/head OIDs, and
  selected epoch pair;
- optional previous-upload and last-reviewed heads, with presence encoded
  separately from value;
- each included file's file/material identities, path, read state,
  changed-since-review flag, and accepted/excluded state;
- complete local defect identity, conversation, immutable original semantic
  anchor, classification, lifecycle, and anchor availability;
- the selected file and comparison, semantic line/group selection, mapping
  confidence, permitted segments/groups, and optional selected source text;
- only private notes named by the invocation and drafts supplied explicitly.

The launch panel offers opaque private-note IDs and the current draft's opaque
ID/revision, all unchecked. Its intent carries only the checked identities; it
never carries prose. At dispatch the application reloads the current ledger
observation and editor draft, and exact note identity plus draft identity and
revision must still match. A removed note, edited draft, duplicate choice, or
over-limit selection refuses before session persistence. Unchecked prose has no
route into the supplement.

Counts and optional-presence fields are explicit, so absence is not rewritten
as an empty string or collection. Core construction refuses invalid identities,
inconsistent anchors, invalid attachments, and fixed count/byte bounds whole.
The application adapts a caller-supplied BPR-0238 private observation; it owns
no ledger and performs no implicit load. Presentation coordinates, Qt indexes,
scroll state, process arguments, environment values, and credentials have no
field in the supplement.

## Protocol and process lifecycle

The four profiles have separate wire contracts; selecting a protocol is not a
label over one common canary format.

- Codex App Server uses newline-delimited App Server messages. The host sends
  `initialize`, waits for its response, sends `initialized` and `thread/start`,
  waits for the returned thread ID, and only then sends the supplement as the
  text input of `turn/start`. It consumes the response to that id-2 request,
  correlates output by thread and turn, and succeeds only after a
  `turn/completed` notification whose turn status is `completed`. `failed` and
  `interrupted` remain distinct typed protocol failures.
- Codex JSONL execution starts `exec --json -`. The supplement is the stdin
  prompt and stdout agent-message items use the JSONL event shape. A
  `thread.started`/`turn.started` lifecycle must reach `turn.completed`.
- Claude starts print mode with stream-JSON input and output plus verbose event
  delivery. The supplement is a stream-JSON user text block. Output must begin
  with `system`/`init`, assistant text is decoded from its content blocks, and
  a same-session `result`/`success` with `is_error:false` is the terminal.
- External-terminal fallback receives a versioned plain stdin envelope and
  emits newline-delimited text.

Structured stdout is decoded as JSON rather than classified by substrings. The
bounded parser consumes the complete value, rejects duplicate keys and invalid
syntax, decodes all JSON string escapes including Unicode surrogate pairs, and
validates unescaped UTF-8 as Unicode scalar values. Lone continuations,
overlong encodings, UTF-8 encodings of surrogates, out-of-range scalars, and
truncated sequences are refused.
Protocol state then validates the documented discriminators, required semantic
fields, correlation identity, ordering, and terminal. Structured stdout is
decoded incrementally across arbitrary partial reads.
App Server parsing stops at each host-followup boundary. The next response
phase is unlocked only after the launcher has written the complete
`initialized`/`thread/start` or `turn/start` followup. A child that preloads a
later response, delta, or completion before that write is a causal protocol
failure, even if the preloaded frames would otherwise form a valid transcript.
Individual frames are limited to 64 KiB, total stdout and retained stderr to
1 MiB each. The synchronous output sink completes before the launcher reads
more child output, which makes consumer backpressure part of the contract;
stderr is drained on every polling pass so a full error pipe cannot deadlock
stdout. Provisional events carry the exact session ID and an ordered sequence.

The portable protocol lifecycle lives once in
`src/adapters/agent/src/agent_protocol_loop.cpp`. POSIX and Windows transports
own only their process primitives and present bounded stdout/stderr reads,
wait, input-close, and termination to that loop. A transport marks whether a
bounded read completes the availability snapshot it observed; the loop defers
decoded events and follow-up writes until that snapshot is complete, so an
already-buffered later App Server response cannot cross a 16 KiB read boundary
and escape causal validation. A natural process exit is drained, decoding is
finished, and its result is classified; write, cancellation, timeout, and
protocol failures retain their already-recorded cause without forcing a final
decoder pass. Every path attempts teardown exactly once. Teardown failure
replaces an otherwise successful result but cannot overwrite nonzero exit,
protocol, cancellation, or timeout.

The terminal result distinguishes failed start, nonzero exit, protocol
failure, explicit cancellation, timeout, stale result, and success. Failed,
cancelled, timed-out, and stale results expose no final event collection.
Cancellation and deadlines are observed in bounded polling slices on the
QProcess-owning thread. POSIX children start in their own process group, so
teardown targets the descendant tree and not only the immediate process.
`QProcess` cannot assign a Windows Job Object before the child begins running;
post-start assignment leaves a descendant-escape race. The adapter therefore
refuses Windows launch before `QProcess::start`. Windows support remains blocked
until a suspended-create, assign-before-resume path has target-native proof.

> **Known false at HEAD.** The adapter stopped refusing at `c6f3d96`, which
> replaced that branch with a dispatch into `windows_agent_process.cpp` — a
> translation unit no toolchain has yet compiled. The conclusion still holds
> (no Windows launch is claimed and none has been proved), but the stated
> mechanism is not the shipped one. BPR-0332 owns the resolution and BPR-0240
> owns the compile. Marked by BPR-0401.

The application composition follows one public route: the workbench panel emits
an intent; `agent_invocation_host` reruns core legality and validates the full
current context, durably admits an immutable session, then materializes the
workspace through the retained cache actor, starts the adapter, serializes
stream/terminal projection, and cleans the owned workspace. A core/context
refusal performs no session write. A session-store refusal is typed and visible
before workspace, environment, executable, or process work. Workspace and
parent-root cleanup each require their originally captured directory identity;
the parent is removed non-recursively only when that exact directory is empty.
If terminal cleanup refuses a changed workspace identity, the host retains the
original capability and shutdown polling may request one de-duplicated retry
on the effects actor after terminal completion; polling never performs Git or
filesystem work itself.
Construction- and gesture-cut contracts prove the public canary cannot be
satisfied by a fixture-only launch helper.

## Session semantics

- Every invocation is an explicit user action.
- Sessions are pinned to immutable OIDs.
- Head movement marks results stale; it does not silently retarget the session.
- BetterPullReview can record what supplemental context it sent, but it cannot claim to audit every item the full agent independently accessed.
- Structured AI output is a private proposal until a user explicitly promotes or publishes it.
- A full agent may use its own GitHub tools subject to its normal approvals; BetterPullReview learns resulting shared changes through normal synchronization.

Each exact host/account/repository/pull-request scope has a durable monotonic
session sequence and a bounded catalog of the newest 128 immutable metadata
records. Pruning never rewinds the separate high-water sequence, so an ID is
not reused after restart; exhaustion at signed 64-bit maximum refuses. Records
contain only session/profile, review identity, base/head OIDs, and epoch pair —
never context prose, output, results, credentials, argv, environment, or
workspace paths. The panel presents the latest retained binding and the core's
`current`, `head_moved`, `epoch_pair_changed`, or `different_review`
classification; observing a new review never retargets the stored binding.

## What implementation settled

The sections above were written before any of this existed. They state the
allocation; what follows is what building it decided, and what each claim can
be checked against.

### The credential rule is a shape, not a promise

`core::agent_profile` has no field a credential could occupy — no token, no
key, no generic string map. `AllowedEnvironment` is a list of variable
**names**; a map would give every name a value, and a value is where a secret
goes. Names are not automatically harmless, however: selecting `GITHUB_TOKEN`
would select its value indirectly. Core therefore accepts only a conservative
portable name syntax and rejects credential-bearing name classes and aliases
case-insensitively (`TOKEN`, `SECRET`, `PASSWORD`, authentication, API/access/
private keys, credentials, and PAT). It also rejects Git's indirect credential
and configuration injection channels, including askpass/SSH command variables,
every `GIT_CONFIG*` entry, Git proxy/exec overrides, and Git Credential Manager
(`GCM_*`) variables. Case-insensitive duplicates are invalid.

`core_agent_profile` asserts this by reading the type: each field by name, and
then the struct's total size against the sum of those fields. Adding a `Token`
member fails to compile with a message asking why it cannot hold a credential.
A test that built a profile and searched it for a secret would pass for every
profile nobody thought to put one in.

The same guard, as a structured binding over every field, covers
`core::agent_context_supplement` in `core_agent_context_supplement`. The
binding form is the better one: a `sizeof` sum has to be hand-computed with
padding, fails for the wrong reason on a layout change, and passes for a field
swapped with one of equal size.

### The environment is an allowlist because of how it is built

`application::materialize_agent_environment` first revalidates the complete
profile, then iterates the **allowlist** and asks the parent for each name. It
never enumerates the parent, so a variable the profile did not name cannot be
reached and therefore cannot be copied. An invalid name refuses the profile
whole; application materialization does not salvage a safe-looking subset.

A function that started from the parent's environment and removed
known-sensitive names would be a denylist wearing an allowlist's name, and
would pass every secret nobody had thought to list.

Checked twice. `application_agent_launch` compares the request's environment
against the whole allowlist, name for name. `qt_agent_launch_canary` launches
a real child that reports its own environment, and compares **that** against
the allowlist — because the request can carry the boundary perfectly and the
call can lose it: `QProcess` hands a child a copy of the parent's environment
unless told otherwise, and `systemEnvironment()` is the convenient call. One
word changes it, and only the canary notices.

Both tests plant three secrets of different shapes and assert the parent holds
each at the moment the child runs. Grepping for one name would pass against a
child that inherited another.

### Nothing sensitive travels in argv, and the reason

Command lines are world-readable: `ps` on POSIX, the process list on Windows. A
context supplement naming an unreleased feature, a customer, or a private path
would be visible to every other user of a shared machine.

`agent_launch_request` separates `Arguments` from its initial and deferred
input channels, and `build_agent_launch` has no branch that reads the
supplement into the former. `Arguments` carries only the protocol's fixed
switches, asserted argument for argument rather than searched for one known
note — in the request, and again from the child itself.

### Private notes travel only where selected

`core::build_agent_supplement` copies `Notes` from `SelectedNotes` and from
nowhere else. It holds no note store, so a request that selected none produces
a supplement with none **because there was no source**, rather than because a
filter removed them.

A redaction step would be worse in the way that matters: it is a place a
mistake can live, and the mistake is silent. The supplement looks right to
whoever wrote the code and wrong only to the reviewer whose unfinished thinking
is now in a tool they meant to ask about a diff.

### Process ownership is enumerated

`QProcess` and `QProcessEnvironment` have an ownership allowlist in
`cmake/VerifyArchitectureBoundaries.cmake`. It named two files; RFC-BPR-0011
adds `src/adapters/agent/src/agent_process_launcher.cpp` as the third, and the
scan checks that the adapter's own CMake target owns it.

That target also owns the private portable protocol loop. The POSIX translation
unit wraps `QProcess`; the Windows translation unit retains `HANDLE`, Job
Object, and cancellation primitives. Architecture checks require both backends
to select the one shared loop and forbid them from regaining decoder,
follow-up, stderr-accounting, or protocol-phase policy.

The launcher is **not** in `src/adapters/qt`. That whole directory is an
effect-free region, because presentation starts no processes — and the scan
refused the first attempt to put it there. `qt::agent_launch_panel` holds no
process and no launcher, so a refresh, a navigation, or new activity arriving
cannot reach one.

### A session answers about what it was asked

`core::observe_agent_session` compares and reports; nothing rewrites a
binding. Head movement makes a session stale and never retargets it, and
`continue_against_binding` returns the session's own binding unconditionally —
a named function rather than a field access, so continuing against an old
revision is a call a reader can find.

The epoch pair is part of the identity. An agent asked "what changed since the
last review" and one asked "what changed in the whole pull request" were asked
different questions even where the head is identical.

### The dangerous workspace mode needs a choice, not a setting

`application::prepare_agent_workspace` refuses `existing_checkout` without an
explicit choice for **this invocation**. A profile cannot opt itself into the
reviewer's uncommitted work. `checkout_cleanliness` has three states because
"not asked" is not "clean": a host that could not determine the state must say
so rather than pick the reassuring answer.

### Where to check each claim

| Claim | Test |
|---|---|
| No profile type can carry a credential | `core_agent_profile` |
| Credential-bearing environment names and case aliases are invalid | `core_agent_profile`, `application_agent_launch` |
| An unmapped host permits nothing | `core_agent_profile` |
| A required profile excludes the merely allowed | `core_agent_profile` |
| Head movement makes a session stale, never retargets | `core_agent_session` |
| The epoch pair is part of session identity | `core_agent_session` |
| No supplement field can carry a credential or an unselected note | `core_agent_context_supplement` |
| An oversize supplement is refused whole | `core_agent_context_supplement` |
| BPR-0238 observations contribute only explicitly selected private prose | `application_agent_launch` |
| Core/context refusal writes no session; store refusal starts no downstream effect | `application_agent_invocation_host` |
| Durable session IDs remain monotonic across restart and concurrent openers | `sqlite_review_ledger_storage` |
| Opaque note/draft choices are unchecked and revalidated at dispatch | `qt_agent_launch_panel`, `application_agent_launch` |
| A valid request's environment is exactly its safe allowlist | `application_agent_launch` |
| Four protocol lifecycles preserve partial reads and require their terminal | `application_agent_launch` |
| The supplement never reaches argv | `application_agent_launch`, `qt_agent_launch_canary` |
| A real child receives only the allowlist | `qt_agent_launch_canary` |
| Partial stdout and pressured stderr make progress together | `qt_agent_launch_canary` |
| Every terminal condition is distinct and failed runs publish nothing | `qt_agent_launch_canary` |
| POSIX timeout terminates a live descendant heartbeat | `qt_agent_launch_canary` |
| Windows launch is refused before start pending safe containment — **known false at HEAD, and the cited test does not support it: `test_windows_assignment_failure_has_no_pre_assignment_side_effect` ends on `Outcome == success`. See BPR-0332.** | `qt_agent_launch_canary` (Windows-conditional) |
| Existing-checkout mode needs a choice | `application_agent_workspace` |
| Dirty state is reported, unknown stays unknown | `application_agent_workspace` |
| The launch surface only emits intents | `qt_agent_launch_panel` |
| Scope is disclosed before launch is possible | `qt_agent_launch_panel` |
| GHES policy refuses before effects | `application_agent_invocation_host` |
| Stale/cancelled composed runs leak no child or owned workspace | `application_agent_invocation_host`, `application_agent_invocation_cancellation_contract`, `application_agent_invocation_staleness_contract` |
| A same-path, same-marker substitute cannot retarget cleanup | `git_process_contract` |
| Two fresh processes prove selected stdin, durable reopen, and next ID | `application_agent_invocation_composition_contract` |
| Direct construction or gesture bypass cannot satisfy the public canary | `application_agent_invocation_construction_cut_contract`, `application_agent_invocation_gesture_cut_contract` |
| Protocol policy exists once, preserves complete availability-batch causality, and terminates once with the first cause intact | `application_agent_protocol_loop`, `architecture_boundaries`, `architecture_boundary_contract` |
| Process ownership is enumerated | `architecture_boundaries` |

### What is not claimed

BetterPullReview records what supplemental context it sent. It **cannot** claim
to audit every item the full agent independently accessed — the agent has its
own GitHub access and its own tools, which is the point of the arrangement.

The process and shipped composition contracts use the hostile local canary, not
an installed agent tool or live provider. A smoke test against an installed
Codex or Claude remains optional extra validation rather than a gate. Windows
still refuses before child start; no Windows process-lifecycle success or
descendant-containment claim is made. **The first clause is known false at
HEAD** — `c6f3d96` replaced the refusal with a dispatch into an uncompiled
backend — while the second still holds. BPR-0332 owns the resolution. Marked by
BPR-0401.
