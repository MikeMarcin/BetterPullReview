# RFC-BPR-0104 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `agent-scratch`
([[BPR-2007]], [[BPR-2008]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Frontend-design was loaded before
the Qt surface work. Owner amendments in force: "If you need it you need it go
build it."; "main should be the source of truth for the tracker"; "Continue
working unattended, you can file reviews but dont block on their results".
Debug timings are nongating; nothing below is a timing claim.

## How the installed agents are given a folder and a grant

Installed: `/opt/homebrew/bin/claude` 2.1.319 and `~/.local/bin/codex`
0.167.0 (`claude --version`, `codex --version`). Established on 2026-11-11
from each one's own `--help`, Codex's own protocol schema (`codex app-server
generate-json-schema`) and runs on throwaway Git workspaces, before BPR
passed anything.

| Runtime | What a working and scratch directory is | How a write and run allowance is given | What enforces it |
|---|---|---|---|
| Claude Code 2.1.319, `-p` stream JSON | The process's working directory (BPR's workspace); `--add-dir DIR` adds a directory the file tools may reach | `--allowedTools` rules: `Write(//abs/**)`, `Edit(//abs/**)`, `Bash(PREFIX/*)`. Headless, anything else that would ask is refused and listed in the result's `permission_denials` | Claude's own permission check, on its tools. A script a rule lets it run is not confined: it has the user's rights |
| Codex 0.167.0, App Server and `exec` | `cwd` (thread or turn), and a sandbox policy or a permissions profile | `-c permissions.NAME.filesystem={":root"="read","DIR"="write"}` with `-c default_permissions="NAME"` replaces the configured `sandbox_mode` (this computer's `workspace-write`) for the run; the thread reports `workspaceWrite` with the folder as its one writable root | Codex's sandbox (Seatbelt): a `touch` in the workspace failed with "Operation not permitted", the folder write succeeded, network failed to resolve |

Three facts decided the arguments:

- A Claude `Bash` rule in the `:*` form (`Bash(python3 DIR/:*)`) matches only
  at a word boundary, so it never matched a file inside the folder: the first
  live run wrote `count.py` and was refused running it ("This command requires
  approval"). The glob form `Bash(python3 DIR/*)` matches.
- No Claude rule matches a quoted path: with a space in the folder's path,
  `python3 "/a b/scr/count.py"` was refused under both `Bash(python3 /a b/scr/:*)`
  and `Bash(python3 "/a b/scr/:*)`. A folder is therefore granted only on a
  path with no space, control character, quote, parenthesis or comma; macOS's
  profile data sits under "Application Support", so the folder goes under the
  user's cache folder there.
- Codex keeps the user's `approval_policy` (`on-request` here) unless told
  otherwise; a granted run passes `approval_policy="never"` because a headless
  run cannot answer an escalation.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | Live course below: Claude Code (Haiku) and Codex each wrote `count.py` in its folder, ran it with `python3` against the workspace and reported its output (`src/parse.cpp` 3, `src/validate.cpp` 2) in one turn. Off: `application_agent_launch` (`test_the_grant_crosses_exactly`) and `application_agent_invocation_host`: no `--add-dir`, no `--allowedTools`, no profile | Pass |
| AC02 | Live: each agent's attempt to create `bpr-probe.txt` in the workspace was refused (Claude: "Claude requested permissions to write to … workspace/bpr-probe.txt, but you haven't granted it yet"; Codex: "`touch` failed with `Operation not permitted`"), and `git status --porcelain --untracked-files=all` in every workspace BPR made was clean after both turns. Arguments: only the folder is writable in what BPR passes | Pass |
| AC03 | `application_agent_scratch_folder`: the folder is made owner-only under its root, outside the repository and workspace; a turn's own folder is removed when it ends (`application_agent_invocation_host`); at 256 MiB (a sparse file) the folder is full; `application_agent_launch`: a full folder gets no write allowance and the agent is told "It is full: this turn you may not write there". A native conversation's folder is kept with the conversation, as its retained workspace is | **Partly met.** The bound is checked when a turn starts; a write that crosses 256 MiB during a turn is not refused. Not reached: removal of a native conversation's folder, because RFC-BPR-0096 removes no retained conversation automatically |
| AC04 | `core_agent_grant`: off until turned on; own checkout is `own_checkout` with no allowance and nothing to ask; a disposable checkout asks once (`unasked`), declining leaves none. `application_agent_invocation_host`: the reviewer's own checkout launches with no build allowance even when asked; `qt_agent_setup_view`: the setting's words; `sqlite_agent_setup_storage`: the pull request's answer is kept across a restart | Pass at the unit, host and storage layers; the question's composed path (`grant_agent_turn`) has no course |
| AC05 | `qt_agent_footer`: the grant line reads "Writes only in its scratch folder", "Reads only" and "Builds and tests in its disposable checkout" for the three states, the scratch item is checked while on, the builds line names the agent; Claude's words say "permission rule" and never "sandbox"; Codex's say "Codex's sandbox enforces this". Live: the line read "Writes only in its scratch folder" for the turn that wrote, "Reads only" for the turn that could not | Pass |
| AC06 | `application_agent_launch`: a read-only (tour) run and a tool-free run refuse a grant; the plain-text harness refuses one; the existing tour and hook argv tests are unchanged and pass | Pass |
| AC07 | Independent critical review | **Pending.** Filed for the coordinator; a self-review (below) is input to it, not it |
| AC08 | `core_agent_grant`: a new agent and agents migrated from the pre-list settings start with the folder on and builds off; `sqlite_agent_setup_storage`: an agent stored before the setting reads back on; with its starting state Off, a pull request with no choice resolves to no scratch allowance | Pass |
| AC09 | `sqlite_agent_setup_storage`: the choice is kept for that pull request and agent after a restart, another pull request and another agent have none, the starting state is untouched. Live: unchecking "Scratch folder in this pull request" in the footer picker turned the line to "Reads only" and the next turn had no allowance: Claude's write of `second.py` was refused, Codex's failed with "operation not permitted", and the folder held only `count.py` | Pass for restart, other pull request and next turn; New session is the same stored choice (it is not keyed by conversation) and has no separate course |

## Live course

`live_second_account_course --course agent-scratch --root DIR --pull 40 --path PROGRAM [--author codex]`
([source](../../tests/end_to_end/live_second_account_course.cpp)): the shipped
composition, offscreen, signed in through `gh`'s active account, on
[BPR-Test #40](https://github.com/MikeMarcin/BPR-Test/pull/40) (read only),
the installed agent added from the agent setup. Turn 1 asks it to write
`count.py` in its scratch folder, run it against the workspace and try to
create `bpr-probe.txt` in the workspace; the course then unchecks the footer
picker's scratch item and turn 2 asks it to write `second.py` there.

Run 2026-11-11 from the lane's working tree (after `Bash(FOLDER/*)` replaced
`FOLDER/:*`; before `sh`, `bash` and `zsh` left the runner list, which the
course does not use), root `/Users/jaeger/bpr-agent-scratch-course-20261111`
(removed after), agent folders under `data-agent-scratch-v1/<conversation>`:

| | Claude Code 2.1.319 (Haiku) | Codex 0.167.0 (App Server) |
|---|---|---|
| Line before turn 1 | Writes only in its scratch folder | Writes only in its scratch folder |
| Turn 1 | answered; "Worked for 0 s: 1 command, 1 file written in its scratch folder"; printed `3 src/parse.cpp`, `2 src/validate.cpp`; `bpr-probe.txt` refused | answered; same folded line; printed `src/parse.cpp: 3`, `src/validate.cpp: 2`; `touch` failed with `Operation not permitted` |
| Folder after turn 1 | `count.py` | `count.py` |
| `git status` of every workspace | clean | clean |
| Line after unchecking the item | Reads only (item unchecked) | Reads only (item unchecked) |
| Turn 2 (`second.py` there) | refused ("… second.py, but you haven't granted it yet") | refused ("operation not permitted") |
| Folder and workspaces after | `count.py`; clean | `count.py`; clean |

Two earlier runs are kept as findings. The first (`Bash(FOLDER/:*)`) let
Claude write `count.py` and refused running it, which led to the glob rules.
A run with the course root under `/private/tmp` let Codex write `second.py`
after the folder was turned off: that launch carried no allowance (a changed
argument list starts a new App Server, `native_agent_connection::matches`),
and this computer's own Codex `workspace-write` mode lets commands write
`/tmp` and `$TMPDIR`. The folder is never under `/tmp` in the application
(beside the profile's data, or the user's cache folder), so the course was
moved out of it rather than the product changed; a reviewer whose own
configuration lets Codex write the folder is still allowed by it, not by BPR.

## What each runtime enforces and what it only permits (input to AC07)

A self-review by the implementer; AC07 needs an independent one.

- Claude Code: every allowance is a permission rule Claude's tool layer
  checks. `Bash(python3 FOLDER/*)` also matches `python3 FOLDER/../x.py`; a
  script it may run can write anywhere the user can, reach the network and
  read credentials. `Bash(FOLDER/*)` with `chmod +x FOLDER/*` lets any file
  the agent wrote run directly. The builds list (`cmake`, `make`, `npm`,
  `cargo` and others, each `Bash(NAME:*)`) runs the pull request's code with
  any arguments, from any directory. The grant item's words say so: "A
  permission rule Claude Code follows, not enforced. Its scripts run with your
  account's rights."
- Codex: writes are held by Seatbelt to the folder (and, for builds, the
  disposable checkout and the temporary folder), network is off, everything
  is readable. A build that writes outside those (a cache under the home
  folder) fails. The words say "Codex's sandbox enforces this, network off".
- BPR passes no network allowance and no `bypassPermissions`,
  `--dangerously-*` or sandbox-off flag; the reviewer's own Claude settings
  still apply, so a reviewer whose own settings allow more gets more, from
  them and not from BPR.
- Windows: a grant path must be absolute POSIX, so a Windows turn reads only.

## Runs

Focused rows at the candidate, all passing: `core_agent_grant` (new),
`core_agent_activity`, `application_agent_launch`
(`test_the_grant_crosses_exactly`, new), `application_agent_scratch_folder`
(new), `application_agent_invocation_host`
(`test_the_grant_reaches_the_launch_and_the_folder_goes_with_the_turn`, new),
`sqlite_agent_setup_storage`, `qt_agent_footer`, `qt_agent_setup_view`,
`qt_settings_window`, `application_agent_setup_host`,
`application_agent_chat_*` (eleven rows, including the helpers public paths
and `application_agent_chat_custom_agent_contract`),
`application_agent_finding_composition_contract`,
`application_agent_invocation_composition_contract`,
`application_composition_source_contract` (production application source pin
resealed, its note saying this change is a grant), `architecture_boundaries`
(first failed on the quoted shell names in the runner list, which were
removed), `test_network_isolation_manifest`; `coding-format` on the changed
C++; clang-tidy on `agent_grant.cpp`, `agent_activity.cpp`,
`agent_scratch_folder.cpp` (one finding fixed), `agent_run_settings_picker.cpp`,
`agent_entry_dialog.cpp`, `agent_run_status.cpp` (one naming warning in
`agent_run_settings_picker.cpp` that predates this change); documentation
gate. Not run: the whole dev suite, the core-only preset (the core change is
two new files and two fields).
