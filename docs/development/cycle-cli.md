# Review Cycles terminal (`bpr cycle`)

`bpr cycle` works local Review Cycles from a terminal ([[BPR-1167]],
[RFC-BPR-0058](../rfcs/RFC-BPR-0058-headless-cycle-cli-and-portable-consumer-skill.md)).
It is the same `bpr` executable as [`bpr tour`](tour-cli.md), built by the
`tour-cli` preset with tests disabled:

```sh
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix /absolute/install/root
/absolute/install/root/bin/bpr cycle capabilities
```

A dev graph gets the executable and its course with `-DBPR_BUILD_TOUR_CLI=ON`
(the `dev` preset leaves it off): `cmake --preset dev -B build/dev-tour-cli
-DBPR_BUILD_TOUR_CLI=ON`, then `ctest --test-dir build/dev-tour-cli -R
"^cycle_cli_public_path$"`.

## One set of services

The terminal composes the profile's own cycle store under the same
`application::review_cycle_runtime` the desktop's Review Cycles host composes,
with the read-only workspace source, the chosen-file reader and the local
export destination. Decisions stay in the core: wire names (`RC-3`, `R2`, `I7`,
`F1`), display lines to bytes, which cycle a folder is bound to
(`core::bound_to`, also what Open Project uses), round assembly, export
authorization and delivery admission. The desktop and the terminal say the
same words about a store, capture, export or response outcome
(`review_cycle_words.hpp`). The terminal starts no window, display, platform
plugin, account, provider, credential vault, agent, browser or network
connection; `QCoreApplication` is its only Qt object.

## Commands

| Command | What it does |
|---|---|
| `capabilities` | Contract, commands, limits and exit codes. Needs no profile. |
| `list [--cursor RC-n]` | One page of this profile's cycles. |
| `open PATH` | Opens a folder or file as a cycle: the cycle already bound to that root object, or a new one bound and captured as `R1`. |
| `show [RC-n]` | The cycle, its notes, frozen rounds, delivery observations and responses. |
| `capture [RC-n] [--again]` | Reads the bound folder now. Identical bytes record nothing unless `--again`. |
| `note [RC-n] [--file PATH [--lines A:B]] [--body-file PATH\|-]` | A note on a line range, a whole file of the latest capture, or the whole cycle. |
| `review [RC-n] [one of --include/--exclude/--earlier/--later In, --action A, --goal-file PATH\|-] [--expected-revision N]` | The next round's draft and the exact document it freezes as; one change per command. |
| `freeze [RC-n] [--expected-revision N]` | Freezes the draft revision you read into an immutable round. |
| `export [RC-n] --round Fn --output PATH\|- [--format markdown\|bundle] [--replace SHA256]` | A frozen round exactly as frozen, to a chosen file or standard output. |
| `compare [RC-n] [--from Rn] [--to Rn] [--changed-only] [--cursor PATH] [--path P]` | Files of a capture or a pair, or one file's rows, from retained bytes. |
| `response import [RC-n] --round Fn --file PATH\|-` | Appends a recipient's structured response to the round. It changes no note and verifies nothing. |

A cycle is named `RC-n`. Without one, the command uses the one active cycle
bound to the working folder; several are `target_ambiguous` and none is
`not_found` with `bpr cycle open .` as the next command. Nothing remembers a
"current" cycle and no window's selection is consulted.

## Output

Default output is plain prose and short lists on standard output. Errors say
why on standard error, followed by a `Next:` command when there is one; a
refusal writes nothing to standard output. Terminal controls and
bidirectional overrides in text are shown as `\uXXXX`.

`--json` writes exactly one `bpr.cycle.cli-result.v1` document on standard
output and nothing on standard error:

```json
{"schema":"bpr.cycle.cli-result.v1","command":"cycle freeze","operation_id":"…",
 "outcome":"ok","complete":true,"data":{…},"error":null}
```

An error has `"outcome":"error"`, `"data":null` and `"error":{"code","message","next"}`,
with `detail` where the code has facts to give (an existing file's digest, the
draft's current revision, the candidates of an ambiguous folder). A page with
more after it has `"complete":false` and a `next` cursor. IDs, revisions,
counts and byte sizes are decimal strings. `--schema-version 1` is accepted;
any other is `unsupported_version`.

| Exit | Codes |
|---|---|
| 0 | ok |
| 2 | `invalid_input`, `unsupported_command`, `unsupported_option`, `unsupported_version`, `target_ambiguous`, `not_found`, `profile_unknown`, `profile_ambiguous` |
| 3 | `interaction_required` |
| 4 | `stale_revision`, `conflict`, `draft_busy` |
| 5 | `policy_denied`, `capability_unavailable`, `source_unavailable`, `destination_unavailable`, `round_not_ready`, `capture_unstable`, `over_limit`, `requires_partition` |
| 6 | `uncertain` |
| 7 | `storage_unavailable`, `profile_unavailable`, `operation_failed` |
| 130 | `cancelled` |

## Without a terminal

A command is interactive only when standard input and output are both
terminals and neither `--json` nor `--non-interactive` is given. Otherwise
nothing opens an editor, prompts or reads standard input unless `-` was given
for it, and end of input is never an answer:

- `note` without `--body-file` is `interaction_required`. In a terminal it
  opens the trusted editor (`BPR_TOUR_EDITOR_ARGV`, or `VISUAL`/`EDITOR` as one
  absolute path, as for tours); with none configured it is
  `interaction_required` too. A note is at most 16 KiB.
- A scripted `review` change and every `freeze` name the draft revision they
  read with `--expected-revision`; a changed draft is `stale_revision`. In a
  terminal, `freeze` shows the round and asks for the word `freeze`.
- `export` never replaces other bytes at the destination without
  `--replace` naming exactly their SHA-256, as the `conflict` reported it; in
  a terminal it asks for the word `replace`. A file that already holds the
  round is left as it is.

There is no `--yes` or `--force`.

## Exports and what is recorded

`export` reads the named frozen round, decides the channel with the same
`core::authorize` every exporter uses (local file or terminal output) and
renders the document or bundle; a refused channel or an output over budget
writes nothing and uses no other channel. A save is recorded as `saved` under
the file's name. `--output -` to a pipe or file writes the exact bytes and
records `retrieved` by `terminal_output`; to a terminal it shows the text with
controls neutralized, which are not the exact bytes, and records nothing. No
observation means a round was received, read, acted on or accepted.

## Working while the desktop is open

The profile has one writer: whichever process holds its runtime lease
([[BPR-1168]]). A command that can take the lease runs here, on the store, and
releases it while a person answers. When another BetterPullReview process
holds it, the command is sent to that owner over its same-user command channel
(a local socket beside the desktop's activation endpoint on macOS and Linux, an
owner-private named pipe on Windows) and the owner runs it on its own store, in
a session of its own beside its windows. The terminal never opens the store a
second time, takes the lease or starts the desktop.

What travels is one command from the finite registry
(`core::cycle_command`) with that command's own options, the absolute working
folder and the absolute paths it names, and any text the terminal already read
(a note or goal, or a response from standard input). Prompts, the trusted
editor and standard output stay in the terminal: an interactive `freeze` reads
the draft through the owner, asks, then sends the freeze with the revision
shown; `export --output -` receives the bytes, writes them, then sends what
happened (`export record`). Files named by `export --output` and `response
import --file` are written and read by the owner, as the same user.

Windows showing the cycle read it again after a command writes it. While the
desktop shows the next round's draft:

- a goal typed and not yet entered is saved first, so `review` reads it and a
  `freeze` against the revision before it is `stale_revision`, never a round
  without it;
- while an input method is composing in the goal, `review` and `freeze` answer
  `draft_busy` (exit 4) and change nothing; notes and exports are not held;
- a desktop edit refused because the terminal changed the draft first keeps the
  typed goal in its field to save again; a desktop note is added to the cycle as
  it now is rather than dropped.

Review Cycles have no undo in either interface: a change is reversed by its
counter-change (`--exclude` after `--include`, a new goal), an ordinary edit
under the revision read, from either side.

`--operation-id ID` (1 to 64 letters, digits or hyphens; one is generated
otherwise, and every JSON result names it) identifies a command. Its store
writes are derived from it, so repeating a command never applies it twice: an
owner answers a repeat exactly as it answered the first, and the same ID with
another request is `conflict`. If an answer is lost the command exits 6
(`uncertain`): read the cycle, or repeat it with the same `--operation-id` to
get the owner's answer again. An owner of another protocol version is
`unsupported_version` and is sent nothing; an owner offering no channel is
`capability_unavailable`. If the owner exits mid-command the terminal looks
again, and runs the command itself or at the next owner.

A message to the owner is at most 5 MiB: a response on standard input reaches a
running desktop up to 4 MiB (give `--file PATH` beyond that), and an answer
larger than the channel carries is `over_limit`, with anything already written
kept.

## Not in this build

`round read`, its pages and response-only grants for an external harness are
[[BPR-1169]]'s; the portable skill is [[BPR-1170]]'s. The parity course across
desktop, terminal and external consumer is [[BPR-1171]]'s. `publication` commands are not
offered here.
