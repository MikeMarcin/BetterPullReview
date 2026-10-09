---
schema: "implement-rfc.card.v1"
id: "BPR-1301"
title: "A Claude process wrapper and an option to skip the login prompt for agent launches"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner request in chat, 2026-10-06, with a screenshot of the VS Code Claude Code extension settings","reservation":"docs/issues/_reservations/BPR-1301--BPR-1301--ddc91c168c32.json","reference":"https://code.claude.com/docs/en/vs-code#use-third-party-providers","owner_questions":["BPR-1301:q-sign-in-switch","BPR-1301:q-windows-scripts","BPR-1301:q-path-search"],"implementation_note":"Use the frontend-design skill for the Settings rows; owner review is required","acceptance":["BPR-1301:wrapper-path","BPR-1301:wrapper-launch","BPR-1301:unset-unchanged","BPR-1301:launch-recheck","BPR-1301:scope","BPR-1301:persist","BPR-1301:sign-in","BPR-1301:path-lookup","BPR-1301:agent-free","BPR-1301:documents","BPR-1301:owner-look"],"owner_answers":{"date":"2026-10-06","BPR-1301:q-sign-in-switch":"Accept (no switch)","BPR-1301:q-windows-scripts":"Accept (.exe only)","2026-10-07":{"BPR-1301:q-path-search":"Yes, search PATH (Recommended)"},"2026-10-08":{"BPR-1301-wrapper-row-v002":"Accept","BPR-1301-sign-in-line-v002":"Reject: Cant read this"}}}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1301:wrapper-path","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"core_agent_wrapper and qt_claude_process_wrapper: absolute executable regular file (and through a symlink) accepted; relative, ~, missing, dangling link, directory, non-executable, pipe, NUL/line break, over 1024 bytes and Windows .bat/.cmd (and non-.exe, ADS) refused, each its own reason; application_claude_process_wrapper: a refused path is not saved. Controls: .cmd admitted, X_OK dropped, refused path saved each fail. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:wrapper-launch","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"qt_claude_process_wrapper through the real supervised launcher: a recording copy of the canary as wrapper gets today's argv[1..], whole environment, cwd and stdin bytes for agent panel, tool-free writing, round send and --resume round, nothing prepended, no added name; the shipped bare claude fails to start as the in-test control. Composed: application_review_tour_writing_wrapper and application_review_cycle_send_public_path. Control: no substitution fails 3 rows. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:unset-unchanged","outcome":"pass","revision":"ecb7dd771","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"Amended 2026-10-07 by the owner. application_claude_process_wrapper: with no wrapper the bare request reaches the launcher equal to the built one but for the found absolute executable; an absolute executable and no-locator requests pass through unchanged. v001 rows still pass at ecb7dd771. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:launch-recheck","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"qt_claude_process_wrapper: a saved wrapper made chmod -x, then deleted, ends failed_to_start with 'Claude process wrapper PATH: reason'; the counting port shows no process started, bare claude included. Control: recheck removed fails 2 rows. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:scope","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"application_claude_process_wrapper and qt_claude_process_wrapper: Codex JSONL, App Server and external terminal keep their executable with a wrapper saved; host policy and core profiles untouched (the port sits after policy). macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:persist","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"sqlite_workbench_settings_storage: schema 14 round trip and reopen, 13->14 migration, legacy and window writes keep it; application_workbench_preferences_consumers: owned adoption keeps it; composed writing course: path survives restart. Per-profile by each window's own profile subscription; purge removes workbench-settings.sqlite3. Controls: legacy store drop and adoption drop fail. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:sign-in","outcome":"pass","revision":"b07a3f44d","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"Owner Q1 'Accept (no switch)': no switch built; qt_settings_window asserts the one Sign-in line and no extra check box. Look pending in BPR-1301:owner-look. macOS; Linux and Windows station runs owed (commands on the card)."}},{"criterion":"BPR-1301:path-lookup","outcome":"pass","revision":"ecb7dd771","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"core_agent_wrapper (search order, PATH then usual folders, empty/relative/./~ entries and control characters skipped, bound, Windows claude.exe only); qt_claude_process_wrapper on real files and the real launcher (first absolute runnable claude wins past an empty folder and a folder named claude; relative entries never searched though each holds a claude; non-executable passed over; link used as found; usual folder after PATH; found claude gets Claude's exact argv, environment and stdin; removed before launch ends failed_to_start with nothing started); application_claude_process_wrapper (found, not found, wrapper wins without a search, refused wrapper never falls back, re-searched each launch, editor locate latest-only); qt_settings_window x4 (found line texts, read-only, hidden under a saved wrapper, looked up when shown, layout holds); application_review_tour_writing_wrapper composed (not found: nothing starts, Settings says so; claude on the course PATH runs the writing request and Settings names it; then the saved wrapper runs). Seven failing-before controls fail. Agent-free build unaffected. macOS; Linux and Windows station runs owed."}},{"criterion":"BPR-1301:agent-free","outcome":"pass","revision":"ecb7dd771","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"build/dev-agent-free at ecb7dd771 builds; 10/10 architecture, settings x4, preferences, storage, round-trip, cycle-send, core rows; no locate_claude, wrapped_claude_process or claude_wrapper_editor symbol in the app. macOS; Linux and Windows station runs owed."}},{"criterion":"BPR-1301:documents","outcome":"pass","revision":"ecb7dd771","evidence":{"kind":"command","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"security-threat-model.md ('Resolve executables explicitly' and the Claude process wrapper section) and ai-agent-boundary.md carry the dated owner amendment; settings.md and the authority inventory updated; DOCUMENTATION_OK at ecb7dd771."}},{"criterion":"BPR-1301:owner-look","outcome":"pending","revision":"7b79ecd80","evidence":{"kind":"observation","path":"docs/issues/40-parked/BPR-1301-a-claude-process-wrapper-and-an-option-to-skip-the-login-prompt.md","detail":"Process wrapper row: Accepted 2026-10-08 (BPR-1301-wrapper-row-v002, session ee2391ed, frozen build of ecb7dd771). Sign-in line: v002 Rejected 2026-10-08, 'Cant read this'; v003 (frozen build of 7b79ecd80) pending in session 0cccc3a1e0cdbcfe806df51a2b6f6fd0, one Accept or Reject for BPR-1301-sign-in-line-v003."}}]
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

- **BPR-1301:q-path-search**, asked in chat on 2026-10-07 after v001 (the
  open decision below): "Today a shipped BetterPullReview can't start Claude
  unless you set a wrapper path … When no wrapper is saved, should BPR find
  `claude` on your PATH itself, using and showing the absolute path it
  found?" The owner answered in chat on 2026-10-07, verbatim: **"Yes, search
  PATH (Recommended)"**.

## Owner amendment (2026-10-07)

From that answer. It amends this card's boundary ("No `PATH` discovery of
`claude` is added"), the criterion BPR-1301:unset-unchanged, and the
"Resolve executables explicitly" rule in
`docs/architecture/security-threat-model.md` (process handling, and its
Claude process wrapper section) and `docs/architecture/ai-agent-boundary.md`
(Claude process wrapper), where the same dated amendment is written:

- Used only when no wrapper is saved; a saved wrapper always wins, and a
  refused wrapper never falls back.
- Search the application's own `PATH` for `claude` (`claude.exe` on Windows;
  no `PATHEXT` scripts, consistent with ".exe only"). Take the first absolute
  executable regular file; ignore relative and empty entries and the current
  directory.
- Show the found absolute path in Settings, read-only ("Using
  /opt/homebrew/bin/claude (found on PATH)."), or "claude not found on PATH or
  in the usual install folders; set a wrapper."
- Resolve and check again at each launch; never run it through a shell.
- **macOS: a login-shell `PATH` would be needed, so a fixed fallback list is
  used instead (for the owner).** An application opened from the Finder or
  the Dock gets launchd's `PATH`. Observed here on 2026-10-07: a throwaway
  application bundle opened through LaunchServices (`open`) recorded
  `PATH=/usr/bin:/bin:/usr/sbin:/sbin`, `HOME=/Users/jaeger`;
  `launchctl getenv PATH` is empty. `claude` usually lives in
  `~/.local/bin` (the native installer), `~/.claude/local` (the older local
  install), `/opt/homebrew/bin` (Homebrew on Apple silicon, and npm's global
  folder under Homebrew's Node; this machine's is
  `/opt/homebrew/bin/claude` -> `/opt/homebrew/Caskroom/claude-code@latest/…`),
  `/usr/local/bin` (Homebrew on Intel, npm under the installer Node) or
  `~/.npm-global/bin` (npm's documented user prefix); an nvm Node puts it in a
  versioned folder no fixed list can name. None of those is on launchd's
  `PATH`, so a Finder-launched build would find nothing on `PATH`; only a
  login shell's `PATH` has them. No shell is spawned. After `PATH`, the
  search tries this fixed list in that order (`core::usual_claude_folders`),
  and Settings says "(found in a usual install folder, not on PATH)" when one
  of them is used. On Windows, where an application gets the user's whole
  `PATH`, the list is only `%USERPROFILE%\.local\bin\claude.exe` (the native
  installer). An nvm-installed `claude` still needs a wrapper or a `PATH` the
  application inherits (launched from a terminal).

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
  equal to today's for every Claude entry point. *Amended 2026-10-07 (owner
  amendment above):* equal to today's except that the bare `claude` is
  replaced by the absolute path found, and nothing else; a request already
  naming an absolute program is equal to today's.
- **BPR-1301:path-lookup.** (Owner amendment, 2026-10-07.) With no wrapper
  saved, a Claude launch of the bare `claude` runs the first absolute
  executable regular `claude` on the application's `PATH`, then in the fixed
  usual folders; relative, empty and current-directory entries are never
  searched; Windows takes only `claude.exe`; none found ends the request
  `failed_to_start` and starts nothing; the search repeats at each launch, so
  one removed between Settings and the launch is not run; a saved wrapper
  wins without a search. Settings shows the found path read-only, or says
  none was found. Unit, real-file, real-launcher and composed rows (found,
  not found, relative entries, wrapper wins, removed between save and launch)
  with failing-before controls; agent-free builds unaffected.
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

### Open decision (resolved 2026-10-07)

With no wrapper saved, a shipped build cannot start Claude (bare `claude`, no
`PATH` search, the Unix supervisor requires `/`). Resolving `claude` on `PATH`
when no wrapper is set is a policy change to "Resolve executables explicitly"
and this card's boundary, so it was put to the owner, not built. The owner
answered "Yes, search PATH (Recommended)" (BPR-1301:q-path-search); built in
v002 below.

## Implementation v002 (2026-10-07, baa098e4d and ecb7dd771)

From the owner amendment above.

- **Core** `core::usual_claude_folders(home, platform)` and
  `core::claude_search_candidates(PATH, folders, platform)`
  (`src/core/src/agent_wrapper.cpp`): the ordered places, PATH entries first,
  then the folders; empty, relative, `~` and control-character entries skipped,
  over-bound results skipped, nothing listed twice, `claude.exe` on Windows.
  `core::agent_search_path_variable()` names `PATH` beside the allowlist that
  already names it (`architecture_boundaries` reserves that literal).
- **Agent adapter** `adapters::agent::locate_claude`: the first candidate
  `inspect_claude_process_wrapper` accepts. Metadata only.
- **Host** `application::wrapped_claude_process` gains a `claude_locator`: no
  wrapper and a bare executable, it asks the locator at that launch and runs
  the found path, or ends `failed_to_start` with `claude_not_found_message`
  and starts nothing. A saved wrapper is used without a search; a refused one
  never falls back; an absolute executable is left alone.
  `claude_wrapper_editor::locate` asks on the effect lane and answers on the
  decision lane, latest only, for Settings.
- **Qt** `claude_wrapper_panel`: a read-only, selectable line under the status,
  `claudeWrapperFound`: "Using PATH (found on PATH).", "Using PATH (found in a
  usual install folder, not on PATH).", or "claude not found on PATH or in the
  usual install folders; set a wrapper." Hidden while a wrapper is saved;
  looked up whenever the row is shown or the wrapper cleared. The explanation
  ends "Leave it empty to use the claude found on PATH." A second commit
  (ecb7dd771) relays the rows out when a line changes: the first render showed
  the new line pushing the explanation under the Sign-in row.
- **Composition** `locate_claude()` on the window: the application's `PATH`
  (`qEnvironmentVariable` of the core name) and `QDir::homePath()`, read at each
  ask; given to the agent host's port, Send to Agent's port and the editor.
  Tests-on fixtures replace both with `ClaudeSearch` so no course finds the
  developer's own `claude`.
- **Documents** dated amendment in `security-threat-model.md` and
  `ai-agent-boundary.md`; `settings.md`, `settings-authority-inventory.md`.

### Evidence v002 (macOS, build/dev at ecb7dd771)

- Rows: `core_agent_wrapper`, `core_agent_profile`,
  `application_claude_process_wrapper`, `qt_claude_process_wrapper`,
  `qt_settings_window` x4, `application_review_tour_writing_wrapper`,
  `application_review_tour_writing`, `application_review_cycle_send_public_path`,
  `qt_agent_launch_canary`, `sqlite_workbench_settings_storage`,
  `application_workbench_preferences_consumers`,
  `application_review_round_trip_composition_contract`, `architecture_boundaries`:
  16/16 pass. What each shows is in the acceptance records above.
- The cases asked for: found (unit, real launcher, composed), not found (unit,
  composed: nothing starts and Settings says so), relative entries (core, real
  files with a `claude` in `.` and in a relative entry), wrapper wins (unit:
  the locator is not asked; composed: Settings hides the line), removed between
  save and launch (unit; real launcher: the found `claude` deleted before the
  launch ends `failed_to_start` with no process started).
- Failing-before controls, each a one-line mutation rebuilt and run: no lookup
  (application and real-launcher rows fail), not found falls back to the bare
  `claude` (2 fail), the found path beats a saved wrapper (application and
  composed fail), relative entries searched (core fails; the adapter row is
  held twice, since the wrapper text check also refuses a relative candidate),
  no file check (real-file and composed fail), found line shown over a saved
  wrapper (settings and composed fail), composition never searches (composed
  fails), no relayout (settings fails at all four scales).
- Agent-free `build/dev-agent-free`: builds; 10/10 architecture, settings,
  storage, preferences, round-trip, cycle send and core rows; no locator,
  wrapper port or editor symbol in the application.
- `coding-format-check` passes; clang-tidy finds nothing new on the changed
  sources. Round-trip pins resealed with a note. `DOCUMENTATION_OK`.
- macOS GUI `PATH`: an application bundle opened through LaunchServices
  recorded `PATH=/usr/bin:/bin:/usr/sbin:/sbin` (see the amendment).

### Owner look v002 (BPR-1301:owner-look; answered 2026-10-08, below)

The v001 session `38af68502d74f85ae4f3508a5a043cb2` was not answered; on the
owner's word it was released (`resume-development --all`, cursor 7) and its
frozen root and state folder were deleted.

Session `ee2391ed3fa000fb68deb48b43014024`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261007-bpr1301-wrapper-v002`
against the main checkout, host detached, cursor 6 after preparation. Two
candidates, one Accept or Reject each: `BPR-1301-wrapper-row-v002`
(fingerprint `e53f4fdbafd0…`, now with the found line) and
`BPR-1301-sign-in-line-v002` (`acfffbcee1d3…`, unchanged text). The frozen
build of ecb7dd771, its source and build tree live in
`/Users/jaeger/.mask/attended-review/bpr1301-wrapper-look-v002`;
`python3 /Users/jaeger/.mask/attended-review/bpr1301-wrapper-look-v002/launch.py`
(this terminal's `PATH`) and `... launch.py --as-from-finder` (launchd's
`PATH`) check the build, fixtures and samples and report an exit within 8
seconds (both smoke-tested offscreen: Settings shown). Next: `status --after 6`
on that state; two Accepts satisfy :owner-look and the card finishes; a Reject
returns with its note for a v003.

### Publication (2026-10-07)

`main` is frozen for a macOS prerelease, so v002 is pushed as the branch
`claude/bpr1301-v002` for the coordinator to merge after the release, not to
`origin/main`.

The rebase onto `origin/main` efa26ca02 renamed baa098e4d to a2c7b2ce3 and
ecb7dd771 (the frozen look's build) to c96ea197d; their round-trip pins were
resealed over BPR-1086's in the commit after the park. Merged tree: 15/15
focused wrapper, settings, preferences, writing, cycle-send, round-trip and
architecture rows; DOCUMENTATION_OK.

Integrated 2026-10-08 onto `origin/main` 79d7c85e0, after the freeze lifted:
a2c7b2ce3 is now 594c61cbd and c96ea197d (the frozen look's build) is now
ffcb3a2e7 (`git range-diff`: identical; only the first commit's card and
round-trip note text changed). The frozen root and session ee2391ed keep the
c96ea197d identity. The round-trip pins were
recomputed on the merged tree over BPR-1305's reseals (full application
ef0bef84…, app manifest b01cca50…; mutation pin unchanged). Merged tree:
18/18 focused wrapper, settings, preferences, writing, cycle-send, launch
canary, round-trip, composition (application, agent invocation, tour public
path) and architecture rows; DOCUMENTATION_OK.

### Owner look v002 answered (2026-10-08)

The owner handed session `ee2391ed3fa000fb68deb48b43014024` back at cursor 9
(host stopped):

- `BPR-1301-wrapper-row-v002` (`e53f4fdbafd0…`): **Accept**. The Process
  wrapper row, with its found line, is accepted.
- `BPR-1301-sign-in-line-v002` (`acfffbcee1d3…`): **Reject**, note verbatim
  "Cant read this", with a screenshot of the native macOS dark window
  (`/Users/jaeger/bpr-owner-att/BPR-1301-sign-in-line-v002-0.png`, kept out
  of the repository).

The screenshot shows the explanation paragraph and the Sign-in line cut off
top and bottom, not only faint: their wrapped lines were drawn into rows too
short for them. The macOS style keeps a form's fields at their size hint
(`QFormLayout::FieldsStayAtSizeHint`), so both labels were drawn narrower
than the width their height was measured at. The offscreen Fusion runs grow
fields to the row, which is why v002's height check passed. The explanation
was also set in `QPalette::PlaceholderText`, which reads 2.95:1 against the
page in BetterPullReview Light, 3.59:1 in Dark and 4.0:1 in High Contrast
Light, all below AA. In the screenshot (macOS dark) it read about 6:1; macOS
light would put it near 3.8:1 (estimated).

v002 was released (`resume-development --all`, cursor 10; both verdicts
kept). Its frozen root `bpr1301-wrapper-look-v002` was deleted; its state
folder keeps the session record.

## Implementation v003 (2026-10-08, 7b79ecd80)

The owner asked for the sign-in line made readable: AA contrast against the
panel in light, dark and high contrast, its font size and its placement, in
the page's normal text role rather than a faint one, and kept short. BPR-1288
owns the page's full redesign.

- `claude_wrapper_panel`: the Claude form's field column grows to the row under
  every style (`AllNonFixedFieldsGrow`), so each wrapped line is measured at
  the width it is drawn at. Every Claude line (status, found, explanation,
  Sign-in) is set in `QPalette::WindowText` at the page's own size. That is
  the role the page's other explanations use (the page has no separate
  secondary text role), at 14.7:1 in Light, 13.8:1 in Dark and 21:1 in both
  high-contrast themes. Space, not a fainter colour, sets the explanation
  apart, and an 8 px margin under the wrapper row sets the Sign-in row apart.
- The Sign-in line is shorter and still says the whole thing: "Claude and
  your wrapper handle sign-in. BetterPullReview never asks." At the window's
  default width it is one line.
- The rest of Agents & context has no other faint explanatory text. The
  page's opening paragraph, the request scope line and the writing-hooks text
  are all in the normal role. The only grey text is the disabled "No current
  draft" check box, a disabled control.
- `docs/development/settings.md` quotes the new line.

### Evidence v003 (macOS, build/dev at 7b79ecd80)

- `qt_settings_window` gains `test_agents_page_claude_rows_are_readable`. Under
  a proxy style that keeps form fields at their size hint, as macOS does, no
  Claude line is cut off at 680, 900 or 1300 px. Every line is at the page's
  text size, and every line reads 4.5:1 or better against the page in the
  native palette and in BetterPullReview Light, Dark, High Contrast Light and
  High Contrast Dark (`qt::contrast_ratio`, WCAG). The existing row's sign-in
  text check is updated. 4/4 `qt_settings_window` rows pass (scales 100, 125,
  150 and 200). It also passes with `QT_STYLE_OVERRIDE=macos` offscreen.
- Failing-before controls, each a one-line mutation rebuilt and run:
  `PlaceholderText` back on the explanation fails "claudeWrapperHelp reads
  2.950000:1" in Light; with the growth policy line removed it fails "no
  Claude line is cut off ... at the reviewed width: claudeWrapperHelp".
- Offscreen renders in all four themes and the macOS style show every line
  whole. `coding-format-check` passes. clang-tidy on the two changed files
  finds nothing new (one older naming warning at
  `settings_window_tests.cpp:639`). `DOCUMENTATION_OK`.

### Owner look v003 (BPR-1301:owner-look, pending)

Session `0cccc3a1e0cdbcfe806df51a2b6f6fd0` (new and short; not cbd48b91),
state `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1301-sign-in-v003`
against the main checkout, host detached (pid 52129, loopback port 49680),
cursor 3 after preparation. One candidate, one Accept or Reject:
`BPR-1301-sign-in-line-v003` (fingerprint `e467343c43dc…`). The frozen build
of 7b79ecd80, its source and build tree live in
`/Users/jaeger/.mask/attended-review/bpr1301-sign-in-look-v003`;
`python3 /Users/jaeger/.mask/attended-review/bpr1301-sign-in-look-v003/launch.py`
checks the build and the fixture tree, opens Settings over Home on a temporary
github.com fixture profile and reports an exit within 8 seconds. Launched
natively once (`ATTENDED_SETTINGS_SHOWN`, up 23 s, then closed; nothing
captured). In that root, `qt_settings_window` x4 and
`application_review_round_trip_composition_contract` pass (5/5; the pinned
scope is untouched). Next: `status --after 3` on that state; an Accept
satisfies :owner-look (the wrapper row was accepted in v002) and the card
finishes; a Reject returns with its note for a v004.

Published: 7b79ecd80 was rebased onto `origin/main` d8738b93c as d7a30e56d,
with no conflicts and no upstream change to these files. The frozen root and
the session keep the 7b79ecd80 identity.

### Owed station runs (not blocking)

Linux and Windows have not run this change (v001, v002 or v003). At a revision
holding it (`origin/main` from 3627b98af; v003 from 7b79ecd80), no remote mutation; add
`core_agent_profile` to the expression:

- `bpr-main-linux`: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure -R '^(core_agent_wrapper|application_claude_process_wrapper|qt_claude_process_wrapper|qt_settings_window.*|sqlite_workbench_settings_storage|application_workbench_preferences_consumers|application_review_tour_writing.*|application_review_cycle_send_public_path|qt_agent_launch_canary|application_review_round_trip_composition_contract|architecture_boundaries)$'`
- `bpr-main-win`: `cmake --preset windows-dev && cmake --build --preset windows-dev && ctest --preset windows-dev -j4 --output-on-failure -R` with the same expression. Windows adds the `.cmd`, `.bat`, `WRAPPER.CMD` and `.ps1` refusals on real files and the `.exe` copy as wrapper.

## Boundaries

Host and Qt. The core profile registry and host policy are unchanged. No
`PATH` discovery of `claude` is added; that is a separate decision if wanted.
*Amended 2026-10-07 by the owner ("Yes, search PATH (Recommended)"):* with no
wrapper saved, `claude` alone is searched for on `PATH` and in a fixed list of
usual install folders, as the owner amendment above says; no other
executable is searched for and no shell is started. No environment-variables
setting.


## Windows pin (bpr-main-win, 2026-10-07)

`application_review_tour_writing_wrapper` was registered `WINDOWS_UNCONTAINED`
without a pin, so `test_network_isolation_manifest` named it on Windows.
Measured contained, it exits 1 before printing a line (0.09 s), like
`application_review_tour_writing`, which runs the same composed course.

It is now pinned `dos-volume-name`. Uncontained it passes (10.4 s), and the
manifest passes. This is only the registration. The card's own Windows rows
wait for v002.
