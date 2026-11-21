---
schema: "implement-rfc.card.v1"
id: "BPR-2348"
title: "A custom agent's Check runs the program BetterPullReview found for that kind of agent, not the entry's own Program: a shim on the search path doubles the entry's arguments, and with nothing found the check says a program that exists is not found"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2348--BPR-2348--17d67a588ab5.json","found_by":"owner, 2026-11-21, build 74 on macOS: a custom Codex (JSONL) agent whose Program is a corporate sandbox launcher's command-line binary (arguments `--no-extra-output`, `--codex`) failed Check with the launcher's 'must be the first argument' error. The same probe command passed twelve times from a terminal, with the app's environment, descriptors and process group reproduced. The check had run a shim found on the search path instead. Started from a terminal, the app then found no Codex program and the check said 'Not found'","related_cards":["BPR-1753","BPR-1769","BPR-1859","BPR-2051","BPR-2305","BPR-2320"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: []
---

# BPR-2348: A custom agent's Check runs the program BetterPullReview found for that kind of agent, not the entry's own Program: a shim on the search path doubles the entry's arguments, and with nothing found the check says a program that exists is not found

A custom agent's turns start the entry's own Program with its arguments
([[BPR-1753]], `agent_entry_process`). Its Check does not. Check starts the
program setup found for the entry's **kind** (Claude Code, Codex): the
preferred entry's Program when the preferred entry is of that kind,
otherwise whatever the search finds on `PATH` and in the usual folders. It
then adds the entry's arguments to that program. So:

- **A.** Check can run a different program from the one the turns run, and
  pass or fail for reasons that have nothing to do with the entry.
- **B.** What it runs depends on how the app was started, because the
  search reads the app's `PATH`.
- **C.** When the search finds nothing, Check says the entry's program
  "isn't where it was found any more" and marks it **Not found**, although
  the entry's Program exists and the turns can run it.

[[BPR-2320]] made the result name the checked entry. That holds: the lines
below name the right entry. They describe the wrong program.

## Observed

The owner, 2026-11-21, build 74 on macOS. Two custom agents, both with a
corporate sandbox launcher's command-line binary as Program:

| | Claude agent (preferred, first) | Codex agent (second) |
|---|---|---|
| Speaks | Claude Code (stream JSON) | Codex (JSONL) |
| Program | the launcher | the launcher |
| Arguments | `--no-extra-output`, `--skip-skills-upgrade` | `--no-extra-output`, `--codex` |
| Check | `--version` | `--version` |

The owner's `~/.local/bin` holds two shims the launcher installed:

```sh
# ~/.local/bin/codex
exec '<launcher>' --no-extra-output --codex "$@"
# ~/.local/bin/claude
exec '<launcher>' --no-extra-output "$@"
```

### Started from the Dock

The Codex agent's Check failed at its instructions probe, after its
`--version` passed:

```text
✕ <codex agent>'s instructions reach the model
  <codex agent> answered, but not in a way BetterPullReview can read.
  nonzero_exit (exit 2)
  stderr:
  ERROR: --no-extra-output must be the first argument to <launcher>.
```

The launcher was run by hand from a terminal with the probe's exact
arguments, `exec --json --skip-git-repo-check --ephemeral --sandbox
read-only -c approval_policy="never" -c mcp_servers={} -c
tools.web_search=false -`, after the entry's two. Every variation below
answered `ok` as clean Codex JSONL, exit 0:

- the Codex profile's environment allowlist, with and without `USER`
  and `LOGNAME`;
- a Dock app's `PATH` (`/usr/bin:/bin:/usr/sbin:/sbin`), with and without
  `LANG` and `TMPDIR`;
- descriptor 3 open as a pipe or `/dev/null`, descriptor 4 open on a file;
- a process group of its own, as the supervisor makes.

The check had run `~/.local/bin/codex`, the shim, so the launcher received
`--no-extra-output --codex --no-extra-output --codex exec …`.

The Claude agent passed through its own Program, because it is the
preferred entry. Had it been checked through `~/.local/bin/claude`, that
check would have run `--no-extra-output --no-extra-output
--skip-skills-upgrade …`.

### Started from a terminal

The search read the shell's `PATH` and accepted no `codex`. Check all ended
at once (`agent.setup_check.failed failure=agent_not_found`, no process
run):

```text
<codex agent>        <launcher path>        Not found
✕ <codex agent>'s instructions reach the model
  <codex agent> isn't where it was found any more. Look again, or choose its
  program in Settings.
  No process output was returned.
```

The row shows the entry's Program, which exists, beside "Not found".

## Cause

`src/application/src/agent_setup_host.cpp`:

1. **Detection** (around lines 520–585) fills `Found` with one program per
   kind. For the preferred entry's kind it uses that entry's Program
   ("[[BPR-1753]]. A chosen entry with its own program is the program setup
   checks for its kind"). For every other kind it takes a saved program
   (`CodexProgram`), or else `Locate`, which searches `PATH` first and then
   the usual folders (`src/core/src/agent_wrapper.cpp`,
   `usual_codex_folders`, `usual_claude_folders`: `~/.local/bin`,
   `/opt/homebrew/bin`, `/usr/local/bin`, `~/.npm-global/bin`).
2. **The check** (`start_check`, around lines 1170–1260) takes
   `row = chosen_row()`, the row for the kind of `preferred_entry()`, and
   runs `path = row->Path` (line 1204). Both the entry's own check command
   (`own->Executable = path`) and the probe (`.Program = path`) use it, with
   `entry->Arguments` put before the probe's arguments.
3. During Check all, `preferred_entry()` becomes the entry being checked
   ([[BPR-2051]], `CheckingOverride`), but `Found` is not detected again. A
   second entry of a kind is checked with the first one's program, or with
   the searched one.
4. With no row for the kind, `start_check` settles `agent_not_found` at once
   (around lines 1176–1186), and Qt words it as the program having moved
   (`src/adapters/qt/src/agent_setup_view.cpp`, line 202;
   `agent_setup_settings.cpp`, around line 845, "Not found").

The turns do not share this. `agent_entry_process::run`
(`src/application/src/agent_program_resolution.cpp`, around line 144) starts
`entry->Program`. So Check can pass while turns would fail, and fail while
turns would work.

[[BPR-2320]]'s tests missed it: the setup host fixtures have no located
program of the entry's kind other than the entry's own.

## Change

### 1. Check an entry with its own Program

- A custom entry with a Program is checked with that Program: its check
  command, the instructions probe ([[BPR-2305]]) and the stream-JSON probe
  ([[BPR-2320]]). This is the same program and the same arguments its turns
  get, through the same resolution as `agent_entry_process`.
- Its result never depends on detection, on which entry is preferred, or
  on the app's `PATH`.
- An entry with no Program (a popular agent's preset that names none) keeps
  today's rule: the program found for its kind, as its turns resolve it.
- Detection still reports where each popular agent's own program is, for
  the Agent step's cards. It no longer chooses what a custom entry runs.

### 2. Say what is missing

- An entry whose Program cannot be run is refused as `program_refused`,
  naming the path and why, as [[BPR-1859]] words it.
- "Not found" and "isn't where it was found any more" are kept for an entry
  that relies on the search (no Program) when the search finds nothing.
  They never appear beside a Program that exists.

### 3. Show what ran

- The check's details name the program and the arguments it ran, before the
  process output. Show the program by path and the arguments as the entry
  lists them, then "…and the check's own" for the rest. Nothing here can
  hold a secret that the entry does not already show.

### 4. Tests

- Setup host: a custom Codex entry with Program `P` and arguments `A`, while
  a located `codex` elsewhere and a preferred Claude entry exist, runs its
  check command and probe with `P` and `A`, never the located program. The
  fixture process port records each request's executable.
- The same with two custom Codex entries checked by Check all: each runs its
  own Program.
- The same with no `codex` located anywhere: the entry passes, and no
  `agent_not_found` is settled.
- An entry whose Program is missing fails as `program_refused` naming it.
- Qt: a custom entry's failed check shows the program it ran in its details.
  A custom entry with a Program never reads "Not found".

## Acceptance

- AC1. A custom agent's Check runs the entry's own Program with the entry's
  arguments, the same command its turns run, whichever entry is preferred
  and however the app was started.
- AC2. A program on the search path with the same name (a shim, another
  install) never changes a custom entry's check.
- AC3. An entry whose Program exists is never reported as not found. One
  whose Program cannot run is refused by name.
- AC4. A check's details name the program it ran.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Owner step

On the corporate Mac, started from the Dock and then from a terminal: Check
all passes for both launcher agents in either order, and the Codex agent's
details name the launcher's own path.

## Out of scope

- The launcher's shims and its first-argument rule.
- How a popular agent's preset is found ([[BPR-1769]], [[BPR-1859]]).
