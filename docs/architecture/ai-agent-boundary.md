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
harness (`bpr-agent-terminal`). The Agent tab and Send to Agent name them by
plain name: Claude Code, Codex, and a harness that reads plain text.
GitHub.com permits all four profiles; an explicit GHES host refuses Codex
before workspace or launcher effects and offers only the approved corporate
alternatives.

**Looking for agents ([RFC-BPR-0092](../rfcs/RFC-BPR-0092-agent-first-run-setup.md), owner answer Q2).**
This amends the earlier rule that merely showing the panel performs no
executable discovery. The first time a window's Agent tab is shown, and on
**Look again**, the setup host looks for each agent the current Git host's
policy permits. Looking reads file metadata only: no process starts, no shell
runs, nothing is written, and the application's own `PATH` is used. Claude is
found as before (a saved process wrapper, then `PATH`, then the usual
folders, below). Codex is found the same way (Q5): a saved Codex program path
in Settings > Agents & context, then `PATH`, then `HOME/.local/bin`,
`/opt/homebrew/bin`, `/usr/local/bin` and `HOME/.npm-global/bin`; on Windows
only `codex.exe`. The plain-text harness is never searched for: it runs only a
program path the reviewer chose there. Every Codex and harness launch
resolves the same way (`located_agent_process`), so a shipped build starts the
found absolute program rather than handing a bare name to a launcher that
refuses one; none found ends the request `failed_to_start` and starts nothing.

**Check it works.** The setup's one explicit check runs only the chosen agent's
own status command (`claude auth status --json`, `codex login status`) and a
`--version` read, through the supervised launcher with the profile's
environment allowlist, no review context and a 15-second bound, and then makes
and removes the workspace an Ask would make (below). It sends no question, no
code and no model request.

[[BPR-1769]] keeps the effective saved entry's program and fixed arguments
for both check commands, even when detection chose a migrated preset before
its preferred entry was saved. Claude's third-party Bedrock status JSON is
accepted through the same strict `loggedIn` field; it does not require a
normal Claude account or validate the corporate network/SSO session itself.
Custom agents only prove program availability or their own configured check,
and the UI says so. Both agent and workspace work remain visibly busy until
settled. Stop cancels the workspace probe through its own token, cleanup still
runs, and choosing the private-copy fallback retries directly. Each failed row
has its own bounded local plain-text disclosure; Git's log is adapter-redacted.
These outputs are not automatically logged, exported or copied.

After the old settings have migrated, the canonical agent entries are the
only saved-wrapper authority. Reset clears that list and the legacy wrapper
and model fields, preventing the old wrapper returning without its arguments.
The Agent and Repository steps offer their own metadata rescans; the Check
step offers only check/recovery controls.

Initial protocols:

```text
codex_app_server
codex_jsonl_exec
claude_stream_json
external_terminal
```

The external-terminal fallback is mandatory when a corporate harness exposes no supported machine-readable interface.

### Claude process wrapper

[[BPR-1301]]. Settings > Agents & context offers one setting per application
profile, **Process wrapper** under **Claude**, modelled on the VS Code
extension's `claudeProcessWrapper`. It is one absolute path to a program run in
place of `claude` for every Claude stream-JSON launch of that profile: the
agent panel, side chat, comment workshop, tool-free writing and Review Cycle
Send to Agent. A wrapper is either a separately installed `claude` or a
program that prepares a provider (Bedrock, a gateway, credentials it fetches
itself) and then runs Claude. The registry names the bare `claude`, which no
launcher starts as it is; with no wrapper saved the launch searches for it
(below).

- **A path, never a command line.** The text is not split, quoted or expanded
  (`~` and variables included) and no shell runs it. It is accepted only as an
  absolute path to an existing regular file this user may execute, through a
  symbolic link or not, with no control character and at most 1,024 bytes. On
  Windows it must be an `.exe`: `CreateProcessW` runs a `.bat` or `.cmd`
  through `cmd.exe`, so those are refused (owner, 2026-10-06, "Accept (.exe
  only)"). A Unix script with a `#!` line is started by the kernel from the
  file, which is not a shell string. Each refusal has its own one-line reason,
  and a refused value is not saved.
- **Checked twice.** The adapter inspects the file when the reviewer saves it
  and again before every launch. A wrapper removed or no longer executable ends
  that request as `failed_to_start` naming the wrapper; nothing is started and
  there is no fallback to the bare `claude`.
- **Exactly Claude's launch.** One substitution point,
  `application::wrapped_claude_process`, sits in front of the supervised
  launcher and replaces only the executable. The wrapper receives Claude's
  exact arguments with nothing added (BetterPullReview bundles no Claude, so
  unlike the VS Code extension it passes no bundled path), the same working
  directory, stdin context, environment allowlist, supervisor, process group,
  Job Object and three-handle list. The core profile registry stays immutable
  and host policy decides by profile identity, so every verdict, including
  the explicit-GHES rule, is the one the profile already had. Codex and the
  external terminal are unaffected.
- **No environment widening.** Provider configuration lives in
  `~/.claude/settings.json`, reached through the allowlisted `HOME`, or is set
  by the wrapper itself; a wrapper that wants `AWS_PROFILE` or a gateway URL
  sets it inside its own script. The one name added since is `USER`, on the
  shipped Claude profile only ([[BPR-1375]]): a login name, not a secret. On
  macOS Claude Code files its account sign-in in the login keychain under it,
  and without it every request answered "Not logged in" although the person
  was signed in (Claude Code 2.1.296, 2026-10-09: `claude auth status` reads
  `loggedIn: false` with `HOME`, `PATH`, `LANG` and `TMPDIR` alone and `true`
  once `USER` is added). BetterPullReview never reads the keychain or the
  sign-in itself.
- **No wrapper: `claude` on `PATH`.** Owner amendment, 2026-10-07, verbatim:
  "Yes, search PATH (Recommended)". With no wrapper saved, a Claude request
  that names the bare `claude` runs the first `claude` (`claude.exe` on
  Windows; no `PATHEXT` script, as for a wrapper) that is an absolute
  executable regular file, searched in this order: each absolute entry of the
  application's own `PATH` (empty, relative and `~` entries, the current
  directory included, are skipped), then the fixed usual install folders
  (`core::usual_claude_folders`): `HOME/.local/bin` (the native installer),
  `HOME/.claude/local`, `/opt/homebrew/bin`, `/usr/local/bin` (Homebrew, and
  npm's global folder under Homebrew or installer Node) and
  `HOME/.npm-global/bin`; on Windows `%USERPROFILE%\.local\bin`. The fixed
  folders exist because a macOS application opened from the Finder or the
  Dock gets launchd's `PATH`, `/usr/bin:/bin:/usr/sbin:/sbin` (observed
  2026-10-07 on an application opened through LaunchServices), which holds
  none of them; a login-shell `PATH` would, but no shell is ever started to
  ask for one. The search is repeated at every launch, so a `claude` removed
  or replaced since Settings showed it is not the one that runs; none found
  ends the request as `failed_to_start` ("claude was not found on PATH or in
  the usual install folders; ...") and starts nothing. The found absolute
  path replaces only the executable, exactly as a wrapper does, and is never
  run through a shell. Settings shows it read-only under the field ("Using
  /opt/homebrew/bin/claude (found on PATH).", "... (found in a usual install
  folder, not on PATH).", or "claude not found on PATH or in the usual install
  folders; set a wrapper."), looked up again whenever the row is shown. A
  saved wrapper always wins: nothing is searched, and a refused wrapper never
  falls back to the found `claude`. A request that already names its program
  by path is not searched for.
- **Sign-in is Claude's.** BetterPullReview asks the preset agent's own
  status command during setup, but has no agent sign-in prompt and offers
  no "don't prompt to sign in" switch (owner, 2026-10-06,
  "Accept (no switch)"); the page says in one line that sign-in is left to
  Claude and the wrapper. Tool-free writing uses the same sign-in as every
  other Claude request: the person's own Claude account, read by Claude
  ([[BPR-1375]], owner 2026-10-09, verbatim: "no claude should use -p or the
  equivalent, the free version cli api same as codex"). When Claude answers
  that it is not signed in (`error: "authentication_failed"`), the request ends
  as `not_signed_in` and the tour editor says "Claude isn't signed in. Run
  claude auth login in a terminal, then try again. Your text is unchanged."
- **Model.** [[BPR-1375]], owner 2026-10-09. The row's **Model** is one name
  saved for the profile and passed as `--model NAME` after Claude's print-mode
  arguments, for tour writing and Ask agent requests whose request names no
  model of its own. Only letters, digits and `. _ - : @ / [ ]`, starting with
  a letter or digit and at most 128 bytes, are admitted, at the row, in the
  store and again by the launch builder, so it is always exactly one argument
  and never an option. Whether the account may use the model is Claude's
  answer.
- **Trust.** The wrapper is code the reviewer chose, at the same trust as
  choosing which `claude` to run, and BetterPullReview does not verify that it
  runs Claude. Under an explicit GHES host the "approved corporate harness" is
  whatever that reviewer's wrapper runs. The tool-free writing restrictions are
  Claude flags, so a writing run is only as tool-free as the wrapper keeps it.
  The path is profile data, not a credential: it is stored and shown as typed
  and never logged with arguments or environment.
- **Agent-free builds** have neither row. The stored value stays in the
  profile's settings, carried through every other preference write unread.

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

**A linked clone's history ([RFC-BPR-0092](../rfcs/RFC-BPR-0092-agent-first-run-setup.md), Q7, Q8 A).**
When the pull request's repository is linked to the reviewer's own clone, the
disposable workspace gets one `objects/info/alternates` line naming that
clone's objects folder, the only alternate the topology check then admits.
The agent reads the clone's history and blame through it; nothing is written
to the clone, and the workspace still has no remote. Only what the clone lacks
is copied, from the application's own cache and never through the reviewer's
remote or credentials: nothing when the clone holds the head, the head's
objects without the base's when it holds the base. Otherwise the private copy
also holds the base its cache has, so the brief's named diff works when the
base is not an ancestor of the head. A sparse review's workspace is shallow at
the head unless the clone holds every parent of the head. This is in place of
the `git worktree` the RFC first described, which the owner accepted on
2026-10-22. Check it works makes and removes the same workspace through the
agent host's `probe_workspace`, with a session identity from a reserved range,
no admitted session and no process.

## Context supplement

Version three ([RFC-BPR-0092](../rfcs/RFC-BPR-0092-agent-first-run-setup.md))
adds the review brief to every review request: each entry point but a
comment-workshop press, and never a writing request. The brief holds what the
signed-in reviewer can already read on GitHub, bounded with every cut counted:
the title, the description (16 KiB, cut at a line), the base branch, the
changed files with their status, the review threads (64 KiB in all, unresolved
first, omissions counted), the workspace's kind (`linked_clone_history`,
`private_copy` or `private_snapshot`, set once it is made), the diff named
rather than copied (`git diff BASE...HEAD`, none for a snapshot), and the
review skill's text. It also carries the conversation so far about the pull
request ([[BPR-1753]], owner decision (b), 2026-10-23): the earlier
questions and answers, oldest first (`brief.conversation.N.asked` and
`.answered`), each side cut to 8 KiB, the newest 50 turns within 48 KiB and
the older ones counted (`brief.conversation.omitted`). The application
carries the conversation rather than a harness's own resumable session, so a
follow-up continues it in whichever agent answers. The turns are kept, as the
Agent tab shows them, in the profile's `agent-setup.sqlite3` for each pull
request (host, owner, name and number; at most 50 turns, 64 KiB per text and
500 pull requests, the least recently written going first); Clear session
removes them, and settings resets remove the file. They are the reviewer's
questions and the agent's prose, and no credential. Its lines follow every
version-two field and are written only when present, so a launch without a
brief differs from version two only in its `schema` line. The agent may still
read GitHub through its own tools.

**Agents the reviewer adds ([[BPR-1753]], owner amendment
2026-10-23).** The picker offers only the agents in the profile's list
(`core::agent_entry`, at most 16, kept in `agent-setup.sqlite3`): each a
name, a protocol, a program and fixed arguments, with a default model and
effort the protocol takes. Each launches as a fixed profile slot of its
protocol (`core::agent_entry_profiles`), told to every host at composition
and allowed by `core::built_in_agent_policy` wherever its protocol's built-in
profile is; the policy question is unchanged. `application::agent_entry_process`
starts a slot as its entry says: the entry's program, inspected again at each
launch under the wrapper rules (a popular agent with none has its own program
looked for again), and the entry's fixed arguments before the protocol's
own. Nothing from a pull request, a question or a setting is put into the
arguments, no shell runs them, the environment allowlist is the protocol's,
and no credential is involved. A pull request's conversation is kept per
agent (`agent_conversation_key::Agent`), so a follow-up carries only its own
agent's earlier turns.

**What a harness offers ([[BPR-1753]], owner look v006).** The setup
host asks an added agent's harness for its models and effort levels when the
Agent tab shows that agent or its menu opens, once per window: a Codex
protocol runs the agent's own program and fixed arguments with `app-server`,
`initialize`, `initialized` and `model/list`, and closes its input once the
list is in (`agent_launch_purpose::model_catalog`); Claude Code runs with
`--help`. Each is a tool-free run like the setup check: an empty folder of its
own, the profile's environment allowlist, no workspace, no context and the
check's 15-second bound. BetterPullReview reads no Codex or Claude
configuration file.

**No run time limit by default ([[BPR-1753]], owner decision
2026-10-23).** A run has no time limit unless the reviewer sets one in
Settings › Agents & context (one limit for every agent, in minutes, kept in
`agent-setup.sqlite3`); with none, the launcher is given
`application::agent_run_without_limit`, the longest wait an `int` of
milliseconds names. Stop, closing the window and quitting end a run as
before, through the same process-group supervision; output, workspace and
frame bounds are unchanged and are not time limits. A custom agent's setup
check runs only its program's own check arguments, fixed and without a
shell, and must exit 0; with none it only inspects the program.

**Findings in the answer ([[BPR-1753]], owner decision 2026-10-23).**
A harness writes its own result frame, so its model cannot add `bprFindings`
to it. The review skill therefore asks for a block fenced `bpr-findings` at
the end of a review's answer: a `findings` array of `path`, one-based
inclusive `startLine` and `endLine` (at the head unless `side` is `left`),
`severity`, `category` and `confidence` by the payload's stable names,
`claim`, and optionally `evidence`, `consequence` and `suggestedComment`.
When a successful turn returned no structured payload,
`application::decode_agent_review_findings` reads the last such block from
its output (at most 32 findings within 48 KiB; an entry that is not a
finding is left out), states the provider and analyzed base and head from
the session, and anchors a finding to its first line only on a file the
turn's frozen snapshot holds; any other finding has no anchor, and
`core::validate_agent_finding` delivers it unanchored. Nothing reaches a
draft or a note without the reviewer's gesture on its card.

**The review skill (owner decision D1, Q9 A).** The method ships as an Agent
Skills package, `packaging/agent-skills/bpr-pr-review/SKILL.md`, built into
the application with `metadata.bpr-version`. Its text goes in every review
request's brief, the same for every agent, and nothing is written to any
harness's configuration. It grants nothing. The reviewer edits it in Settings
> Agents & context > Review skill; the first edit keeps a copy in the profile's
`agent-setup.sqlite3` with the built-in version and SHA-256 it came from. A
save must keep the name `bpr-pr-review` and a description and stay within
64 KiB. An update never changes an edited copy: a newer built-in is offered
with **Compare**, **Use version N** and **Keep mine**, and Restore default and
Use version N save the copy to Downloads first and remove nothing when that
fails.

The version-two fields supply the approved local state that GitHub does not
represent:

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
QProcess-owning thread. On Unix, a bundled, Qt-free
`better_pull_review_agent_supervisor` starts the selected agent in its own
process group. The supervisor starts a separate session so application-group signals and
orphaned-stopped-group hangups do not interrupt child cleanup. A dedicated local socket carries only launch errno and the
application's lifetime; context and protocol traffic retain their ordinary
stdin/stdout/stderr paths. Closing the application end requests teardown even
when the application was forcibly killed. Qt closes inherited descriptors
above the dedicated channel before helper execution, and the helper closes
that channel before agent execution. Agent argv and the profile environment
remain unchanged. The helper holds the direct child unreaped until its group
has been signaled, preventing PID reuse during cleanup, then forwards its
normal exit status or terminating signal. It performs no provider, credential,
repository or network work. Descendants that remain in the assigned process
group are covered; this is not a sandbox for a program deliberately escaping
that group. The helper resolves beside the installed application executable;
the tests-enabled adapter selects the same helper from the build tree. Its
bundle copy depends on the helper binary so helper-only rebuilds refresh it.
`QProcess` cannot assign a Windows Job Object before the child begins running,
and post-start assignment leaves a descendant-escape race, so Windows does not
launch through `QProcess` at all. The adapter dispatches into a private WIN32
backend, `windows_agent_process.cpp`, which creates the child suspended,
assigns the Job Object, and only then resumes it; assignment-before-resume is
pinned by a source-order assertion in the architecture suite. That backend
compiled under MSVC on 2026-08-22 and on 2026-08-26/27 passed the full
target-native canary — including the no-pre-assignment-side-effect and
descendant-teardown courses — and all five composed agent courses, recorded in
BPR-0240's dated sections. There is no pre-start refusal on Windows; the
product owner chose to document the shipped dispatch rather than restore one
(BPR-0332, 2026-09-01).

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
BPR-0622 adds an optional exact guard-file reference to the launch request.
The Unix launcher validates its canonical path, private regular-file identity
and single link, then takes a nonblocking shared lock through a read-only
descriptor. The supervisor retains a duplicate until group teardown; the agent
receives neither that descriptor nor the lifetime channel. An exclusive
recovery lock therefore excludes launch and remains excluded while the old
supervisor still runs after application death. The parent-death fixture pauses
that supervisor to observe this interval directly. Busy, substituted and
symlinked guard files refuse before child execution. A requested guard on an
unsupported platform refuses rather than dropping lifetime protection.

The completed BPR-0622 path supplies this guard from durable workspace ownership
in production. `github_application.cpp` schedules ordinary startup recovery on
the Git effect lane through `recover_installed_agent_workspaces`; the installed
Git adapter validates bounded journal, scope and original native identities
before reclaiming an abandoned workspace. Live owners are preserved. Missing,
ambiguous or substituted ownership causes visible repair/refusal; neither a
directory name nor an exclusive lock alone authorizes removal. Historical roots
without durable authority remain outside this recovery path. A startup refusal
is stated in every window's status bar as well as in the agent panel. It is not
a cleanup failure of the quit, so an orderly quit still exits 0 ([[BPR-0817]]).

The `application_agent_crash_recovery` and materialization-recovery courses
force-kill the real composition, check child termination, Git/SQLite identity
and repeated ordinary recovery without replay, then explicitly start the next
session. Where durable ownership exists they require workspace reclamation;
the earlier BPR-0621 orphan-preservation result is historical. BPR-0622 records
native Windows, Linux and macOS evidence, summarized in the
[RFC-0013 ledger](../verification/RFC-BPR-0013.md). That evidence does not certify
the RFC-0014 installed-package or complete purge matrix.
The composed-path contract observes the portable process launcher through a
test-only delegating probe: the real panel gesture must produce one launcher
`run`, one host process start, and the real child canary. A distinct launcher
construction cut preserves the gesture while removing both launcher and host;
the exact positive predicate then stays false and every downstream effect
counter remains zero. Separate host-construction and gesture cuts retain their
own boundaries. None of these portable courses claims that the Windows backend
compiles or runs.

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
| A timeout terminates a live descendant heartbeat, POSIX and Windows alike | `qt_agent_launch_canary` |
| Forced Unix application death stops a stubborn agent and its live descendant, and a fresh launch succeeds | `agent_parent_death` (Mac execution; composed state recovery is checked separately below) |
| The supervised agent receives neither an unrelated high descriptor nor the lifetime channel | `qt_agent_launch_canary` (Unix descriptor probes) |
| The composed portable launcher is constructed and invoked once by the real panel gesture | `application_agent_invocation_composition_contract`, `application_agent_invocation_launcher_construction_cut_contract` |
| Removing the host or disconnecting the gesture cannot satisfy the public canary | `application_agent_invocation_construction_cut_contract`, `application_agent_invocation_gesture_cut_contract` |
| A Windows child whose Job Object assignment fails never runs its first statement, and teardown stops a spawned grandchild's heartbeat | `qt_agent_launch_canary` (Windows-conditional: `test_windows_assignment_failure_has_no_pre_assignment_side_effect`, `test_timeout_terminates_the_descendant_process_tree`), target-native 2026-08-27 |
| Composed agent-session death preserves Git/SQLite state without replay, reclaims durably owned abandoned workspaces and permits the next explicit session | `application_agent_crash_recovery`, `application_agent_materialization_crash_recovery`, `agent_workspace_ownership` (BPR-0622's exact native candidates) |
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
Codex or Claude remains optional extra validation rather than a gate. The
Windows process lifecycle and descendant containment are claimed only on the
strength of BPR-0240's 2026-08-26/27 target-native runs; a portable host
cannot reproduce them and does not pretend to.

## Explicit AI review: findings, chat, workshop, and the MCP helper

Everything above is RFC-BPR-0011: how a separately authenticated agent is
launched, what context it is given, and what a session is bound to.
RFC-BPR-0012 sits on top of that and adds what the reviewer does with the
answer — structured findings, a revision-aware side chat, a comment workshop, a
full agent session, and a read-only MCP context helper that serves the frozen
projection outward.

The sections below state what was built, not what was intended. Each claim
names the card that landed it and the check that proves it; the row-by-row
ledger is `docs/verification/RFC-BPR-0012.md`, and everything RFC-BPR-0012
specified and did not build is named under **What RFC-BPR-0012 does not
claim** rather than left to silence.

### The finding schema

`core::agent_finding` in `src/core/include/bpr/core/agent_finding.hpp` is what
an agent returns about a review. It carries the provider, severity,
confidence, category, path, line range, anchor, claim, evidence, consequence,
an optional suggested comment, the context the finding says it required, the
base and head OIDs it was analyzed against, and the commits it reports having
read inside that range (BPR-0497, `core_agent_finding`).

Four decisions are structural rather than conventional.

**Severity and category reuse the defect vocabulary.** `defect_severity` and
`defect_category` are the same ladders the local defect workflow already uses,
so promoting a finding into a defect needs no mapping — and a mapping is where
a severity gets silently widened or a category invented.

**Confidence is data, never authority.** `agent_finding_confidence` defaults to
`unstated`, because an agent that said nothing has not thereby claimed the
lowest confidence, and nothing in the validator reads it — not even to bound
it, since a branch that refused an unknown enumerator would still be a branch
in which confidence changed an anchoring outcome. An enumerator nobody declared
is refused by the decoder instead, which is where wire input stops being wire
input (BPR-0500, `application_agent_finding_protocol`).

**Every text field and the analyzed-OID list carries a declared bound**: 4 KiB
for the claim, 16 KiB for the evidence, 4 KiB for the consequence, 16 KiB for
the suggested comment, 4 KiB for the required context, and 256 analyzed
commits. Seven bounds are admitted at the limit and refused one byte past it.

**There is no field a Qt coordinate could occupy** — not a y-position, a
viewport offset, a scroll position, a widget row, or a ribbon path. Position is
stated only in RFC-BPR-0004 identities, which are facts about the diff rather
than about the screen showing it, so they survive scrolling, folding, wrapping,
and a theme change. The guard reads the type: a structured binding names every
member, and adding one fails to compile (`decomposes into 16 elements, but only
15 names were provided`). A test that built a finding and searched it for a
coordinate would pass for every finding nobody thought to put one in.

On the wire, one JSON object per finding rides inside the protocol's own result
framing, and the payload is `{"session":<n>,"findings":[…]}` with the core's
field names. Codex App Server carries it as `params.turn.bprFindings` on the
completed `turn/completed` notification; Codex JSONL as top-level
`bprFindings` on `turn.completed`; Claude as top-level `bprFindings` on the
successful `result` frame. The external terminal has no result frame at all, so
a payload there is refused as `unsupported_finding_transport` rather than given
framing nobody specified. Decoding happens once, in the portable protocol loop
both transports reach, under `maximum_agent_finding_payload_bytes` (48 KiB,
`static_assert`ed under the frame bound so it is reachable) and
`maximum_agent_findings_per_turn` (32). Five of the twelve declared
`agent_protocol_failure` values are this payload's own —
`finding_payload_too_large`, `too_many_findings`, `malformed_finding_payload`,
`unknown_finding_session`, and `unsupported_finding_transport` — and
`agent_protocol_failure_count` fails the build if a new one is added without
being distinguished (BPR-0500, `application_agent_finding_protocol`,
`application_agent_protocol_loop`).

### The anchor policy, and its three states

An anchor is a semantic identity or it is nothing.

`core::agent_finding_anchor` engages exactly one of `Line` (a
`source_line_ref`) or `Boundary` (a `source_boundary_ref`) — the only two
RFC-BPR-0004 identities that name a *place*. `Segment`, `Group`, and `Move`
ride beside the position as optional refinements the frozen snapshot must
contain, never as alternative positions: a segment or group covers a span and
could not say which line a claim is about, and a move relation names a pair of
segments, so admitting either as a position would let one finding anchor to two
places at once. `line_pair_id` is deliberately absent, because a pair is a
derived correspondence between two lines that already have identities, and
admitting it would give one position a second spelling.

`core::validate_agent_finding` decides the verdict from two supplied values and
nothing else — no clock, no provider, no filesystem, and **no current head**.
The snapshot it validates against is `core::agent_finding_snapshot`, whose
`Binding` is the session's own frozen `agent_revision_binding` — what the agent
was asked about — with one `agent_finding_snapshot_file` per path carrying the
comparison digest, both side line counts, and the segment, group, and move
identities the frozen alignment contained.

The order of the three stages is itself the policy:

1. Whether this is a well-formed finding at all. These checks read only the
   finding, so a malformed one reports that whatever it is compared against.
2. Whether it was analyzed against the revision the snapshot froze. A finding
   from another revision is stale, and is refused **before any line count is
   consulted** — measuring it against the newer snapshot is the silent
   reattachment the RFC forbids, and it would sometimes succeed, which is worse
   than always failing.
3. Whether the frozen snapshot actually contains what the anchor names.

The three states are `anchored`, `unanchored`, and `stale`, pinned at three by
`agent_finding_anchor_state_count`; a fourth would fall somewhere between
anchored and refused and a caller would have to guess which side of the line it
was on. When an anchor does not hold, `agent_finding_refusal` says why:
`unknown_path` (matched exactly — there is no nearest, prefix, or case-folded
match), `span_past_end`, or `malformed`. **The verdict has no field a line
could occupy**, which is how "never a nearest or inferred line" is a property of
the type rather than of the implementation; a refusal reports that it refused
and does not hand back somewhere else to look. The analyzed base and head ride
on every verdict, refusals included, because a reviewer choosing between
continuing against the old revision and asking again needs to see which revision
the answer was about (BPR-0497, `core_agent_finding`: 29 refusal rows over every
refusal site, and mutations that made stale fall back to anchored, made the
line-count check always-true, added an early confidence branch, or added a
`ViewportY` field each failed by name or failed to compile).

The host validates through that one function and delivers the result as
`application::agent_finding_delivery`, which carries the session id, the
session's own binding, the staleness of that binding against what is observed
now, and one verdict per finding in the order the payload stated them. **There
is no field the head current at delivery could occupy**, so a payload arriving
after an accepted head move is delivered `head_moved` with its analyzed OIDs
intact rather than adopting the new head. Nothing is dropped: an invalid anchor
re-enters as `unanchored`, because a reviewer who cannot see what the agent got
wrong cannot correct it. Deleting the validation call at that seam fails an
invalid-anchor case by name (BPR-0500, `application_agent_finding_protocol`,
`application_agent_invocation_host`).

In the composed application the snapshot is read once, from the alignment the
reviewer is looking at, stated as `agent_invocation_request::FrozenFiles`, and
written to the MCP helper's snapshot root from those same values — so one
revision has one answer rather than two that could disagree (BPR-0504,
`application_agent_finding_composition_contract`).

### Chat scope

Seven entry points, four observations, and one intent type that cannot be
obtained without naming an action.

`core::agent_entry_point` is the closed set `docs/ux/ai-experience.md` names:
`ask_about_selection`, `analyze_current_file`, `analyze_latest_upload`,
`evaluate_proposed_fix`, `workshop_comment`, `run_ai_review`, and
`open_full_agent_session`. Every switch over it omits a `default` label and
`agent_entry_point_count` pins the size, so an eighth fails the build twice
until somebody says what it acts on and what it is called.

`core::agent_action_intent` has a deleted default constructor and an `explicit`
one taking an entry point. A default-constructed intent would mean "some AI
action", which is the shape of the accidental invocation this RFC exists to
prevent, and the `explicit` constructor means an entry point never converts
into an intent at a call site — the command a reviewer gave is written out at
the place it is acted on.

`core::agent_observation` declares the four things that happen while a review is
open — `refresh`, `navigation`, `comment_arrival`, `revision_update` — so the
sweep proving they invoke nothing has something exhaustive to sweep. All an
observation may become is `agent_observation_record`, a one-field value with
**no field an invocation, an intent, or an entry point could occupy**; an
observation that carried an entry point would be an invocation waiting for
someone to notice it was free. `intent_for` returns nothing for every
observation, unconditionally — it exists so that the refusal is one reviewable
place rather than an absence somebody fills in locally.

Scope is `core::agent_chat_scope_kind`, ordered narrowest to widest —
`selected_lines`, `file`, `pull_request`, `repository` — so `wider` is a
comparison of enumerators rather than a table. Exactly one identity shape is
engaged per kind: selected lines engage the selection, a file engages its
identity and comparison, and the pull-request and repository scopes engage
nothing at all, because the session's own binding already names the host, the
repository, and the pull request — a scope that named its own pull request
could name a different one from the session it belongs to.
`next_agent_chat_turn` is the only place a scope changes, and with no widening
intent it is the identity: no branch below it can widen, and neither
navigating, scrolling, nor selecting something else is an argument to it. That
is what "navigation does not silently add context" means as a property rather
than a habit. A widening records `WidenedFrom` and keeps it, so the drawer can
say "widened from your selection" for as long as the widened scope lasts.

`core::decide_agent_action` is a sibling of `decide_agent_invocation` rather
than a widening of it — giving the older function defaulted intent, selection,
and session parameters would have created the implied intent this design
forbids — and it calls the older one for the host-policy half, so there is one
host-policy rule rather than two that can drift. It decides in order: whether
the intent names a declared entry point, whether the host permits the profile,
whether the session is usable and is this profile's session, and whether the
entry point has the selection it acts on. The verdict names the entry point it
decided in every state, so a permission for "run AI review" cannot be carried
over to "open full agent session" without the mismatch being visible in the
value (BPR-0499, `core_agent_chat_scope`).

The panel obeys the same rule one layer out. `qt::side_chat_panel` discloses
ten fields as ten separate labels — provider, Git host, workspace, epoch pair,
head OID, session state, scope, chips, private-context opt-in, and
continuations — and its `observe` contains no dispatch call and writes no
field. Driving refresh, navigation, comment arrival, and revision update
through the shipped panel produces zero intents, zero chips, and no movement in
any of the ten. A chip appears only from a projection; nothing in the widget
appends one. Each of the four gestures emits exactly one typed intent, and the
send carries a `core::agent_action_intent` built from the projection's own
entry point, so every message leaving the panel names the explicit action it is
(BPR-0502, `qt_side_chat_panel`).

**The typed message rides the supplement, not a second channel.** Since
[BPR-0517](../issues/60-done/BPR-0517-rfc-0012-carry-the-side-chat-message-in-the-agent-frame.md)
`core::agent_context_supplement` carries a bounded `Message`
(`maximum_chat_message_bytes`, 16 KiB) that the core admits only for the
session the send names — `valid_chat_message` refuses an out-of-session or
oversized message by name — and only the chat session host fills it, from the
panel's send intent. `encode_agent_supplement` writes it once as
`chat_message` after `pull_request_url`, and `frame_agent_input` documents
where each protocol carries that payload; the decode side is unchanged.
`application_agent_chat_composition_contract` asserts the fixture agent
receives the question exactly once, and
`application_agent_chat_continued_contract` drives
`continue_against_old_revision` to one further turn under the kept session.

### The two stale continuations

After head movement a session is stale and is never retargeted.
`core::offer_agent_continuations` carries `observe_agent_session`'s verdict
whole and returns an `agent_continuation_offer` whose `Choices` is a fixed
two-element `std::array` — `start_new_session` and
`continue_against_old_revision` — so "exactly two" is a property of the type
rather than of a branch that could one day append to a vector. `Required` says
whether the reviewer has to choose; the choices themselves do not change with
the answer, because what can be done with a session does not depend on why it
stopped matching, and `different_review` therefore offers the same two with
`Required` true. `ContinueAgainst` is the session's own binding,
unconditionally, and **the type has no field the observed binding could
occupy** (BPR-0499, `core_agent_session`).

`qt::side_chat_panel` presents a stale projection as stale using the core's own
`head_moved` name, offers exactly that two-element array in the core's order,
and emits nothing while doing it: the intent count after presenting a stale
session is zero, and Send is disabled until the reviewer answers. Each
continuation emits exactly one `choose_chat_continuation_intent` when and only
when it is chosen, carrying the offered binding — asserted unequal to the moved
head — because the projection type has no field the observed revision could
occupy. Making either continuation start itself on presentation fails
`presenting a stale session starts neither continuation` (BPR-0502,
`qt_side_chat_panel`). In the shipped application `refresh_agent_authority`
presents the panel with the session's own binding, so its head-OID field carries
the accepted head rather than anything the panel computed (BPR-0504,
`application_agent_finding_chat_panel_construction_cut_contract`,
`composed_application_hosts`).

### The promotion boundary

AI output is a proposal until a person promotes it, and nothing it says can
publish.

`core::promote_agent_finding` takes one validated finding, its verdict, and one
explicit choice, and returns exactly one local artifact or a typed refusal. The
three choices are `draft_comment`, `defect`, and `private_note`, and there is
deliberately no fourth: a published comment is not something a finding can be
turned into, because publication is a separate gesture the reviewer makes
afterwards against text they have had the chance to read. Dismissal is not a
choice either — it produces no artifact, so it arrives as
`agent_finding_standing::dismissed` on the request and is read *before* the
verdict, since the reviewer's decision does not depend on it.

**The result type has no field a `core::review_publication_proposal` could
occupy**, and that is how "a finding can never publish" is a property of the
type rather than of the implementation. A structured-binding guard and a
per-field trait both fail to compile when one is added. `command_count()`
exists so "exactly one" is checkable rather than merely intended; a promotion
that produced a draft *and* opened a defect would be two decisions the reviewer
made once.

Promotion is onto an existing local artifact, never a creation. The core's
`review_draft_command` is `edit`/`discard`/`begin_publish`/… against a named
draft and `defect_command_kind` has no "file a defect", so what a finding
becomes is an `edit` of the reviewer's draft whose body is the suggested comment
byte for byte, a `reclassify` of the reviewer's defect carrying the finding's
severity and category unmapped, or a `supplement_note` payload whose text is the
claim byte for byte. **Nothing is composed**: evidence and consequence are
deliberately not folded into the note, because the evidence bound alone equals
the note bound, so a composed note could only be repaired by truncating it or
by refusing a well-formed finding.

**The anchor is checked, never translated.** Neither command carries an anchor
field, so the anchor cannot ride along inside one; instead the named draft or
defect must already be anchored at exactly the finding's `source_line_ref`, and
a mismatch is `anchor_elsewhere`. Without that, promotion would move the agent's
claim onto whichever line the caller happened to pass — an inferred anchor
arriving from the other direction. A boundary-anchored finding fills neither,
because the line above or below a seam is one the agent did not name; it stays
promotable into a note, which attaches by file comparison rather than by line.
No `review_mutation_target` is built anywhere: it names a host, repository, pull
request, and expected head, and assembling one from a finding would hand the
host a half-formed publication for something the reviewer has not read. Eleven
refusal reasons are swept over a coverage array tied to the enumeration count
rather than sampled (BPR-0498, `core_agent_finding_promotion`).

The host carries the same absence outward. `application::agent_finding_host`
holds one delivery so the verdict is *read* rather than supplied —
`agent_finding_action_request` has no verdict field a widget could forge one
into — and holds exactly two ports: a promoted-note store that writes through
the review ledger open right now (off the decision lane since [[BPR-1211]], its
answer presented when it returns), and a projection. **There is no mutation port, no
provider, no coordinator, and no publication path reachable from it.**
`agent_finding_outcome` has no field a `review_publication_proposal` or a
`review_mutation_target` could occupy. `promoted_note` builds the one note,
and writing it reaches exactly one port method,
`review_ledger_store_port::store_note`; that is the single authority agent
output may reach. Every outcome is presented, refusal included,
and `PresentedOutcomes` is asserted equal to `Actions`, because an outcome that
was computed and not presented is one the reviewer never saw (BPR-0500,
BPR-0504).

The workshop keeps both texts. `application::agent_comment_workshop_host` turns
one proposed draft `edit` into an `agent_comment_workshop_proposal` carrying the
draft it is against, the reviewer's `Original` read from that draft at the
moment the proposal is made, and the agent's `Proposed`. It has one port and no
store, and **applies nothing**: the edit reaches the draft only if the reviewer
accepts it on the surface. Five refusals cover every path that produces no
proposal — `different_draft`, `not_an_edit`, `empty_revision`, `unchanged` —
because "nothing appeared" is the one answer a workshop must never give.

**A press carries the review-feedback guidance while the label helper is on
(BPR-1091).** `application::workshop_feedback_supplement` turns the profile's
effective Review feedback hook (BPR-1090) into a `core::supplement_feedback_guidance`: the text, the shipped version it
was taken from (`review-feedback-v1`), and whether it is the shipped default or
the profile's override, which is sent as written and never merged with the
default. The composition fills it in `dispatch_agent_invocation` for a
`workshop_comment` gesture with an operation, and for nothing else; the core
admits it only beside an admitted workshop operation and refuses it whole
anywhere else, oversize, empty, or not UTF-8. `encode_agent_supplement` writes
`feedback_guidance.source`, `.version` and `.text` directly after
`workshop_operation`, and only when it is present: with the helper off, a press
is byte for byte the request it was before the hook existed
(`application_agent_launch` pins that wire's digest). The guidance is text on
the existing launch; it adds no argument, environment variable, tool, MCP
server, model call or publication path, and saving the hook or turning the
helper on launches nothing.

**The proposal says what it changes besides wording (BPR-1091).**
`core::compare_workshop_candidate` reads the original and the candidate with the
shared recognizer (`recognize_feedback_prefix`), compares every suggestion
block's exact bytes (`core::suggestion_blocks`), and reads whether the prose
still asks a question (code, fenced blocks and query strings excluded). The
host attaches the result to every `agent_comment_workshop_proposal`, and
`qt::review_draft_editor` shows it under the two texts, recomputed from the two
texts it shows, as `qt::workshop_candidate_line`: marks, not sentences. The
label is its BPR-1083 mark (weight and if-minor in the mark, a custom
decoration as written), a label change is the mark before an arrow and the
mark after (the Plain mark where a side has none), the question is a speech
bubble holding a `?`, and the suggested change is the BPR-1084 ring. A kept
facet is drawn in the faint ink with no word; a changed one in full ink with
one word, added, removed or changed. A header the recognizer cannot read is
its token as written under a wavy line in the attention colour, never passed
off as plain. Each facet's tooltip is its sentence, and the whole comparison
in words ("Label changed: question → issue (blocking) · No longer asks a
question", or "Wording only") is the accessible description. Nothing is
refused or rewritten on these grounds, and Accept is still one local
`review_draft_command` `edit` carrying the candidate's bytes, whatever the
candidate's text asks for.

On screen, `qt::finding_queue_view` offers exactly six actions —
`open_evidence`, `ask_follow_up`, `convert_to_draft_comment`,
`convert_to_draft_defect`, `add_to_private_notes`, `dismiss` — each emitting one
`finding_action_intent` naming the session, the ordinal, the finding, and the
verdict, and performing no promotion itself. Every action is offered against
every finding whatever its anchor state, and the anchor state is *shown*
instead: disabling an action on it would move that decision into a widget, and a
widget that answered it could disagree with the core. A seventh action declared
without a case fails at compile time on `expression evaluates to '6 == 7'`.
`qt::review_draft_editor` shows the original and the proposal on two separate
`QPlainTextEdit` surfaces at once; accepting emits exactly one
`review_draft_command` and writes nothing, and rejecting emits nothing at all
and leaves the body byte for byte what it was, tab and trailing space included.
Neither widget can hold a provider, endpoint, store, or process: each member set
is one aggregate bound by name, and a `void*` member added to either fails to
compile (BPR-0503, `qt_finding_queue_view`, `qt_review_draft_editor`).

Composed, the whole path holds. The `convert_to_draft_comment` gesture on the
shipped queue produces one `review_draft_command` and changes no draft, because
the conversion is presented as a before/after proposal first; accepting there is
the one gesture that writes, and the local draft body then equals the agent's
suggested comment byte for byte. `review_mutation_evidence` records zero
submissions, zero target observations, and zero stored comments across the
entire course. With the workshop host cut, the same gesture produces zero
proposals, leaves the draft byte-identical, and presents exactly one refusal on
the shipped agent panel — a refused conversion is named rather than dropped
(BPR-0504, `application_agent_finding_composition_contract`,
`application_agent_finding_workshop_construction_cut_contract`).

### Full-agent behaviour

A full agent uses its own GitHub tools under its own approvals, and what it
changes returns to BetterPullReview only through ordinary synchronization.

That is read two ways, because either alone would be weaker. A complete
full-agent launch — a real child process, its own workspace, a returned
structured payload, a promotion, and an acceptance — records **zero** GitHub
mutation calls of every kind, so nothing left the application while the agent
was working or after it finished. And the composition source names
`review_mutation_coordinator` zero times, carries exactly one
`ReviewMutationHost->publish(` call site — the reviewer's own `begin_publish`
branch — one `->submit(`, no `.submit(`, and no `observe_target` at all, so
there is no second edge for a backchannel to travel along. The positive
direction, that a publication does round-trip through that one path, is
`application_review_round_trip_composition_contract`, which pins the same file
by digest (BPR-0504, `application_agent_finding_full_agent_contract`,
`application_review_round_trip_composition_contract`).

The host's typed `deliver_findings` is the one new edge out of
`agent_invocation_host`, and it reaches `application::agent_finding_host` and
stops there.

### The MCP tool list

The helper is a first-party executable BetterPullReview ships,
`tool_better_pull_review_mcp_context_helper`, built from `src/adapters/mcp/`
and linking `BetterPullReview::Core` and nothing else — not the application
host, not Qt, not the GitHub, Git, or SQLite adapters — so "it reaches no
credential and no network" is a link line a reviewer reads in one line. The
`tool_` prefix is deliberate: it puts the helper in the manifest
`live_mutation_isolation` walks, so every build also proves the shipped helper
links no live GitHub mutation; `app_` would have exempted it. The placement
reasoning is recorded in full under **Placement decision (BPR-0501)** in
`docs/verification/RFC-BPR-0012.md`.

It speaks newline-delimited JSON-RPC 2.0 over stdio, at one declared protocol
revision (`2025-06-18`) rather than a negotiated range, reporting itself as
`better-pull-review-context` version `1`. Three request methods are answered —
`initialize`, `tools/list`, and `tools/call` — anything else is
`unknown_method`, and a notification carrying no identity gets no answer at all.

The advertised tool list is a closed source-level enumeration. There is no
dynamic, configured, or plugin registration anywhere in the adapter, so adding
a tool is a diff a reviewer reads. It is exactly these four, in this order:

| Tool | What it answers | Takes a path |
|---|---|---|
| `snapshot_binding` | The frozen `agent_revision_binding` this snapshot is pinned to | no |
| `list_snapshot_files` | Every path the frozen review projection contains | no |
| `read_snapshot_file` | One frozen file's bytes, bounded, from inside the snapshot | yes |
| `snapshot_file_alignment` | One frozen file's comparison, line counts, and alignment identities | yes |

**A mutation is not a tool that is turned off here; it is a tool that does not
exist.** There is no write, no draft, no publish, and no note store, not even a
disabled one, and no host, port, endpoint, URL, or credential argument anywhere
in the descriptor table. Ten declared refusals keep the causes separable —
`invalid_request`, `unknown_method`, `unknown_tool`, `invalid_arguments`,
`network_location_refused`, `path_outside_snapshot`, `snapshot_entry_missing`,
`snapshot_entry_too_large`, `revision_binding_stale`, `snapshot_unavailable` —
because a test that cannot tell a path escape from a stale revision cannot hold
either to account. Bounds are declared: 64 KiB per request line, 1 MiB per
`read_snapshot_file` answer, 4096 paths per projection, and 16 levels of JSON
nesting.

The advertised list is compared against three independent sources — the `enum
class mcp_tool` text read out of the header, the compiled `mcp_tools` array,
and a fourth list written in the canary — so a tool added to the first three
still fails until a person edits the test. The refusal sweep drives twenty-one
entries through the real executable and requires each to be refused by its own
exact typed cause with no result beside it. The child's complete environment
reduces to exactly `BPR_MCP_SNAPSHOT_ROOT` and `BPR_MCP_PINNED_BINDING` while
the parent holds planted GitHub, Anthropic, and AWS secrets, and no secret value
reaches either child channel. Head movement with the pin unchanged makes every
call `revision_binding_stale` and **re-reads nothing** — the difference between
reporting staleness and silently serving whichever revision exists now. No
socket surface survives: `nm -u` on the shipped binary shows no socket, connect,
bind, listen, accept, or resolver import among its 84 undefined symbols, so the
suite declares no `LOOPBACK_PORT_COUNT` and zero is the claim rather than an
omission. Nineteen scripted negative controls each removed one check and made
the canary fail (BPR-0501, `application_mcp_context_helper`).

In the shipped application `application::mcp_context_helper_supervisor` owns the
helper's whole lifetime, with its own process launcher — a shared one would let
one cancel stop whichever child the reviewer did not mean. It starts a helper
only on an explicit `start`; nothing in it starts one on observation, refresh,
navigation, or head movement. The launch is asserted whole rather than sampled:
**no argument vector at all**, so `--listen` is not a flag this side could pass;
a working directory that is the pinned snapshot root and names no URI scheme and
no network or UNC authority; and an environment of exactly the two names the
adapter declares, compared element for element against the supervisor's own
`environment_names()` so a rename fails rather than diverges. Five refusals
cover the rest — `no_helper`, `not_pinned`, `network_location`,
`already_running`. Cutting the construction leaves the reviewer's gesture
intact, writes no snapshot at all, and refuses the start as `no_helper`
(BPR-0504, `application_agent_finding_composition_contract`,
`application_agent_finding_mcp_construction_cut_contract`).

### Privacy

Private prose travels only where it was selected, and the selection is per
session.

`core::agent_private_context_optin` is keyed by `agent_session_id`, with notes
and drafts separately default-off, and the two-argument
`build_agent_supplement` carries zero of both without it. The opt-in lives on
the session rather than on a profile or a setting, so it cannot be turned on
once and forgotten, and a build whose ids disagree is refused whole (BPR-0499,
`core_agent_context_supplement`). The host builds through that gated overload,
keyed to the session, and a source-order assertion proves the ungated overload
is unreachable from the host (BPR-0500, `application_agent_launch`). Underneath
it all, `core::build_agent_supplement` copies notes from the selection and from
nowhere else and holds no note store, so a request that selected none produces a
supplement with none **because there was no source** rather than because a
filter removed them.

The MCP helper is the outward direction of the same rule. Its environment
allowlist is a list of two names, never a map from name to value — a map would
give every name a place a token could sit — and both entries are first-party and
non-secret: a directory path and a revision binding. The helper reduces its own
environment to exactly that list before it reads a single request, so a parent
that leaks a credential into the child leaks it into a process that has already
dropped it (BPR-0501, `application_mcp_context_helper`).

BPR-0971 changes the helper's startup stderr record to complete received and
retained counts, an unexpected-retained count, and presence of only the two
fixed allowlist names. Arbitrary environment names, values, argument text,
snapshot paths and revision identities are absent. Startup still reports the
fixed refusal cause or readiness. The complete serialized record has a 512-byte
limit and is best effort; missing stderr is not proof that reduction succeeded.
On POSIX, a single owned writer gets a 20 ms ordinary-delivery wait, then its
blocked write is canceled and the writer joined before protocol service starts.
No descriptor flags change and no writer survives the attempt. Failed delivery
cannot change the helper's exit or JSON-RPC result; stdout's signal behavior is
restored. A failed sink cannot report its own loss through that sink. This
course does not bound uninterruptible kernel/device stalls. Windows uses one owned synchronous-I/O writer with cancellation and join.
Windows and Linux native verification remain pending. The Mac courses are
`application_mcp_context_helper` and `application_mcp_terminal_diagnostics`.

Nothing the agent returns reaches a second authority. The finding host's only
durable write is `review_ledger_store_port::store_note`, and there is no branch
in that path that could publish, open a defect, move a review head, or admit a
session (BPR-0500, `application_agent_invocation_host`).

### Where to check each RFC-BPR-0012 claim

| Claim | Test |
|---|---|
| A finding type has no field a coordinate could occupy | `core_agent_finding` |
| Every declared text bound is admitted at the limit and refused one byte past | `core_agent_finding` |
| An anchor validates against the session's frozen binding, never the current head | `core_agent_finding` |
| Anchored, unanchored, and stale are three distinguishable verdicts, and none carries a substitute line | `core_agent_finding` |
| A stale finding is refused before any line count is consulted | `core_agent_finding` |
| One promotion yields exactly one local command, or a typed refusal | `core_agent_finding_promotion` |
| No promotion result can carry a `review_publication_proposal` | `core_agent_finding_promotion` |
| The suggested comment and the claim are carried byte for byte | `core_agent_finding_promotion` |
| Every one of the eleven promotion refusals is separately reachable | `core_agent_finding_promotion` |
| Seven entry points are legal only with their own explicit intent, and an eighth fails the count | `core_agent_chat_scope` |
| Refresh, navigation, comment arrival, and revision update produce zero invocations | `core_agent_chat_scope`, `qt_side_chat_panel` |
| A chat turn without a widening intent does not widen, and a widening records where it came from | `core_agent_chat_scope` |
| A stale session offers exactly two continuations and retargets neither | `core_agent_session`, `qt_side_chat_panel` |
| A session without the per-session opt-in carries zero notes and zero drafts | `core_agent_context_supplement`, `core_agent_chat_scope` |
| The host builds the supplement through the gated overload only | `application_agent_launch` |
| A structured payload decodes once, in the shared loop, on each carrying protocol | `application_agent_finding_protocol`, `application_agent_protocol_loop` |
| Every bound and malformation has its own typed cause, and all declared causes are pairwise distinct | `application_agent_finding_protocol` |
| The external terminal refuses a payload as `unsupported_finding_transport` | `application_agent_finding_protocol` |
| Deleting the validation call at the decode-to-core seam fails an invalid-anchor case | `application_agent_finding_protocol` |
| A payload arriving after an accepted head move is stale with its analyzed OIDs intact | `application_agent_invocation_host` |
| A promoted note reaches `store_note` and no other authority | `application_agent_invocation_host` |
| The advertised MCP tool list equals the source enumeration exactly | `application_mcp_context_helper` |
| Every MCP refusal — connect, bind, escape, symlink, and each write/draft/publish/note name — is refused by its own cause | `application_mcp_context_helper` |
| The helper child's environment is exactly the two allowlisted names while the parent holds secrets | `application_mcp_context_helper` |
| A moved head makes every `tools/call` stale and re-reads nothing | `application_mcp_context_helper` |
| The shipped helper links no live GitHub mutation | `live_mutation_isolation` |
| The side-chat panel discloses ten fields, each moving alone, and emits only intents | `qt_side_chat_panel` |
| Neither new widget can hold a launcher, process, credential, endpoint, or store | `qt_side_chat_panel`, `qt_finding_queue_view` |
| The queue offers exactly six actions, each emitting one intent naming the exact finding | `qt_finding_queue_view` |
| The queue shows the anchor state as its own field | `qt_finding_queue_view` |
| The workshop shows original and proposal as two separate surfaces; rejecting leaves the original byte-identical | `qt_review_draft_editor` |
| One real gesture reaches one delivered finding and one local draft, with zero GitHub mutation calls | `application_agent_finding_composition_contract` |
| A full-agent launch records zero GitHub mutation calls and has no second edge to the mutation coordinator | `application_agent_finding_full_agent_contract` |
| A publication does round-trip through the one RFC-BPR-0008 path | `application_review_round_trip_composition_contract` |
| Each composed host has a construction cut that keeps the gesture and takes its own effect to zero | `application_agent_finding_finding_host_construction_cut_contract`, `application_agent_finding_workshop_construction_cut_contract`, `application_agent_finding_mcp_construction_cut_contract`, `application_agent_finding_chat_panel_construction_cut_contract`, `application_agent_finding_queue_construction_cut_contract` |
| All five constructions are required anchors, and the gate fails when one is dropped | `composed_application_hosts` |
| The Qt adapter still starts no process | `architecture_boundaries` |
| New drawer content stays downstream of the composed window | `qt_composed_github_scale_100` |

### What RFC-BPR-0012 does not claim

**Stated limitations.**

- Confidence is not evidence. It is carried and displayed, and no branch
  anywhere reads it to decide a verdict, a promotion, or an enablement.
- An anchor that does not validate is shown as unanchored. BetterPullReview
  does not guess a nearby line, and a finding whose anchor fails is still
  presented in full rather than hidden.
- BetterPullReview records what supplemental context it sent. It **cannot**
  claim to audit every item a full agent independently accessed — the agent has
  its own GitHub access and its own tools, which is the point of the
  arrangement.
- Every process contract here uses the hostile local canary, not an installed
  Codex or Claude. A smoke run against a real installation remains optional
  extra validation rather than a gate.
- No live GitHub call is made anywhere in this RFC's evidence. The full-agent
  round trip is proven against the RFC-BPR-0008 fixture synchronization path,
  where a backchannel would still be detectable as a mutation call outside it.
- A single maximal finding exceeds the 48 KiB payload bound and is refused as
  oversize rather than truncated. That is the deliberate direction to fail in,
  and it is a real limit on how much one finding may say.
- The helper is built and proven, but packaging or distributing it to an
  external agent is outside RFC-BPR-0012.

**Not delivered.** Named here rather than left to silence:

- **Delivered after this list was first written**, and struck from it rather
  than left to contradict the tree: the panels sit in the conversations
  drawer ([[BPR-0513]]), a send reaches one agent turn through the composed
  side-chat session host ([[BPR-0514]]), and the typed message rides the
  input frame ([[BPR-0517]]).
- **The frozen snapshot carries alignment facts, not file bytes.** The helper's
  `binding` and `alignment` records are written from the same values the finding
  validator uses, and `files/` is created empty, so `list_snapshot_files`
  answers with an empty list rather than a wrong one. Freezing file content is a
  larger effect than the composition slice owned.
- **The six comment-workshop operations are one typed set on one path.**
  `core::workshop_operation` names them in the UX guide's order — clarify,
  shorten, make actionable, phrase as a question, make more direct, and check
  the premise — with a pinned count. The operation rides as an optional field
  of the agent supplement, admitted only under the `workshop_comment` entry
  point, reaches the agent's wire frame in one position, and is named by the
  proposal `application::agent_comment_workshop_host::propose` produces; the
  revision editor offers the six as controls that emit a typed workshop
  request. A press starts one agent turn through the same invocation path
  every entry point uses, bound to the press by the session identity the
  composition allocates, and the turn's converted outcome is the proposal
  that names the operation; an unrelated outcome cannot spend the press.
  There is still one shipped path from a draft to a before/after proposal
  and one entry point (BPR-0533).
- **A workshop press is a private run from a recorded revision (BPR-1245).**
  RFC-BPR-0023 refuses mutable note or draft context on every run, so the
  press does not send the draft. The composition asks
  `private_review_workflow::record_workshop_press` on the private store's
  effect lane, after any draft change already queued there;
  `application::private_review_application_host::record_workshop_press`
  reads the stored draft and commits its text as one immutable reviewer
  revision on a new thread whose subject is the draft's line, decided by
  `core::create_private_thread`. It returns a `findings` run intent whose
  only source is that revision. The press dispatches through the same
  launch path with `workshop_comment` and its operation. At admission,
  `core::admit_workshop_run_sources` refuses a workshop run outside a
  composition unless it has exactly one source and that source is a
  complete, current reviewer turn revision; the host also requires the
  `findings` purpose, no chat message and a named operation. A refused
  recording dispatches nothing and settles the press with its reason.
- **The Windows backend is not re-proven here.** RFC-BPR-0012 adds no platform
  code; the Windows process lifecycle remains claimed only on the strength of
  BPR-0240's 2026-08-26/27 target-native runs, exactly as the RFC-BPR-0011
  sections above say.

## RFC-BPR-0023 runtime facts and input admission

M5 extends the existing invocation/launch/decoder/result path. Requested
settings are explicit invocation inputs; configured settings are explicit
profile defaults. Both remain separate from protocol reports. The effective
wire setting uses the request when present, otherwise the default. Neither a
profile display name, executable path, assistant prose, stderr nor environment
value supplies a runtime fact. Content-self-reported settings remain absent:
none of these four transports has a typed content-self-report field.

### Tested protocol capability boundary

| Protocol | Requested/default settings sent | Reported facts admitted | Unknown facts |
|---|---|---|---|
| Codex App Server | `thread/start.params.model`; `turn/start.params.model` and `.effort` | `thread/start.result.model`, `.reasoningEffort`, `.thread.id`; `turn/start.result.turn.id` | Tool version, remote session/run, content-self-report |
| Codex JSONL exec | Model as `-m NAME` and effort as `-c model_reasoning_effort="LEVEL"` ([[BPR-1753]]; names `core::valid_claude_model` admits, levels `minimal`, `low`, `medium`, `high`, `xhigh`); others refuse as `unsupported_runtime_settings` | `thread.started.thread_id` | Model, effort, tool version, other remote IDs, content-self-report |
| Claude stream JSON | Model as `--model NAME` ([[BPR-1375]]; `core::valid_claude_model` names only); effort as `--effort LEVEL` ([[BPR-1753]]; `low`, `medium`, `high`, `xhigh`, `max`), any other refusing as `unsupported_runtime_settings` | `system/init.session_id`, `.model`, `.claude_code_version`; `assistant.message.model`; the result frame's `total_cost_usd`, a plain decimal only, as the run's reported cost | Effort, remote thread/turn/run, content-self-report |
| External terminal | Explicit settings refuse as `unsupported_runtime_settings` | None | All runtime facts |

These are field-specific observations, not provider-execution attestation.
App Server's thread result can describe its configured default before the turn
setting takes effect; its report retains that exact basis. `initialize`'s
`userAgent` is never scraped into a version. The adapter does not discover a
model catalog or promise that an agent supports every bounded model/effort
value; a tool-side rejection remains a failed protocol operation. The public
App Server contract describes the [request and result fields](https://learn.chatgpt.com/docs/app-server);
the Claude field mapping is grounded in the publisher's
[SDK 0.3.211 type declarations](https://unpkg.com/@anthropic-ai/claude-agent-sdk@0.3.211/sdk.d.ts).
The local canary fixtures exercise these envelopes; they do not qualify an
installed vendor release or infer facts absent from its messages.

A decoder admits metadata only after its normal phase and remote-identity
checks. Each distinct report retains its protocol/event/field basis. Identical
reports coalesce; conflicting requested, configured and reported values remain
separate and produce the core's typed conflicts. Scalars retain the M3 limits:
256 bytes per model/effort/version/basis, 1,024 per remote ID, at most 16 reports
and 16 KiB aggregate metadata. Empty, control-containing, invalid UTF-8,
wrong-type and oversized fields refuse the frame, without truncation. Missing
fields remain absent, including nullable App Server effort. Raw JSON and
credential-related neighboring fields are not persisted as provenance.

### One invocation snapshot and one bounded wire input

`agent_launch_options::AdmittedRun` accepts the store-issued durable run
snapshot. The builder validates its session, profile identity/display snapshot,
protocol, entry point, exact revision binding, requested/default facts and
ordered private source set. Each supplied full body must match its admitted
source reference, byte count and digest. Unadmitted, missing or changed private
bodies refuse. The request, stream events and terminal result carry the same
`agent_run_id`; reusing a session does not reuse its invocation identity.
Finalization can require the expected run, discarding foreign-run/session facts
while retaining same-run facts on revision staleness, cancellation and failure.

Private selection remains capped at 256 sources and 2 MiB raw body bytes. A
separate **8 MiB aggregate encoded stdin** admission counts supplement fields,
TSV escaping, selected full bodies, protocol JSON escaping, fixed wrappers and
App Server follow-ups before allocating the encoded context. App Server
reserves the maximum escaped size of a 1,024-byte remote thread ID. The process
adapter repeats admission before starting a child; the shared protocol loop
also checks the actual cumulative bytes before each write. An invalid late
thread ID can receive the initialization handshake, but receives no private
context. Neither layer clips the input to make it fit.

M5 supplied this launch snapshot contract. M6 implemented the durable host path
below, and M8 connected it to the production composition root and UI.
Storage schema 7 persists the typed facts; mutation-recovery remains unchanged.

### Durable private conversation host

`private_review_application_host` implements the invocation host's optional
`private_agent_run_port`. When configured, ordinary invocations require an
exact private-run intent; admission reloads store-owned source bodies and
commits the reviewer prompt and run before the existing workspace/process path.
The final source claim serializes edits and deletion through input delivery.
Only changed thread/revision/run/finding identities are posted to the decision
lane. A projection retrieves full content through bounded store queries.

The shared decoder retains complete assistant items: App Server deltas and
matching completed items, JSONL completed agent-message IDs, Claude assistant
text blocks, and terminal stdout including line endings. Identical completed
items coalesce; conflicts refuse. Claude's result summary is not appended as a
second answer. At most 256 items and 1 MiB assistant bytes are retained; the
existing frame/stdout bounds can impose a smaller representable answer.
The run stores item byte lengths and digest, while its assistant turn stores the
single concatenated body. Errors retain only validated incomplete text.

The host persists a correlated original response before the existing stale
presentation step. Explicit cancellation cannot be overwritten by a late
success. Admission failure starts no child; terminal storage failure reports
`private_storage_failure`, and bounded recovery marks remaining admitted runs
interrupted after producer quiescence. There is no automatic retry or rerun.
Finding replies preserve their original run/ordinal/digest as the thread subject.

The claim and SQLite store live on the invocation effect lane. SQLite's immediate
transaction temporarily excludes all writers to the ledger and returns `busy`
for contention; the process timeout bounds the claim. Raw selected-body vectors
are released before launch. The immutable encoded request remains bounded and
lives until process retirement; source exclusion releases at input acknowledgement.
The launcher creates no durable supplement copy.

M8 connects these APIs to the production composition root and drawer/workshop.
Until then, callers without the private run port keep their legacy zero-run path
and cannot supply private source bodies. The separate `WritingOnly` review-tour
path refuses private-run intents and keeps its existing restricted protocol.
M6 adds no GitHub capability, publisher or second process launcher.

## Review-tour writing boundary

An explicit tour field button captures one raw-text interval and permits one
local undoable replacement. This purpose-specific contract does not change the
ordinary comment workshop's separate acceptance flow. The selected approved
Claude stream JSON route disables tools, MCP, hooks, repository settings and
session persistence and rejects protocol frames that violate that mode. Other
unqualified protocols refuse writing assistance. This application contract and
its deterministic process canaries do not attest an installed vendor binary or
model's semantic quality.

### Tool-free writing on the person's own sign-in (BPR-1375)

Owner decision, 2026-10-09, verbatim: "no claude should use -p or the
equivalent, the free version cli api same as codex". Writing runs the installed
CLI non-interactively on the person's own subscription sign-in, never a paid
API key. For Claude that is:

```text
claude -p --input-format stream-json --output-format stream-json --verbose
  [--model NAME]
  --tools "" --disallowedTools "*" --strict-mcp-config --mcp-config '{"mcpServers":{}}'
  --setting-sources "" --disable-slash-commands --no-session-persistence
  --settings '{"disableAllHooks":true,"autoMemoryEnabled":false,
              "enabledPlugins":{"cc-plugin-agents-md@builtin":false,
                                "cc-plugin-telemetry@builtin":false,
                                "cc-plugin-plugin-authoring@builtin":false}}'
```

(`application::claude_writing_restrictions`. The line breaks are for
reading; the settings JSON is one argument.) There is no `--bare`:
its own help says it reads only `ANTHROPIC_API_KEY` or an `apiKeyHelper` and
never OAuth or the keychain, so with it a person signed in to their Claude
account could never write a tour. What `--bare` also skipped is held off
another way, each checked against Claude Code 2.1.296 on 2026-10-09 with a
loopback stand-in for the Messages API and a debug log, in a folder holding a
planted `CLAUDE.md`, `AGENTS.md`, `.claude/settings.json` (a hook) and
`.mcp.json` (a server):

| `--bare` skipped | Held off by | Observed |
|---|---|---|
| `CLAUDE.md` auto-discovery | `--setting-sources ""`, and the empty folder below | Neither planted file reached the system prompt or messages |
| Hooks | `disableAllHooks`, no settings source | The planted hook did not run |
| Plugin sync and the built-in plugins | `enabledPlugins` off for the three built in; no settings source names a marketplace | `plugins: []`; without it the built-in telemetry plugin started `sh` in the working folder |
| Auto-memory | `autoMemoryEnabled: false` | No memory section in the prompt; no `memory_paths` in the init frame |
| MCP servers, claude.ai connectors | `--strict-mcp-config` with an empty configuration | `mcp_servers: []`, signed in with a Claude account too |
| LSP | No plugin, no settings source | Zero LSP servers |
| Background prefetches | Not held off | Claude still fetches its own bootstrap from Anthropic, on the same sign-in |
| Keychain reads | Wanted: this is the sign-in | `apiKeySource: "none"`, the account answers |

The process starts in an empty folder of its own (`bpr-writing-XXXXXX` under
the temporary folder), made by the agent adapter and removed when the request
ends, never the review workspace, so nothing from the reviewed repository is
where Claude starts and nothing left there outlives the request. The decoder
refuses, before any text is read, an init frame that reports a tool, an MCP
server, a plugin or an auto-memory folder, so a plugin a later Claude builds
in stops the request rather than riding along. It reads past `system` frames
other than init and `rate_limit_event`, which 2.1.296 writes around a turn. The
child environment is the profile allowlist, `USER` included (above). Claude
writes its own state under `HOME` as it does for any run (`~/.claude.json`, an
empty `~/.claude/sessions`), and in print mode it opens a same-user socket
under `/tmp/cc-socks/` for its own messaging; neither is in the working folder
and neither is BetterPullReview's.

**Codex is not offered.** Codex CLI 0.162.1 (and 0.155.1 before it) cannot
be made tool-free for any model a ChatGPT account lists: every listed model
is `code_mode_only` with multi-agent tools. With `--ignore-user-config`, a
read-only sandbox, an empty `mcp_servers`, web search disabled and every
feature flag that names a tool turned off (`shell_tool`, `unified_exec`,
`multi_agent`, `multi_agent_v2`, `apps`, `plugins`, `view_image`,
`image_generation`, `browser_use`, `computer_use`, `goals`, `hooks`,
`tool_suggest`, `skill_search` and more), the request Codex sends still
declares `exec` (JavaScript that calls nested `apply_patch` and MCP resource
reads), `wait`, `request_user_input`, `request_user_input_async` and the six
`collaboration` tools (`spawn_agent` among them), observed on a loopback
stand-in for the Responses API on 2026-10-09. Neither `codex exec` nor the App
Server's `thread/start` takes a tool list. Codex writing stays refused before
any process starts until Codex can run with no tools.

Only bounded admitted context accompanies the request; credentials, general
private notes, unrelated drafts and linked-ticket retrieval are excluded. Current
editor/owner/hook/source generations must still match at delivery. Outline output
contains no comment bodies, Ready flags or publication authority; retained entries
need individual visible author completion. See the
[Settings guide](../development/settings.md#review-preparation-writing-hooks)
and [tour operator guide](../development/review-tours.md) for the user-facing
behaviour.

### Writing request and result bounds

Moved from the Settings guide. Writing hooks hold text instructions only and
grant no tool, publication or broader editing authority. Save and Restore
default do not run an agent. A saved-hook Test is one explicit, nonpublishing
request with the selected allowed agent on a synthetic sample bound to the open
review; it has no draft destination and changes no tour field. The Review
feedback hook writes no tour field, so its Test is unavailable.

A request captures one selection (including whitespace), otherwise its whole
field. Empty Problem/Solution fields first recover an unambiguous existing
description section, byte for byte, without inference. Other empty requests
need approved evidence; an empty stop also needs its exact currently available
pinned source. Context is collected only on that gesture: bounded author
description/comments, private brief text, surrounding prose and the selected
stop's code. No linked resource is fetched.

| Bound | Limit |
|---|---|
| Input selection | 6,000 UTF-8 bytes |
| Supplied evidence | 8,192 bytes |
| Complete outgoing agent message | 16,384 bytes |
| Returned replacement text | 16,384 bytes |
| Complete result | 32,768 bytes |

Over-limit requests ask for a smaller selection. Extra keys, invalid
provenance, malformed UTF-8, duplicate or stale responses cannot alter the
field; the response protocol must confirm an empty tool/MCP list, and tool
blocks and finding payloads are refused. One accepted result is one native
undo transaction: undo restores raw Markdown and selection, and redo uses the
received bytes without running the agent again. Private
Recovered/Inferred/Unclear notes stay outside the tour body. The outline hook
returns body-free suggestions.

The application's flags do not change the developer's installed global
configuration. The CLI flag contract is documented in the
[provider's CLI reference](https://code.claude.com/docs/en/cli-reference);
actual installed-provider and model-quality qualification is still required.

## Review Cycles and agents

Review Cycles ([RFC-BPR-0058](../rfcs/RFC-BPR-0058-headless-cycle-cli-and-portable-consumer-skill.md),
[RFC-BPR-0060](../rfcs/RFC-BPR-0060-removable-agent-adapters-and-capability-isolation.md))
need no agent. A frozen round is a complete assignment that a person or any
harness can act on from its Markdown alone, and a recipient's structured
response or a reply pasted by hand is recorded the same way whatever produced
it. BetterPullReview starts no model for a cycle except through Send to Agent,
and its claims about who answered stay claims.

There are three ways a round reaches a harness, each decided by the profile's
disclosure policy for its channel:

- **Copy, Save or `bpr cycle export`.** The reviewer hands the document over;
  nothing is started.
- **A grant (`bpr cycle round grant`).** A harness that can run `bpr` reads that
  one round and, unless the grant is read-only, appends a response to it. The
  [bpr-cycle skill](../development/cycle-cli.md#the-bpr-cycle-skill) teaches only
  those consumer commands, under `BPR_CYCLE_ROLE=consumer`, and starts no model
  of its own. The harness keeps its own tools, credentials and approvals.
- **Send to Agent.** In a build with agent integrations, the share sheet hands
  the round's document to one of the existing agent profiles above (Codex,
  Claude, or an external terminal harness, named by its profile) through the
  same supervised launcher,
  environment allowlist and host policy, in the cycle's bound folder, and
  records what was seen: accepted by the adapter, acknowledged with a session,
  or uncertain. Only Claude stream JSON continues a session, and only one an
  earlier send of the cycle reported. Nothing is retried or sent elsewhere, and
  nothing is captured or verified for the reviewer
  ([threat model](security-threat-model.md#review-cycles)).

A build with `BPR_BUILD_AGENT_INTEGRATIONS=OFF` compiles out the launcher, the
protocol decoders, the agent hosts, the MCP helper and Send to Agent, and its
window withdraws their controls; it still opens every stored cycle and every
record an agent wrote earlier. See
[release packages](../development/release-packages.md#packages-without-agent-integrations-or-local-automation).
