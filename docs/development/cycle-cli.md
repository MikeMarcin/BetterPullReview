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

The preset builds without agent integrations ([[BPR-1179]]): the installed
`TourCLI` component is `bin/bpr`, the process supervisor its Git launches run
inside, and their private Qt, with no agent adapter or MCP helper. `bpr` is
local automation. A graph built with `BPR_BUILD_LOCAL_AUTOMATION=OFF` cannot
build it, and that desktop binds no owner command endpoint. To remove `bpr`,
delete the files listed in `build/tour-cli/install_manifest_TourCLI.txt`. The
desktop and its stored cycles are untouched; see
[release packages](release-packages.md#packages-without-agent-integrations-or-local-automation).

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
| `review [RC-n] [one of --include/--exclude/--earlier/--later In, --action A, --goal-file PATH\|-, --context PATH [--context-lines A:B], --drop-context PATH] [--expected-revision N]` | The next round's draft and the exact document it freezes as; one change per command. `--context` includes a file, or some of its lines, of the revision under review for the recipient to read. |
| `freeze [RC-n] [--expected-revision N]` | Freezes the draft revision you read into an immutable round. |
| `export [RC-n] --round Fn --output PATH\|- [--format markdown\|bundle] [--replace SHA256]` | A frozen round exactly as frozen, to a chosen file or standard output. |
| `compare [RC-n] [--from Rn] [--to Rn] [--changed-only] [--cursor PATH] [--path P]` | Files of a capture or a pair, or one file's rows, from retained bytes. |
| `response import [RC-n] --round Fn --file PATH\|-` | Appends a recipient's structured response to the round. It changes no note and verifies nothing. |
| `round grant [RC-n] --round Fn [--read-only]` | Exposes one frozen round to a harness: reading it and, unless `--read-only`, adding a response to it. Decided like any export of the round. |
| `round revoke [RC-n] --round Fn` | Withdraws the round's grant. What was read before cannot be recalled. |
| `round read [RC-n] --round Fn [--page-bytes N] [--budget-bytes N]` | For a harness: the granted round's assignment and its first page of items. |
| `round page [RC-n] --round Fn --cursor C` | The next page, by the cursor the previous page gave. |
| `round source [RC-n] --round Fn [--cursor N \| --source Sn [--range A:B]]` | The round's own source attachments: listed, or one's bytes. |
| `response append [RC-n] --round Fn --file PATH\|-` | For a harness whose grant allows it: a structured response to the round. |
| `policy [--classification C --channel CH --permission P]` | The profile's disclosure policy: what each channel decides for each kind of source; or one channel set. |

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
owner-private named pipe on Windows; protocol 4) and the owner runs it on its own store, in
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

Windows showing the cycle read it again after a command writes it. Reading
again moves no window ([[BPR-1171]]): Home, a pull request or another cycle in
front stays in front, a note being written keeps its text and the keyboard,
and only the reviewer's own Open Project or resume brings a cycle's page
forward. What the terminal and a harness are told does not depend on whether
the desktop is open: the same words, data, pages, source bytes and refusals
either way, because the owner runs the same command code. While the desktop
shows the next round's draft:

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

## A harness reading one round

A harness that can run `bpr` reads a frozen round without a copy, a file or
another model ([[BPR-1169]]). The reviewer exposes the round:

```sh
bpr cycle round grant RC-3 --round F2
```

The grant is stored in the profile, names exactly that round of that cycle and
its digest, and allows reading it and adding a response to it (with
`--read-only`, reading only); nothing else. Granting is the reviewer's export
of the round through `external_retrieval`, decided by the same
`core::authorize` as Copy, Save and `export`, so a source of unknown origin
needs that channel allowed first (see below). Naming a round is not
permission: without a grant, `round read` is `policy_denied` (`not_granted`)
however exactly the round is named, and a grant for `F2` reads neither `F1` nor
another cycle's `F2`. `round revoke` withdraws it; the grant stays listed by
`show` as revoked, and every later read or response is refused. What was read
before cannot be recalled.

The harness then runs:

```sh
bpr cycle round read RC-3 --round F2 --json
```

It receives the round's identities (`cycle`, `round`, `payload_sha256`, the
logical project), the assignment, counts (`items_total`, `suggestions_total`,
`sources_total`, `instruction_bytes`, `source_bytes`) and the first page of
items, each exactly as the canonical payload holds it. A page holds whole
items up to `--page-bytes` (1 MiB by default, 4 KiB to 2 MiB) and never cuts
one; `next` is the cursor of the next page, or null on the last, and
`complete` is false until then. The harness reads every page with `round page
--cursor C` before acting on any, and checks that every page names the same
`payload_sha256` and that the items it holds run from `0` to `items_total`. A
cursor names its round, digest, position and page size: one from another round
or digest, or past the end, is `invalid_input`. A harness that states how much
it can hold with `--budget-bytes N` is told `requires_partition`, with the
round's measure and nothing else, when the instructions are larger, and so is
any reader of a round with an item larger than the largest page: the reviewer
splits it into rounds. A read writes nothing; repeating one returns the same
bytes.

`round source` lists the round's own attachments (`S1`, `S2`, … with path,
revision, kind, range and digests, 256 at a time) and returns one's bytes
(`--source S1`, optionally `--range START:END` within them, 1 MiB at a time,
base64 with their SHA-256 in JSON). Nothing else of the project is readable this
way: not another file of the capture, a note or draft outside the round, or the
store. The working folder may find the cycle (`round read --round F2` inside the
folder it is bound to), never a window's selection.

`response append` adds a structured response under a grant that allows it,
recorded as received through the local interface (`local_interface` in
`show`). It is the recipient's statement: it changes no note, wording or round,
verifies nothing and publishes nothing, and its claimed author, provider and
model are kept as claims. `round grant`, `round revoke`, `policy`, `export`,
`review`, `freeze`, `note` and every other command are the reviewer's, on the
reviewer's authority: a harness's grant reaches none of them.

A harness may declare itself for every command it runs with
`BPR_CYCLE_ROLE=consumer` in its environment. `bpr` then refuses every command
but `round read`, `round page`, `round source`, `response append` and
`capabilities` with `policy_denied`. That confines a cooperating harness; it is
not containment of another process running as the same user, which needs the
operating system's or the harness's own policy.

## Disclosure policy

`bpr cycle policy` shows what each channel (`clipboard`, `local_file`,
`terminal_output`, `external_retrieval`, `agent_adapter`) decides for each kind
of source (`unclassified`, `public_source`, `enterprise_restricted`), and
whether that is configured or the default. A folder or checkout is
`unclassified` until its classification is observed, and channels that reach
another program (`external_retrieval`, `agent_adapter`) are not allowed for it
until configured:

```sh
bpr cycle policy --classification unclassified --channel external_retrieval --permission allowed
```

`--permission unconfigured` returns a channel to its default. The policy is
the profile's, stored with its cycles, and every decision combines it with any
policy the host imposes, a denial in either standing: the desktop's Compose
feedback routes, its Copy, Save and Use in external agent, `export`, `round
grant` and every harness read get the same answer. A denial made after a grant
holds against it.

## The bpr-cycle skill

`packaging/agent-skills/bpr-cycle/` is the ready-to-import Agent Skills package
for a harness that consumes a round ([[BPR-1170]]): `SKILL.md` plus three small
references (commands and fields, responses, refusals), and nothing to execute.
It teaches only the consumer commands, each run with `BPR_CYCLE_ROLE=consumer`
and `--json`: `capabilities` once, `round read` and every `round page` before
acting, `round source` for the round's own attachments, and, only when the user
asks and the grant allows, `response append` on standard input. It keeps the
harness's own tools, approvals and orchestration, starts no model, publishes
nothing, and sends the user to the reviewer for a grant, a policy change or a
split round rather than working around a refusal. Without `bpr`, it works from
the round's Markdown and replies in prose by item ID; the cycle needs no
callback.

`skill_bpr_cycle_package` (in a graph with `-DBPR_BUILD_TOUR_CLI=ON`) pins the
text: identity (`bpr-cycle`, contracts `bpr.cycle.cli-result.v1` and
`bpr.cycle.response.v1`), portable frontmatter with no tool grant, thinness,
that every command line it teaches is a declared consumer's and every other
`bpr cycle` command it names is a prohibition, that a publisher, the database,
a second model or an installation appears only as a prohibition, and that every
field, value and refusal it names is one the CLI says or decodes. It then runs
the documented lines, placeholders filled, through the real `bpr` against a
local cycle: refused without a grant (the harness cannot grant itself), then
read whole at the default page and at 4 KiB pages, from the bound folder, with
a partition refusal; attachments listed and read; the responses reference's own
example appended as a `local_interface` statement that changes no note or round;
every non-consumer command the binary lists refused to the declared consumer; a
read-only grant, a denying or unconfigured policy and a revoked grant (mid-page
too) each refused as the refusals reference says; and the reviewer's Markdown
export carrying every item for a person with no skill. Fourteen deliberately
broken copies of the package must fail its lint, among them a description YAML
cannot read as a plain scalar. It cannot show that a model follows the text.

The canonical home is the shared AgentSkills repository (`skills/bpr-cycle/`),
installed by MASK, as for [`pr-tour`](tour-cli.md). This repository selects it
from `agentskills` in `.mask.json`; its `.claude/skills/bpr-cycle` and
`.agents/skills/bpr-cycle` are MASK links into the MASK store and are ignored.
The row fails on a real directory there, on a selection from another source, or
on a deployed copy whose digest differs from the package's. Change the package
here, then import it unchanged into AgentSkills and run `mask upgrade bpr-cycle`;
check the import with `python3 tests/scripts/bpr_cycle_skill_tests.py
PACKAGE_DIR BPR --digest` on both copies. The owner authorized the first import
in attended item `BPR-1170-rc58-08-distribution-v001`.

## Not in this build

The parity course across desktop, terminal and external consumer is
[[BPR-1171]]'s. The desktop does not list Use in external agent yet ([[BPR-1161]]'s route stays hidden until it is
enabled there); its host side stores the same grant `round grant` does.
`publication` commands are not offered here.
