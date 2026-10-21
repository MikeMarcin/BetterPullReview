# Review Cycles from the terminal (`bpr cycle`)

`bpr cycle` works with local Review Cycles from a terminal: open a folder as a
cycle, write notes, freeze a round and export it. It also lets an agent harness
read one round you have granted it and add a response; a harness author can
start at [A harness reading one round](#a-harness-reading-one-round).

`bpr cycle` is the same `bpr` executable as [`bpr tour`](tour-cli.md) and uses
the same cycle services as the desktop app. It never starts a window, account,
provider, credential vault, agent, browser or network connection.

## Quick start

From the project folder:

```sh
cd /path/to/project
bpr cycle open .
bpr cycle note --file src/a.txt --lines 2:2 --body-file note.md
bpr cycle review                                # ends with the exact freeze command
bpr cycle freeze --expected-revision 2          # the revision review printed
bpr cycle export --round F1 --output review.md
```

- `open` returns the cycle bound to that folder, or creates one (`RC-1`,
  `RC-2`, …) and captures the folder as its first revision, `R1`.
- `review` shows the next round's draft and ends with the `freeze` command to
  run, including the draft revision you just read.
- `freeze` makes an immutable round (`F1`, `F2`, …). In a terminal it asks you
  to type `freeze`.
- `export` writes the frozen round as Markdown.

## Install

Build with the `tour-cli` preset (no tests or agent integrations) and install
the `TourCLI` component:

```sh
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix /absolute/install/root
/absolute/install/root/bin/bpr cycle capabilities
```

A build configured with `BPR_BUILD_LOCAL_AUTOMATION=OFF` has no `bpr`. To
remove `bpr`, delete the files listed in
`build/tour-cli/install_manifest_TourCLI.txt`; stored cycles are untouched. The
`dev` preset leaves `bpr` out; configure it with `-DBPR_BUILD_TOUR_CLI=ON` to
build it with its tests.

## Commands

| Command | What it does |
|---|---|
| `capabilities` | Contract, commands, limits and exit codes. Needs no profile. |
| `list [--cursor RC-n]` | One page of this profile's cycles. |
| `open PATH` | The cycle bound to a folder or file, or a new one captured as `R1`. |
| `show [RC-n]` | The cycle, its notes, frozen rounds, delivery observations and responses. |
| `capture [RC-n] [--again]` | Reads the bound folder now. Identical bytes record nothing unless `--again`. |
| `rebind [RC-n] --to PATH [--confirm CODE]` | Binds the cycle to its folder's new location; see [Moving a project](#moving-a-project). |
| `note [RC-n] [--file PATH [--lines A:B]] [--body-file PATH\|-]` | A note on a line range, a whole file, or the whole cycle. |
| `review [RC-n] [one of --include/--exclude/--earlier/--later In, --action A, --goal-file PATH\|-, --context PATH [--context-lines A:B], --drop-context PATH] [--expected-revision N]` | Shows the next round's draft and the exact document it freezes as. One change per command. `--context` adds a file, or some of its lines, for the recipient to read. |
| `freeze [RC-n] [--expected-revision N]` | Freezes the draft revision you read into an immutable round. |
| `export [RC-n] --round Fn --output PATH\|- [--format markdown\|bundle] [--replace SHA256]` | Writes a frozen round exactly as frozen, to a file or standard output. |
| `compare [RC-n] [--from Rn] [--to Rn] [--changed-only] [--cursor PATH] [--path P]` | The files of one capture or a pair, or one file's rows. |
| `response import [RC-n] --round Fn --file PATH\|-` | Adds a recipient's structured response. It changes no note and verifies nothing. |
| `round grant [RC-n] --round Fn [--read-only]` | Lets a harness read one frozen round and, unless `--read-only`, respond to it. |
| `round revoke [RC-n] --round Fn` | Withdraws the grant. What was already read cannot be recalled. |
| `round read [RC-n] --round Fn [--page-bytes N] [--budget-bytes N]` | For a harness: the assignment and the first page of items. |
| `round page [RC-n] --round Fn --cursor C` | For a harness: the next page. |
| `round source [RC-n] --round Fn [--cursor N \| --source Sn [--range A:B]]` | For a harness: the round's source attachments, or one attachment's bytes. |
| `response append [RC-n] --round Fn --file PATH\|-` | For a harness whose grant allows it: a structured response. |
| `policy [--classification C --channel CH --permission P]` | Shows the disclosure policy, or sets one channel; see [Disclosure policy](#disclosure-policy). |

When you leave out `RC-n`, the command uses the one active cycle bound to the
working folder. Several are `target_ambiguous`; none is `not_found`, with
`bpr cycle open .` as the next command.

Every command also takes these options. `capabilities` takes only the first
four. Any other option is `unsupported_option`.

| Option | What it does |
|---|---|
| `--json` | Writes one result document on standard output; see [Output](#output). |
| `--non-interactive` | Never prompts or opens an editor. |
| `--schema-version 1` | The result schema the caller reads; any other is `unsupported_version`. |
| `--data-root PATH` | The application data folder, absolute and owner-private. By default the desktop's (on macOS `~/Library/Application Support/BetterPullReview/BetterPullReview`). |
| `--profile ID\|NAME` | The profile, by ID or exact name. By default the default profile (ID 1). |
| `--operation-id ID` | Names the command so that repeating it never applies it twice; see [Working while the desktop is open](#working-while-the-desktop-is-open). |

`bpr` with no arguments names its two command families, `bpr tour` and
`bpr cycle`. `bpr cycle` alone, or `bpr cycle help`, prints the command list
`capabilities` gives.

## Output

By default a command writes plain text on standard output. An error goes to
standard error with a `Next:` command when there is one, and writes nothing to
standard output.

`--json` writes exactly one `bpr.cycle.cli-result.v1` document on standard
output and nothing on standard error:

```json
{"schema":"bpr.cycle.cli-result.v1","command":"cycle freeze","operation_id":"…",
 "outcome":"ok","complete":true,"data":{…},"error":null}
```

- An error has `"outcome":"error"`, `"data":null` and
  `"error":{"code","message","next"}`, plus `detail` when there are facts to give.
- A page with more after it has `"complete":false` and a `next` cursor.
- IDs, revisions, counts and byte sizes are decimal strings.

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

## Scripts and prompts

A command is interactive only when standard input and output are both terminals
and neither `--json` nor `--non-interactive` is given. Otherwise nothing prompts,
opens an editor or reads standard input unless you gave `-`.

There is no `--yes` or `--force`. Each decision has its own option:

| Command | In a script | In a terminal |
|---|---|---|
| `note` | Needs `--body-file`. | Opens the editor (`BPR_TOUR_EDITOR_ARGV`, or `VISUAL`/`EDITOR` as one absolute path). |
| `review` (a change) | Needs `--expected-revision`; a changed draft is `stale_revision`. | `--expected-revision` is optional. |
| `freeze` | Needs `--expected-revision`; a changed draft is `stale_revision`. | Asks for the word `freeze`. |
| `export` | Replaces other bytes only with `--replace` naming their SHA-256. | Asks for the word `replace`. |
| `rebind` | Binds only with `--confirm` naming the code its check gave. | Asks for the word `rebind`. |

Without the option a script gets `interaction_required` (exit 3).

Limits:

- A note is at most 16 KiB.
- `response import --file` reads one regular file of at most 16 MiB and never
  follows a link.

`export` checks the [disclosure policy](#disclosure-policy) first. A refused
channel writes nothing. An export to a file or a pipe records a delivery
observation on the round; one shown in a terminal records nothing. An
observation never means the round was received, read or accepted.

## Working while the desktop is open

A profile has one writer at a time. If the desktop (or another BetterPullReview
process) holds the profile, the command is sent to that process over a same-user
channel (protocol 6), and gives the same answers. Prompts, the editor and output
stay in the terminal. A message to the owner is at most 5 MiB; for a larger
response, give `--file PATH` instead of standard input.

Review Cycles have no undo in either interface: reverse a change with its
counter-change (`--exclude` after `--include`, or a new goal). If an answer is
lost (`uncertain`, exit 6), read the cycle or repeat the command with the same
`--operation-id`: a repeat is never applied twice, and the same ID with a
different request is `conflict`. While someone is composing the round's goal in
the desktop, `review` and `freeze` answer `draft_busy` and change nothing.

## Moving a project

If the folder is moved, renamed or replaced by another checkout, `capture`
refuses it with `source_unavailable`. Rebind the cycle, as with the desktop's
**Rebind Folder…**:

```sh
bpr cycle rebind RC-n --to /new/location/of/the/project
```

1. Without `--confirm`, nothing is bound. The command compares the folder with
   the latest capture (files the same, changed, missing and new) and refuses
   with `interaction_required`, giving the check, a confirmation code and the
   full command in `Next:`. In a terminal it asks for the word `rebind`.
2. Run it again with `--confirm CODE`. If the cycle or folder changed in
   between, the old code is `stale_revision`, with a new check and code.

A rebind keeps every capture, note and round and captures nothing. A folder
bound to another active cycle is `conflict`, and a folder cycle is never
rebound to a file or the reverse.

## A harness reading one round

A harness that can run `bpr` can read a frozen round directly.

### 1. The reviewer grants the round

```sh
bpr cycle round grant RC-3 --round F2
```

The grant names exactly that round and its digest. It allows reading it and
adding a response (with `--read-only`, reading only), and nothing else. A grant
for `F2` reads neither `F1` nor another cycle's `F2`; without a grant,
`round read` is `policy_denied`.

Granting counts as exporting the round through `external_retrieval`, so for a
local folder that channel must be allowed first; see
[Disclosure policy](#disclosure-policy). `round revoke` refuses every later read
or response.

### 2. The harness reads every page

```sh
bpr cycle round read RC-3 --round F2 --json
```

The result holds the round's identities (`cycle`, `round`, `payload_sha256`),
the assignment, the counts (`items_total`, `suggestions_total`, `sources_total`,
`instruction_bytes`, `source_bytes`) and the first page of items.

A page holds whole items up to `--page-bytes` (1 MiB by default, 4 KiB to 2 MiB).
`next` is the next page's cursor, or null on the last. The harness reads every
page with `round page --cursor C` before acting, and checks that every page
names the same `payload_sha256`.

With `--budget-bytes N`, a round whose instructions are larger is
`requires_partition`, as is any round with an item larger than the largest
page; the reviewer then splits the work into smaller rounds. A cursor from
another round or digest is `invalid_input`. A read writes nothing, and
repeating one returns the same bytes. Inside the bound folder, `RC-n` can be
left out.

### 3. The harness reads attachments it needs

`round source` lists the round's attachments (`S1`, `S2`, …), 256 at a time.
With `--source S1` and optionally `--range START:END`, it returns that
attachment's bytes, 1 MiB at a time, as base64 with their SHA-256. Nothing else
of the project is readable this way.

### 4. The harness responds, if allowed

`response append` adds a structured response under a grant that allows it. A
response is the recipient's statement: it changes no note or round and verifies
nothing.

### Confining the harness

Every other command belongs to the reviewer. A harness can declare itself by
setting `BPR_CYCLE_ROLE=consumer` in its environment. `bpr` then refuses every command
but `round read`, `round page`, `round source`, `response append` and
`capabilities` with `policy_denied`.

> [!NOTE]
> `BPR_CYCLE_ROLE=consumer` confines a cooperating harness only. It does not
> contain another process running as the same user.

## Disclosure policy

`bpr cycle policy` shows what each channel (`clipboard`, `local_file`,
`terminal_output`, `external_retrieval`, `agent_adapter`) decides for each kind
of source (`unclassified`, `public_source`, `enterprise_restricted`).

This build treats every local folder as `unclassified`, and the channels that reach
another program (`external_retrieval`, `agent_adapter`) are not allowed for it
until configured. To let a harness read granted rounds, allow
`external_retrieval`:

```sh
bpr cycle policy --classification unclassified --channel external_retrieval --permission allowed
```

- The desktop's **Send to Agent** uses `agent_adapter`; allow it the same way.
  The profile's agent host policy still applies.
- `--permission unconfigured` returns a channel to its default.
- A denial here or in a host-imposed policy wins, for the desktop and the
  terminal alike, and holds against existing grants.

## The bpr-cycle skill

`packaging/agent-skills/bpr-cycle/` is a ready-to-import Agent Skills package
for a harness that consumes a round. It teaches only the consumer commands, run
with `BPR_CYCLE_ROLE=consumer` and `--json`: `capabilities`, `round read` and
every `round page`, `round source`, and `response append` when the user asks and
the grant allows it. For a grant, a policy change or a split round it sends the
user to the reviewer. Without `bpr`, it works from the round's Markdown.

Its canonical home is the shared AgentSkills repository (`skills/bpr-cycle/`),
installed by MASK as for [`pr-tour`](tour-cli.md). To change it, edit the
package here, import it unchanged into AgentSkills, run `mask upgrade bpr-cycle`,
and check both copies with
`python3 tests/scripts/bpr_cycle_skill_tests.py PACKAGE_DIR BPR --digest`.

## Not in this build

- The desktop does not list **Use in external agent** yet; use `round grant`.
- Publishing a pull request is the desktop's alone.
- No command imports a `.bpr-review.zip` bundle; `export --format bundle` only
  writes one.
- **Send to Agent** is the desktop's, in a build with agent integrations.
