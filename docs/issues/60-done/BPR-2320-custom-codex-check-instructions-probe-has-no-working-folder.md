---
schema: "implement-rfc.card.v1"
id: "BPR-2320"
title: "A custom Codex (JSONL) agent's Check always fails: its instructions probe has no working folder, so the process launcher refuses it before the program starts, and the result is labelled as another agent"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2320--BPR-2320--2ecc059e81b3.json","acceptance":["AC1","AC2","AC3","AC4","AC5"],"owner_verification":"pending","found_by":"owner, 2026-11-20, macOS, main at ffcbf8efc: a custom Codex (JSONL) agent whose program is a corporate sandbox launcher (arguments `--no-extra-output`, `--codex`; Check `--version`; Instructions 'At the start of the prompt'; Rely on the launcher's sandbox on) showed 'Check failed' with 'agent working directory is not an accepted directory'. A custom Claude agent on the same launcher passed. Earlier the same day a custom Claude agent pointed at the launcher's double-click entry instead of its CLI opened an interactive terminal window from the Agent tab","related_cards":["BPR-1375","BPR-1753","BPR-1769","BPR-2305","BPR-2307"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: []
---

# BPR-2320: A custom Codex (JSONL) agent's Check always fails: its instructions probe has no working folder, so the process launcher refuses it before the program starts, and the result is labelled as another agent

[[BPR-2305]] added an instructions probe to a custom Codex (JSONL) agent's
Check. The probe can never start, so no custom Codex JSONL agent can pass
Check, whatever its program. That is Part A. The failure is reported under
the wrong agent's name: Part B. Part C is a related gap the owner hit the
same day: Check cannot tell a program that answers from one that opens a
window.

## Observed

The owner, 2026-11-20, macOS. Two custom agents, both with a corporate
sandbox launcher's command-line binary as the program:

| | Claude agent | Codex agent |
|---|---|---|
| Speaks | Claude Code (stream JSON) | Codex (JSONL) |
| Arguments | `--no-extra-output`, `--skip-skills-upgrade` | `--no-extra-output`, `--codex` |
| Check | `--version` | `--version` |
| Instructions | System prompt | At the start of the prompt |
| Rely on the launcher's sandbox | n/a | on |
| Result | **Works** | **Check failed** |

The Codex agent's result, in Settings › Agents:

```text
✕ Claude Code is signed in
  Claude Code didn't start. failed_to_start
  stderr:
  agent working directory is not an accepted directory
```

### A. The instructions probe has no working folder

- The check's first step, the agent's own `--version` launch, passed. Its
  request is `WritingOnly` (`src/application/src/agent_setup_host.cpp`,
  around line 1192), so the process launcher runs it in a fresh empty folder
  ([[BPR-1375]], `src/adapters/agent/src/agent_process_launcher.cpp`, around
  lines 424–442).
- The second step is the probe that [[BPR-2305]] added:
  `build_codex_instructions_probe` (`src/application/src/agent_launch.cpp`,
  around lines 4904–4925). Its request sets the executable, arguments,
  input, environment, path and profile. It sets no `WorkingDirectory` and is
  not `WritingOnly`.
- The process launcher refuses any request whose working directory is empty,
  relative or not a directory, before it starts anything (same file, around
  lines 476–487). That refusal is the message above. The launcher, and Codex,
  never ran.
- The probe runs for every custom Codex JSONL agent
  (`agent_setup_host.cpp`, around lines 1197–1200), whatever its
  Instructions setting. So no such agent can pass Check.
- The tests missed it. `agent_launch_policy_tests.cpp` (around lines
  495–504) checks the probe's arguments and input only. The setup host tests
  run it through a fixture process port that ignores the working directory.
  [[BPR-2305]] left the live check as an owner step.

### B. The result names the wrong agent

- The checked agent was the Codex one. The result line read "Claude Code is
  signed in" and "Claude Code didn't start".
- The words come from `agent_check_words` and the failure text in
  `src/adapters/qt/src/agent_setup_view.cpp` (around lines 206 and
  238–243). The name comes from `setup_agent_name` (around lines 225–236),
  which uses the **preferred** agent's name when its protocol matches, and
  otherwise the setup agent's plain name. Not the entry that was checked.
  How "Claude Code" was reached for a Codex entry is unconfirmed: possibly
  the setup agent the page shows, or the preferred entry (the Claude one).
- "is signed in" is also the wrong claim for a custom agent's check. It ran
  the agent's own check arguments and a probe, not a sign-in status.

### C. Check cannot tell an answering program from one that opens a window

Earlier the same day, the owner's first try pointed both agents at the
launcher's **double-click entry** in its app bundle (the file next to its
command-line binary). Picking the Claude agent in the Agent tab opened a
Terminal window running an interactive Claude session:

- the launcher's own startup lines, so `--no-extra-output` was dropped;
- a skills self-update, so `--skip-skills-upgrade` was dropped;
- an interactive prompt, so `-p`, stream JSON and the prompt were dropped;
- the owner's home checkout as the working folder, not BetterPullReview's.

BetterPullReview then waited on a run whose output it could never read.
Pointing the program at the command-line binary fixed it. Whether Check
would have passed on the double-click entry was not tried: a `--version`
check that it ignored would have ended without output, and how Check treats
that is unconfirmed.

## Change

### 1. Give the instructions probe a folder

- `build_codex_instructions_probe` sets `WritingOnly = true`, as the
  agent's own check launch does. The probe then runs in a fresh empty
  folder that is removed after it: no checkout, no `AGENTS.md` or
  `.codex/` the reviewer did not choose, and nothing left behind.
- The probe keeps `--skip-git-repo-check`, which an empty folder needs.
- Look for every other request built outside the review workspace that
  sets neither `WorkingDirectory` nor `WritingOnly`, and fix them the same
  way. Better still, the request builder asserts that
  one of the two is set, so the next one fails in tests, not on the owner's
  machine.

### 2. Name the checked agent, and say what was checked

- The result line uses the checked entry's own name ("<agent name> didn't
  start"), never the preferred agent's or the setup agent's.
- A custom agent's check is described by what it ran: "Check passed",
  "The check command failed", "Instructions reach the model" or "The
  instructions probe didn't start". "Signed in" is kept for a popular agent's
  sign-in status.

### 3. Catch a program that does not answer in its protocol

- A custom Claude (stream JSON) agent's check also runs one short headless
  turn through its program, as the Codex probe does, and passes only if the
  answer arrives as stream JSON with an init event.
- That turn, and the Codex probe, run with stdin closed after the input and
  a short bound. A program that writes nothing, or writes text that is not
  the protocol, fails with "The program answered, but not in Claude Code's
  stream JSON", plus the first line it wrote and its `--version` line.
- On macOS, a program inside an `.app` bundle's `Contents/MacOS/` that is
  not the bundle's command-line tool is worth a warning in the agent dialog.
  How to tell is open: the bundle's `CFBundleExecutable` is the double-click
  entry, which suggests the rule. Confirm against the launcher before
  relying on it.

### 4. Tests

- Launch policy: `build_codex_instructions_probe` returns a `WritingOnly`
  request, with both Instructions settings.
- Process launcher: a request with neither a working directory nor
  `WritingOnly` is refused with today's message, as now. The probe request is
  not.
- Setup host: a custom Codex JSONL check through the real process launcher
  and a fixture Codex program that follows the rule passes, and its probe
  ran in a removed temporary folder. The fixture process port asserts that
  every request names a folder or is `WritingOnly`.
- Qt: a failed custom Codex check with a Claude agent preferred shows the
  Codex entry's name, and no "signed in".
- Setup host: a custom Claude agent whose fixture program prints plain text
  and exits 0 fails Check with the protocol message.

## Acceptance

- AC1. A custom Codex (JSONL) agent whose program follows its instructions
  passes Check. Its probe runs in an empty temporary folder that is gone
  afterwards.
- AC2. Through a launcher that drops developer instructions, a custom Codex
  agent set to "As developer instructions" fails Check with
  `instructions_dropped` and the offer to switch, as [[BPR-2305]] intended,
  instead of `failed_to_start`.
- AC3. Every check result names the checked agent and says what was checked.
- AC4. A custom Claude agent whose program does not answer in stream JSON
  fails Check with a message naming the protocol, before the Agent tab can
  start it.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Owner step (needs the corporate Mac and its launcher)

[[BPR-2305]]'s owner step 2, unchanged, once Part A lands: Check on the Codex
agent above passes with "At the start of the prompt" and fails with
`instructions_dropped` set to developer instructions. Then one
write-granted turn either writes inside the scratch folder or names the
failed paths ([[BPR-2307]]).

## Out of scope

- The launcher. Its double-click entry ignoring arguments is how a
  double-click entry behaves, not a defect of it.
- Whether the Agent tab can use an agent whose Check failed. Today's rule
  stands.

## Resolution

- **A.** `build_codex_instructions_probe` returns a `WritingOnly` request, so
  the process launcher runs it in a fresh `bpr-writing-*` folder and removes
  it after. It keeps `--skip-git-repo-check`. Every other request built
  outside a review workspace (status, version, own check, model catalog,
  `--help`) already was `WritingOnly`. The new
  `application::names_agent_folder` says whether a request names a folder.
  The setup host tests' scripted process port expects it of every request, so
  a builder that forgets fails there. The launcher's refusal and its message
  are unchanged.
- **B.** The setup projection now carries `CheckedEntry` (the entry whose
  result `State.AgentCheck` shows) and `CheckStep` (`sign_in`, `program`,
  `check_command`, `instructions_probe`, `protocol_probe` or `unknown` for a
  kept result). `agent_check_words` and the new `agent_check_failure_words`
  and `checked_agent_name` (Qt) name that entry. A custom entry's row says
  what its check proves ("… check command passes", "… instructions reach the
  model", "… answers in Claude Code's stream JSON", "… program can run").
  The failure line names the step ("… instructions probe didn't start.",
  "… check command failed."). "Signed in" is left only for a popular agent.
  Settings › Agents and the Agent tab setup both use them. The likely route
  to the observed "Claude Code" was Check all, or Check one entry, with no
  kept result for the chosen entry: the Codex failure stayed on screen, but
  `setup_agent_name` named the setup agent's kind.
- **C.** A custom Claude (stream JSON) agent's check runs
  `build_claude_stream_probe`: one tool-free `-p` turn through its program,
  with its fixed arguments first, in an empty folder of its own. stdin closes
  after one message, and the run is bounded like the Codex probe (2 minutes).
  The check passes only on success with Claude's init frame
  (`claude_stream_answered`). Plain text or nothing fails as
  `output_unreadable`, with the words "<name> answered, but not in Claude
  Code's stream JSON. If its program opens a window, choose the launcher's
  command-line program instead.", its first line ("Its first line: …" or "It
  wrote nothing.") and its own check's version. The decoder keeps the first
  line it read past (`skipped_line`), and a failed run reports it as
  `RejectedOutputLine` when no line was refused.
- **C, left open.** The macOS warning for a program inside an `.app`
  bundle's `Contents/MacOS/` is not implemented. The `CFBundleExecutable`
  rule could not be confirmed against the launcher, which is not on this
  machine. The stream JSON turn above now fails Check on such a program
  before the Agent tab can start it, so the warning would only add an
  earlier hint. Decide it once the owner has looked at the launcher's bundle.

## Owner step (pending owner verification)

Needs the corporate Mac and its launcher. This step was not run here: the
launcher is not on this machine, and an agent may not run a real Codex,
Claude or launcher on the owner's usage. [[BPR-2305]]'s owner step 2, now
unblocked:

1. Settings › Agents › Check on the custom Codex (JSONL) agent (the
   launcher's command-line binary, `--no-extra-output`, `--codex`, Check
   `--version`) passes with "At the start of the prompt", and fails with
   `instructions_dropped` and the offer to switch when set to developer
   instructions.
2. One write-granted turn either writes inside the scratch folder or names the
   failed paths ([[BPR-2307]]).
3. Check on the custom Claude agent on the same launcher still passes, now
   including its stream JSON turn.

## Evidence

Fixture programs only (the canary and a `/bin/sh` script); no real agent ran.
Candidate `f52e593e6` on main `c30c6c313` (first run on `092f4aa9d`; after the
rebase over [[BPR-2318]] the AC1, AC3, AC4 and AC5 suites, `qt_agent_setup_view`,
`qt_settings_window` and the documentation gate were run again and pass), dev preset (debug info off to fit the disk), macOS:

| Criterion | Check | Result |
|---|---|---|
| AC1 | `qt_custom_agent_check` › `test_a_custom_codex_check_passes_through_the_launcher`: the real `agent_process_launcher` and the canary as program. Check passes with both Instructions settings. The probe reports its folder as `bpr-writing-*`, and the folder is gone afterwards | Passed |
| AC1 | `qt_custom_agent_check` › `test_the_launcher_refuses_a_request_with_no_folder`: neither folder nor `WritingOnly` is refused with "agent working directory is not an accepted directory" before it starts; the probe request runs | Passed |
| AC1 | `application_agent_launch_policy` › `test_the_codex_instructions_probe` (`WritingOnly` with both settings, `--skip-git-repo-check`, `names_agent_folder`) | Passed |
| AC2 | `application_agent_setup_host` › `test_a_custom_codex_check_proves_its_instructions_arrive`: a fixture launcher that drops developer instructions gets `instructions_dropped` and the offer. Its port refuses a request with no folder, so this ran past where the owner's run failed | Passed; the live launcher is the owner step |
| AC3 | `application_agent_setup_host` › `test_a_custom_claude_check_proves_it_answers_in_stream_json`: a Codex entry checked while a never-checked Claude entry is preferred sets `CheckedEntry` to the Codex entry. `qt_custom_agent_check` › `test_the_result_names_the_checked_entry`: the row reads "Launcher Codex's instructions reach the model", the failure reads "Launcher Codex's instructions probe didn't start.", and neither says "Claude Code" or "signed in" | Passed |
| AC4 | `application_agent_setup_host` (plain text: `output_unreadable` with first line and version; nothing written, exit 0: same; init frame: passes; failed check command named as that step), `application_agent_launch_policy` › `test_the_claude_stream_probe`, `qt_custom_agent_check` › `test_a_custom_claude_check_needs_stream_json` (the canary passes; a `/bin/sh` program printing a banner fails with the protocol named and its first line) | Passed |
| AC5 | `application_agent_launch`, `application_agent_protocol_loop`, `application_agent_helper_turn`, `application_claude_process_wrapper`, `core_agent_setup`, `qt_agent_setup_view`, `qt_settings_window`, `qt_agent_launch_panel`, `qt_agent_launch_canary`, `qt_claude_process_wrapper`, `qt_review_tour_agent_setup_call`, `application_agent_setup_host_policy_public_path`, `architecture_boundaries` | Passed |
| AC5 | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| Standard | clang-format 22.1.8 on every changed C++ file. `coding-format-check` flags only `tests/end_to_end/follow_up_public_path.cpp`, untouched here and unformatted on main since `58eae7f05`. `tidy.py --warnings-as-errors` on the touched units: three findings, all in lines this card did not change (`agent_launch.cpp:1829`, `agent_launch_policy_tests.cpp:227,239`) | Passed for new code |
