---
schema: "implement-rfc.card.v1"
id: "BPR-1301"
title: "A Claude process wrapper and an option to skip the login prompt for agent launches"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner request in chat, 2026-10-06, with a screenshot of the VS Code Claude Code extension settings","reservation":"docs/issues/_reservations/BPR-1301--BPR-1301--ddc91c168c32.json","reference":"https://code.claude.com/docs/en/vs-code#use-third-party-providers","owner_questions":["BPR-1301:q-sign-in-switch","BPR-1301:q-windows-scripts"],"implementation_note":"Use the frontend-design skill for the Settings rows; owner review is required","acceptance":["BPR-1301:wrapper-path","BPR-1301:wrapper-launch","BPR-1301:unset-unchanged","BPR-1301:launch-recheck","BPR-1301:scope","BPR-1301:persist","BPR-1301:sign-in","BPR-1301:agent-free","BPR-1301:documents","BPR-1301:owner-look"],"owner_answers":{"date":"2026-10-06","BPR-1301:q-sign-in-switch":"Accept (no switch)","BPR-1301:q-windows-scripts":"Accept (.exe only)"}}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1301:wrapper-path","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"core_agent_wrapper and qt_claude_process_wrapper: absolute executable regular file (and through a symlink) accepted; relative, ~, missing, dangling link, directory, non-executable, pipe, NUL/line break, over 1024 bytes and Windows .bat/.cmd (and non-.exe, ADS) refused, each its own reason; application_claude_process_wrapper: a refused path is not saved. Controls: .cmd admitted, X_OK dropped, refused path saved each fail. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:wrapper-launch","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"qt_claude_process_wrapper through the real supervised launcher: a recording copy of the canary as wrapper gets today's argv[1..], whole environment, cwd and stdin bytes for agent panel, tool-free writing, round send and --resume round, nothing prepended, no added name; the shipped bare claude fails to start as the in-test control. Composed: application_review_tour_writing_wrapper and application_review_cycle_send_public_path. Control: no substitution fails 3 rows. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:unset-unchanged","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"application_claude_process_wrapper: with no wrapper the Claude request reaches the launcher equal to the built request and nothing is inspected; qt_claude_process_wrapper: built shipped request equals today's but for the executable. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:launch-recheck","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"qt_claude_process_wrapper: a saved wrapper made chmod -x, then deleted, ends failed_to_start with 'Claude process wrapper PATH: reason'; the counting port shows no process started, bare claude included. Control: recheck removed fails 2 rows. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:scope","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"application_claude_process_wrapper and qt_claude_process_wrapper: Codex JSONL, App Server and external terminal keep their executable with a wrapper saved; host policy and core profiles untouched (the port sits after policy). macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:persist","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"sqlite_workbench_settings_storage: schema 14 round trip and reopen, 13->14 migration, legacy and window writes keep it; application_workbench_preferences_consumers: owned adoption keeps it; composed writing course: path survives restart. Per-profile by each window's own profile subscription; purge removes workbench-settings.sqlite3. Controls: legacy store drop and adoption drop fail. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:sign-in","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"Owner Q1 'Accept (no switch)': no switch built; qt_settings_window asserts the one Sign-in line and no extra check box. Look pending in BPR-1301:owner-look. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:agent-free","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"build/dev-agent-free builds; 16/16 architecture_boundaries, settings, preferences, round-trip, cycle-send rows; no wrapper object or symbol; the page and rows are withdrawn (qt_settings_window withdrawal check); stored value carried unread through preference writes. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:documents","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"ai-agent-boundary.md and security-threat-model.md record trust, GHES and tool-free consequences and unchanged environment; settings docs updated; DOCUMENTATION_OK. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:owner-look","outcome":"pending","revision":"b07a3f44d","evidence":{"kind":"observation","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"Pending: attended session prepared (see card); one Accept or Reject per row."}}]
---

# BPR-1301: A Claude process wrapper and an option to skip the login prompt for agent launches

## Origin

The owner, in chat on 2026-10-06, quoted exactly:

> For agent integration I want to be able to support the same style as the
> vscode claude plugin where i can set the claude process wrapper and disable
> the login prompt as per
> https://code.claude.com/docs/en/vs-code#use-third-party-providers

Their screenshot shows two VS Code settings:

- **Claude Code: Claude Process Wrapper**, "Executable path used to launch the
  Claude process" (a text field);
- **Claude Code: Disable Login Prompt**, "When true, never prompt for
  login/authentication in the extension. Used when authentication is handled
  externally" (a checkbox).

## What the two VS Code settings do

Read from the linked page on 2026-10-06 (the extension settings table and
"Use third-party providers"):

- `claudeProcessWrapper`: "Executable used to launch the Claude process. The
  bundled binary path is passed as an argument when present. Set this to a
  separately installed `claude` binary if the extension build doesn't include
  one for your platform." So the extension runs the wrapper in place of its
  bundled `claude`, with the bundled path as an extra first argument when it
  has one. A wrapper is either a different `claude` or a script that sets up
  the environment (provider variables, credentials, a gateway) and then runs
  Claude. With a wrapper set the extension also skips some of its own steps
  (the "open somewhere else" check, the starting permission mode); BPR has
  neither, so those have no counterpart here.
- `disableLoginPrompt`: "Skip authentication prompts (for third-party provider
  setups)". It is the first step of "Use third-party providers": for Amazon
  Bedrock, Google Cloud's Agent Platform or Microsoft Foundry, check Disable
  Login Prompt, then configure the provider in `~/.claude/settings.json` "so
  your settings are shared between the VS Code extension and the CLI". It
  affects only the extension's own sign-in screen: with Anthropic sign-in
  absent, the extension would otherwise ask for it before any session starts.

The doc does not ask the host to pass any extra environment for a third-party
provider. Provider configuration lives in `~/.claude/settings.json`, which
Claude finds through `HOME`, and a wrapper script sets whatever else it needs
itself. (The extension's separate `environmentVariables` setting was not
requested.)

## How BPR launches Claude today

Read at `origin/main` 810108584 / 91b53d528.

1. **Profile and executable.** The Claude profile is one of four immutable
   built-ins: `claude_stream_json_profile_id`, display name "Approved Claude
   stream JSON", `Executable = "claude"`, the stream-JSON protocol and a
   disposable review workspace (`src/core/src/agent_profile.cpp:157-164`). A
   profile has no credential field by design
   (`src/core/include/bpr/core/agent_profile.hpp:82-116`). Composition takes
   the built-ins unchanged (`src/app/src/github_application.cpp:7046`); only
   a tests-enabled build may replace an executable, through fixture options
   (`src/app/src/github_application.cpp:7060-7083`, under
   `BPR_BUILD_TESTING`).
2. **No discovery.** Nothing resolves `claude` against `PATH`; showing the
   panel "performs no executable discovery" and a missing executable becomes
   `failed_to_start` only after a start gesture
   (`docs/architecture/ai-agent-boundary.md:17-22`), in line with "Resolve
   executables explicitly" (`docs/architecture/security-threat-model.md:280`).
   By code reading, the bare name cannot start at all in a shipped build: the
   Unix supervisor refuses an `argv[1]` that does not begin with `/`
   (`src/adapters/agent/src/unix_agent_supervisor.cpp:82`) and runs it with
   `execv`, which does no `PATH` search (`:129`); Windows passes it as
   `CreateProcessW`'s application name (`src/adapters/agent/src/windows_agent_process.cpp:925`,
   `:996-997`), which neither searches `PATH` nor adds `.exe`. Not run. A
   wrapper path is therefore also the first way a person can name a real
   Claude, which is the second use the VS Code doc gives it.
3. **Arguments.** Every Claude launch is `-p --input-format stream-json
   --output-format stream-json --verbose` (`src/application/src/agent_launch.cpp:2237-2243`);
   a tool-free writing launch appends `--tools "" --disallowedTools "*"
   --strict-mcp-config ... --bare --disable-slash-commands
   --no-session-persistence --settings {"disableAllHooks":true}` (`:2248-2251`);
   a Review Cycle send adds `--resume NAME` when continuing (`:2310-2316`).
   Context goes on stdin only, never the command line.
4. **Login and auth.** BPR has no sign-in prompt, sign-in check or sign-in
   state for any agent. A search of `src/` for login, sign-in and
   authentication text finds only GitHub sign-in. The boundary document says
   the agents "use their own authentication ... exactly as they would when
   launched directly. BetterPullReview does not proxy model traffic or
   provide agent credentials" (`docs/architecture/ai-agent-boundary.md:11`).
   A Claude that is not signed in answers the run with its own error, which
   BPR shows as an ordinary failed or nonzero result.
5. **Environment.** The child environment starts empty and receives only the
   profile's allowlisted names (`src/adapters/agent/src/agent_process_launcher.cpp:363-368`,
   `src/application/src/agent_launch.cpp:1793-1809`): `HOME`, `PATH`, `LANG`,
   `LC_ALL`, `TMPDIR`, `TEMP`, `TMP` (`src/core/src/agent_profile.cpp:163`),
   read from BPR's own environment (`src/app/src/github_application.cpp:7120-7123`).
   Names containing CREDENTIAL, API_KEY, ACCESS_KEY, PRIVATE_KEY or the words
   AUTH, TOKEN, SECRET, PASSWORD and the rest are refused even if a profile
   lists them (`src/core/src/agent_profile.cpp:52-135`).
6. **Launch restriction.** On Unix a bundled supervisor runs the agent in its
   own session and process group, with only stdin/stdout/stderr and its fixed
   lifetime descriptors (`src/adapters/agent/src/agent_process_launcher.cpp:391-426`).
   On Windows the child is created suspended, put in a kill-on-close Job
   Object, then resumed, and inherits exactly the three standard handles
   (`src/adapters/agent/src/windows_agent_process.cpp:975-981`); a guarded
   workspace launch goes through the Windows supervisor with the same
   executable (`:769-858`).
7. **AppContainer and sandbox.** Neither contains a shipped agent launch. The
   macOS `sandbox-exec` profile, the Linux network namespace and the Windows
   AppContainer belong to the automated-test boundary
   (`cmake/BprTestIsolation.cmake:451-469`, `:604`); the agent adapter only
   grants a contained test process's AppContainer SID on its workspace guard
   (`src/adapters/agent/include/bpr/adapters/agent/windows_workspace_guard.hpp:91-125`).
8. **Settings > Agents & context.** One page: the allowed profile for the
   next request, private notes, the private draft, the existing-checkout
   switch, the request scope and the writing hooks
   (`src/adapters/qt/src/workbench_window.cpp:3164-3217`). Its explanation
   says these choices "are not saved as global defaults". Nothing on it
   names an executable. Persisted per-profile settings live in
   `workbench_profile_preferences`, beside `WritingHooks`
   (`src/application/include/bpr/application/workbench_runtime_ports.hpp:696-701`).
9. **Agent-free builds.** `BPR_BUILD_AGENT_INTEGRATIONS=OFF` (`CMakeLists.txt:54`;
   the `*-agent-free` presets in `CMakePresets.json:114-127`, `:167-179`)
   builds no launcher, agent host, MCP helper or round sender, and the page
   is withdrawn (`withdraw_agent_surfaces`, `src/adapters/qt/src/workbench_window.cpp:9248-9308`).

## Design

**Claude process wrapper.** A per-profile setting on Settings > Agents &
context: "Claude process wrapper", an executable path, empty by default.

- Accepted only as an absolute path to an existing regular file the user may
  execute (a symbolic link is accepted when it ends at one), with no NUL or
  line break and at most `maximum_agent_text_bytes`. It is a path, never a
  command line: no splitting, quoting, `~` or variable expansion, and never
  run through a shell. A refused value is not saved, and the row says why in
  one line. Choose... opens a native file chooser.
- When set, every Claude stream-JSON launch runs the wrapper in place of
  `claude`: the agent panel, side chat, comment workshop, tool-free writing
  and Review Cycle Send. The wrapper gets exactly Claude's arguments. VS Code
  adds its bundled binary's path as a first argument "when present"; BPR
  bundles no Claude and discovers none, so nothing is added and a wrapper
  that is itself a `claude` binary works unchanged. Working directory, stdin
  context, the allowlisted environment, the supervisor, process group, Job
  Object and three-handle list are the ones Claude gets today.
- One substitution point in the host, applied from the current profile's
  preference when the launch request is built. The core profile registry
  stays immutable and the host policy keeps deciding by profile identity, so
  the GHES rule ("approved Claude or the external terminal") is unchanged.
- The path is checked again at launch. A wrapper removed or no longer
  executable ends that request as `failed_to_start`, naming the wrapper; it
  never falls back to the bare `claude`.
- Codex and the external terminal are unaffected.

**Don't prompt to sign in.** Requested as a switch that suppresses BPR's own
login or authentication prompt or check. BPR has none (inventory 4), so the
switch would change nothing. The owner accepted question 1: no switch is
built, and the page says in one line, under the wrapper, that sign-in is left
to Claude and the wrapper.

## Security

- The wrapper is code the person chose, at the same trust as choosing which
  `claude` binary to run, and BPR does not verify that it runs Claude. It
  runs under the "Approved Claude" profile, so on an explicit GHES host the
  "approved corporate harness" is whatever that person's wrapper runs. The
  tool-free writing restrictions are Claude flags; a wrapper can drop them,
  so that mode is only as tool-free as the wrapper. Record all three in the
  threat model and `ai-agent-boundary.md`.
- No environment widening. The wrapper receives the same allowlist and
  nothing more; provider configuration is in `~/.claude/settings.json`
  (reached through `HOME`) or set by the wrapper itself. The doc requires no
  extra names, so there is no owner question here. A wrapper that wants
  `AWS_PROFILE` or a gateway URL sets it inside the script.
- Never a shell. A Unix script with a `#!` line is started by the kernel from
  the file, which is not a shell string. Windows `CreateProcessW` runs
  `.bat` and `.cmd` through `cmd.exe`, so those are refused (owner question 2).
- The path is profile data, not a credential; it is shown and stored as
  typed and never logged with arguments or environment.

## Owner questions

1. **BPR-1301:q-sign-in-switch.** BPR never asks an agent to sign in and
   never checks whether Claude is signed in: sign-in is Claude's own, and a
   Claude that is not signed in reports it as a failed run. A "Don't prompt
   to sign in" switch would therefore do nothing. **Accept:** build no
   switch; the Agents & context page says in one line under the wrapper that
   sign-in is left to Claude and the wrapper. **Reject:** say in the note
   what the switch should change in BPR.
2. **BPR-1301:q-windows-scripts.** On Windows a `.bat` or `.cmd` wrapper can
   only run through `cmd.exe`, which is a shell. **Accept:** on Windows the
   wrapper must be an `.exe` (or another directly executable image) and
   `.bat`/`.cmd` are refused with a one-line reason. **Reject:** say in the
   note how a Windows script wrapper should be started.

## Owner answers

The owner answered both questions in chat on 2026-10-06, verbatim:

- **BPR-1301:q-sign-in-switch:** "Accept (no switch)". No switch; one line on
  Agents & context says sign-in is left to Claude and the wrapper.
- **BPR-1301:q-windows-scripts:** "Accept (.exe only)". A Windows wrapper must
  be an `.exe`; `.bat` and `.cmd` are refused with a one-line reason.

## Acceptance

- **BPR-1301:wrapper-path.** Validation accepts an absolute path to an
  existing user-executable regular file (also through a symlink) and refuses
  relative, `~`-prefixed, missing, directory, non-executable, NUL or line
  break, over-length and (per question 2) Windows script paths, each with its
  own reason; a refused value is not saved. Unit-tested with a failing-before
  control.
- **BPR-1301:wrapper-launch.** Through the real launcher, a fake wrapper that
  records its argv, environment, working directory and stdin receives
  exactly what the fake `claude` receives today for an agent-panel run, a
  tool-free writing run and a Review Cycle send, with no added argument and
  no added environment name. Shown failing before the change.
- **BPR-1301:unset-unchanged.** With no wrapper set the launch request is
  equal to today's for every Claude entry point.
- **BPR-1301:launch-recheck.** A wrapper deleted or made non-executable after
  saving ends the request as `failed_to_start` naming the wrapper, and no
  process named `claude` is attempted.
- **BPR-1301:scope.** Codex and external-terminal launches and the host
  policy verdicts are unchanged with a wrapper set.
- **BPR-1301:persist.** The wrapper is stored per application profile,
  survives restart, follows a profile switch, and profile purge removes it.
- **BPR-1301:sign-in.** As question 1 decides: either the one-line statement
  on the page, or the switch with its decided effect and a test that fails
  when the switch is ignored.
- **BPR-1301:agent-free.** An agent-free build has neither row and still
  builds and passes `architecture_boundaries`; a stored wrapper value is
  kept unread, not deleted.
- **BPR-1301:documents.** `security-threat-model.md` and
  `ai-agent-boundary.md` record the wrapper's trust, the GHES and tool-free
  consequences and the unchanged environment; the documentation gate passes.
- **BPR-1301:owner-look.** The owner accepts the Agents & context rows in an
  attended session, one Accept or Reject per row. Required, not
  self-certified.

## Implementation (2026-10-06, b07a3f44d)

- **Core** `core::inspect_agent_wrapper_text` (`src/core/src/agent_wrapper.cpp`)
  reads the text under POSIX or Windows rules: control character, over 1,024
  bytes, `~`, not absolute, Windows `.bat`/`.cmd` (`cmd.exe`), any other
  non-`.exe` or an alternate data stream, each its own one-line reason.
- **Agent adapter** `adapters::agent::inspect_claude_process_wrapper` adds the
  file: missing (or a dangling link), a folder, not a regular file, and on
  Unix `access(X_OK)`. Metadata only.
- **Host** `application::wrapped_claude_process` is the one substitution point,
  in front of the supervised launcher (in front of the tests-on probe) for the
  agent host and for Review Cycle Send. Claude stream-JSON only; it inspects
  the saved path again and replaces the executable and nothing else, or ends
  the request `failed_to_start` with "Claude process wrapper PATH: REASON" and
  starts nothing. `claude_wrapper_editor` saves only what the adapter accepted
  (inspection on the effect lane, write on the decision lane, latest request
  only, joins window quiescence).
- **Storage** `workbench_profile_preferences::ClaudeWrapper`, workbench store
  schema 14 (`claude_wrapper` TEXT, CHECK ≤ 1024 bytes, refused at admission
  with a control character); version 13 migrates to none. The legacy layout
  write, the owned-preferences adoption and every other preference write carry
  it.
- **Qt** `qt::claude_wrapper_panel` on Agents & context: heading Claude, rows
  Process wrapper (field, Choose…, Save; answer under the field, quieter
  explanation under that) and Sign-in (one line). Withdrawn with the page in
  an agent-free build. The page's opening paragraph now says "The request
  choices here", since the wrapper is saved.
- **Documents** `ai-agent-boundary.md` (Claude process wrapper),
  `security-threat-model.md` (Claude process wrapper (BPR-1301), process
  handling), `settings.md`, `settings-authority-inventory.md`.

### Evidence (macOS, build/dev at b07a3f44d)

- `core_agent_wrapper`, `application_claude_process_wrapper`,
  `qt_claude_process_wrapper` (real launcher; a copy of the canary as the
  wrapper receives today's argv[1..], whole environment, cwd and stdin bytes
  for an agent-panel run, a tool-free writing run, a round send and a
  `--resume` round, with the shipped bare `claude` failing to start as the
  in-test control; symlink accepted; relative, `~`, missing, dangling,
  directory, non-executable, pipe refused; a `#!` script wrapper runs; deleted
  and `chmod -x` wrappers end `failed_to_start` naming the wrapper with no
  process started; Codex JSONL, App Server and external terminal keep their
  executable), `sqlite_workbench_settings_storage` (round trip, 13→14
  migration, legacy and window writes keep it across reopen, over-long, line
  break and NUL refused at admission), `application_workbench_preferences_consumers`
  (owned adoption keeps it), `qt_settings_window` ×4 scales (rows, refusal
  text, Return, search, withdrawal), `application_review_tour_writing_wrapper`
  (composed: bare `claude` changes nothing, relative path refused in the row,
  the saved wrapper runs the restricted writing request, the path survives a
  restart), `application_review_cycle_send_public_path` (Send to Agent runs the
  wrapper with Claude's exact arguments and the round's bytes). 85/85 focused
  agent, settings, preferences, round-trip, composed, cycle and canary rows.
- Failing-before controls, each a one-line mutation rebuilt and run: no
  substitution (3 rows fail), no launch recheck (2), `.cmd` admitted (core),
  legacy store drops it (SQLite), owned adoption drops it (consumers), a
  refused path saved (host), no `X_OK` check (launcher), composition never
  sets the source (writing course), Send unwrapped (cycle send course).
- Agent-free `build/dev-agent-free`: builds; 16/16 architecture, settings,
  preferences, round-trip, cycle send and isolation rows; no wrapper object or
  symbol in the graph.
- `coding-format-check` passes; clang-tidy finds nothing on the new
  translation units or changed lines. Round-trip pins resealed with notes.
  `DOCUMENTATION_OK`.
- **Persist across a profile switch** is structural: each window subscribes to
  its own profile's preferences host and sets its own source; the existing
  sibling-profile row of `application_workbench_preferences_consumers` shows a
  write to one profile leaves another untouched. Profile purge removes
  `workbench-settings.sqlite3`, which holds it.

### Owner look (BPR-1301:owner-look, pending)

Session `38af68502d74f85ae4f3508a5a043cb2` (new; not cbd48b91), state
`/Users/jaeger/bpr1301-attended-state/session` against the main checkout,
host detached (pid 58216, loopback port 59771), cursor 6 after preparation.
Two candidates, one Accept or Reject each: `BPR-1301-wrapper-row-v001`
(fingerprint `fe2286645ebc…`) and `BPR-1301-sign-in-line-v001`
(`825e798852ea…`). The frozen build of b07a3f44d, its source and build tree
live in `/Users/jaeger/bpr1301-wrapper-look-v001.mLOVhj`; `python3
/Users/jaeger/bpr1301-wrapper-look-v001.mLOVhj/launch.py` checks the build,
the fixture tree and the samples, opens Settings over Home on a temporary
github.com fixture profile and reports an exit within 8 seconds (smoke-tested
offscreen: up for 24 seconds). Samples to paste:
`/Users/jaeger/bpr1301-wrapper-look-v001.mLOVhj/samples/{claude-wrapper,not-executable,a-folder}`.
Next: `status --after 6` on that state; two Accepts satisfy :owner-look and
the card finishes; a Reject returns with its note for a v002.

### Published commits

The rebase before publication renamed b07a3f44d (the code, the frozen look's
build) to 14643f6a7 over origin/main 4ba7b3fbe; its round-trip manifest pin
was resealed over BPR-1068's. Merged tree: 23/23 focused wrapper, settings,
preferences, writing, cycle-send, round-trip, architecture and overview rows;
DOCUMENTATION_OK.

### Open decision, not built

With no wrapper saved, a shipped build cannot start Claude (bare `claude`, no
`PATH` search, the Unix supervisor requires `/`). Resolving `claude` on `PATH`
when no wrapper is set is a policy change to "Resolve executables explicitly"
and this card's boundary, so it is put to the owner, not built.

### Owed station runs (not blocking)

Linux and Windows have not run this change. At `origin/main` holding it, no
remote mutation:

- `bpr-main-linux`: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure -R '^(core_agent_wrapper|application_claude_process_wrapper|qt_claude_process_wrapper|qt_settings_window.*|sqlite_workbench_settings_storage|application_workbench_preferences_consumers|application_review_tour_writing.*|application_review_cycle_send_public_path|qt_agent_launch_canary|application_review_round_trip_composition_contract|architecture_boundaries)$'`
- `bpr-main-win`: `cmake --preset windows-dev && cmake --build --preset windows-dev && ctest --preset windows-dev -j4 --output-on-failure -R` with the same expression. Windows adds the `.cmd`, `.bat`, `WRAPPER.CMD` and `.ps1` refusals on real files and the `.exe` copy as wrapper.

## Boundaries

Host and Qt. The core profile registry and host policy are unchanged. No
`PATH` discovery of `claude` is added; that is a separate decision if wanted.
No environment-variables setting.
