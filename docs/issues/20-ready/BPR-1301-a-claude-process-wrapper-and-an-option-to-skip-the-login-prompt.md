---
schema: "implement-rfc.card.v1"
id: "BPR-1301"
title: "A Claude process wrapper and an option to skip the login prompt for agent launches"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner request in chat, 2026-10-06, with a screenshot of the VS Code Claude Code extension settings","reservation":"docs/issues/_reservations/BPR-1301--BPR-1301--ddc91c168c32.json","reference":"https://code.claude.com/docs/en/vs-code#use-third-party-providers","owner_questions":["BPR-1301:q-sign-in-switch","BPR-1301:q-windows-scripts"],"implementation_note":"Use the frontend-design skill for the Settings rows; owner review is required","acceptance":["BPR-1301:wrapper-path","BPR-1301:wrapper-launch","BPR-1301:unset-unchanged","BPR-1301:launch-recheck","BPR-1301:scope","BPR-1301:persist","BPR-1301:sign-in","BPR-1301:agent-free","BPR-1301:documents","BPR-1301:owner-look"],"owner_answers":{"date":"2026-10-06","BPR-1301:q-sign-in-switch":"Accept (no switch)","BPR-1301:q-windows-scripts":"Accept (.exe only)"}}
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

## Boundaries

Host and Qt. The core profile registry and host policy are unchanged. No
`PATH` discovery of `claude` is added; that is a separate decision if wanted.
No environment-variables setting.
