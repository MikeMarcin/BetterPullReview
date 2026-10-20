# Review Cycles from the terminal (`bpr cycle`)

`bpr cycle` lets you work with local Review Cycles from a terminal: open a
folder as a cycle, write notes, freeze a round and export it. It also lets an
agent harness read one round you have granted it and add a response.

This page is for two readers:

- someone running `bpr cycle` by hand or from a script;
- someone wiring an agent harness to read rounds (start at
  [A harness reading one round](#a-harness-reading-one-round)).

`bpr cycle` is the same `bpr` executable as [`bpr tour`](tour-cli.md). It uses
the same cycle services as the desktop app and gives the same answers. It never
starts a window, display, account, provider, credential vault, agent, browser
or network connection.

## Quick start

From the project folder, open it as a cycle, add a note, then freeze and export
the round:

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
- `review` shows the next round's draft and ends with the exact `freeze`
  command to run, including the draft revision you just read.
- `freeze` makes an immutable round (`F1`, `F2`, …). In a terminal it shows
  the round and asks you to type `freeze`.
- `export` writes the frozen round as Markdown.

To hand a round to an agent harness instead of a file, grant it; see
[A harness reading one round](#a-harness-reading-one-round).

## Install

Build `bpr` with the `tour-cli` preset, which builds without tests or agent
integrations, and install the `TourCLI` component:

```sh
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix /absolute/install/root
/absolute/install/root/bin/bpr cycle capabilities
```

- The component installs `bin/bpr`, the process supervisor its Git launches run
  inside, and their private Qt. It has no agent adapter or MCP helper.
- `bpr` is local automation. A build configured with
  `BPR_BUILD_LOCAL_AUTOMATION=OFF` cannot build it, and that desktop listens for
  no terminal commands.
- To remove `bpr`, delete the files listed in
  `build/tour-cli/install_manifest_TourCLI.txt`. The desktop and its stored
  cycles are untouched. See
  [release packages](release-packages.md#packages-without-agent-integrations-or-local-automation).

The `dev` preset leaves `bpr` out. To build it with its tests in a dev tree:

```sh
cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON
ctest --test-dir build/dev-tour-cli -R "^cycle_cli_public_path$"
```

## Commands

| Command | What it does |
|---|---|
| `capabilities` | Contract, commands, limits and exit codes. Needs no profile. |
| `list [--cursor RC-n]` | One page of this profile's cycles. |
| `open PATH` | Opens a folder or file as a cycle: the cycle already bound to it, or a new one bound to it and captured as `R1`. |
| `show [RC-n]` | The cycle, its notes, frozen rounds, delivery observations and responses. |
| `capture [RC-n] [--again]` | Reads the bound folder now. Identical bytes record nothing unless `--again`. |
| `rebind [RC-n] --to PATH [--confirm CODE]` | Binds the cycle to its folder's new location, or to a replacement, once you confirm the check it shows. See [Moving a project](#moving-a-project). |
| `note [RC-n] [--file PATH [--lines A:B]] [--body-file PATH\|-]` | A note on a line range, on a whole file of the latest capture, or on the whole cycle. |
| `review [RC-n] [one of --include/--exclude/--earlier/--later In, --action A, --goal-file PATH\|-, --context PATH [--context-lines A:B], --drop-context PATH] [--expected-revision N]` | Shows the next round's draft and the exact document it freezes as. Makes one change per command. `--context` includes a file, or some of its lines, from the revision under review for the recipient to read. |
| `freeze [RC-n] [--expected-revision N]` | Freezes the draft revision you read into an immutable round. |
| `export [RC-n] --round Fn --output PATH\|- [--format markdown\|bundle] [--replace SHA256]` | Writes a frozen round exactly as frozen, to a file or standard output. Markdown by default. |
| `compare [RC-n] [--from Rn] [--to Rn] [--changed-only] [--cursor PATH] [--path P]` | The files of a capture or a pair of captures, or one file's rows, from retained bytes. |
| `response import [RC-n] --round Fn --file PATH\|-` | Adds a recipient's structured response to the round. It changes no note and verifies nothing. |
| `round grant [RC-n] --round Fn [--read-only]` | Lets a harness read one frozen round and, unless `--read-only`, add a response to it. Decided like any export of the round. |
| `round revoke [RC-n] --round Fn` | Withdraws the round's grant. What was already read cannot be recalled. |
| `round read [RC-n] --round Fn [--page-bytes N] [--budget-bytes N]` | For a harness: the granted round's assignment and its first page of items. |
| `round page [RC-n] --round Fn --cursor C` | For a harness: the next page, by the cursor the previous page gave. |
| `round source [RC-n] --round Fn [--cursor N \| --source Sn [--range A:B]]` | For a harness: the round's own source attachments, listed, or one attachment's bytes. |
| `response append [RC-n] --round Fn --file PATH\|-` | For a harness whose grant allows it: a structured response to the round. |
| `policy [--classification C --channel CH --permission P]` | Shows the profile's disclosure policy, or sets one channel. See [Disclosure policy](#disclosure-policy). |

### Choosing a cycle

A cycle is named `RC-n`. When you leave it out, the command uses the one active
cycle bound to the working folder:

- several bound cycles are `target_ambiguous`;
- none is `not_found`, with `bpr cycle open .` as the next command.

Nothing remembers a "current" cycle, and no window's selection is consulted.

### Common options

Every command also takes these options. `capabilities` takes only the first
four. A command refuses any option that is neither its own nor one of these
(`unsupported_option`, exit 2).

| Option | What it does |
|---|---|
| `--json` | Writes one result document on standard output; see [Output](#output). |
| `--non-interactive` | Never prompts or opens an editor, even in a terminal. |
| `--schema-version 1` | Names the result schema the caller reads; any other is `unsupported_version`. |
| `--data-root PATH` | The application data folder, an absolute owner-private path. By default the desktop's own (on macOS `~/Library/Application Support/BetterPullReview/BetterPullReview`). |
| `--profile ID\|NAME` | The profile, by numeric ID or exact name. By default the default profile (ID 1). A name two profiles share is `profile_ambiguous`; a name no profile has is `profile_unknown`. |
| `--operation-id ID` | Names the command so that repeating it never applies it twice; see [Repeating a command safely](#repeating-a-command-safely). |

### Help

`bpr` with no arguments names its two command families, `bpr tour` and
`bpr cycle`. `bpr cycle` alone, or `bpr cycle help`, prints the command list
`capabilities` gives.

## Output

By default a command writes plain prose and short lists on standard output.

- An error says why on standard error, followed by a `Next:` command when there
  is one.
- A refusal writes nothing to standard output.
- Terminal controls and bidirectional overrides in text are shown as `\uXXXX`.

### JSON

`--json` writes exactly one `bpr.cycle.cli-result.v1` document on standard
output and nothing on standard error:

```json
{"schema":"bpr.cycle.cli-result.v1","command":"cycle freeze","operation_id":"…",
 "outcome":"ok","complete":true,"data":{…},"error":null}
```

- An error has `"outcome":"error"`, `"data":null` and
  `"error":{"code","message","next"}`. It adds `detail` when the code has facts
  to give: an existing file's digest, the draft's current revision, or the
  candidates for an ambiguous folder.
- A page with more after it has `"complete":false` and a `next` cursor.
- IDs, revisions, counts and byte sizes are decimal strings.
- `--schema-version 1` is accepted; any other is `unsupported_version`.

### Exit codes

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

A command is interactive only when standard input and standard output are both
terminals and neither `--json` nor `--non-interactive` is given. Otherwise
nothing opens an editor, prompts, or reads standard input unless you gave `-`
for it. End of input is never an answer.

There is no `--yes` or `--force`. Each command that needs a decision has its own
way to state it:

| Command | In a script | In a terminal |
|---|---|---|
| `note` | Needs `--body-file`; without it, `interaction_required`. | Opens the trusted editor: `BPR_TOUR_EDITOR_ARGV`, or `VISUAL`/`EDITOR` as one absolute path, as for tours. With none configured, `interaction_required`. |
| `review` (a change) | Names the draft revision it read with `--expected-revision`; a changed draft is `stale_revision`. Without it, `interaction_required`. | `--expected-revision` is optional. |
| `freeze` | Names the draft revision it read with `--expected-revision`; a changed draft is `stale_revision`. | Shows the round and asks for the word `freeze`. |
| `export` | Never replaces other bytes at the destination without `--replace` naming exactly their SHA-256, as the `conflict` reported it. | Asks for the word `replace`. |
| `rebind` | Binds nothing without `--confirm` naming exactly the code its check gave (`interaction_required`, exit 3, with the check and the code). | Shows the check and asks for the word `rebind`. |

Some limits and rules apply in either case:

- A note is at most 16 KiB.
- `response import --file` reads one regular file of at most 16 MiB and never
  follows a link. A larger file is `over_limit`; a link or special file is
  `source_unavailable`.
- `export` leaves a file that already holds the round as it is.
- A round cannot carry a note on a file whose name holds a control character.
  `review` shows the refusal `unportable_path`, and `freeze` is `invalid_input`
  until that note is left out.

## Exports and what is recorded

`export` reads the named frozen round, checks the channel against the
[disclosure policy](#disclosure-policy) (local file or terminal output), and
writes the document or bundle. A refused channel, or an output over budget,
writes nothing and falls back to no other channel.

What the cycle records depends on where the output went:

| Output | What is written | What is recorded |
|---|---|---|
| `--output PATH` | The exact bytes | `saved`, under the file's name |
| `--output -` to a pipe or file | The exact bytes | `retrieved` by `terminal_output` |
| `--output -` to a terminal | The text with controls neutralized, not the exact bytes | Nothing |

No observation means a round was received, read, acted on or accepted.

## Working while the desktop is open

### Who runs the command

A profile has one writer at a time: whichever BetterPullReview process holds it.

- If no other process holds the profile, the command runs in the terminal and
  lets go of the profile while a person answers a prompt.
- If the desktop (or another BetterPullReview process) holds it, the command is
  sent to that process, the *owner*, which runs it on its own store in a session
  of its own beside its windows. The channel is same-user only: a local socket
  beside the desktop's activation endpoint on macOS and Linux, an owner-private
  named pipe on Windows; protocol 6.

The terminal never opens the store a second time, takes over the profile, or
starts the desktop.

What is sent is one command from the fixed command list with that command's own
options, the absolute working folder, the absolute paths it names, and any text
the terminal already read (a note or goal, or a response from standard input).
Prompts, the trusted editor and standard output stay in the terminal:

- an interactive `freeze` reads the draft through the owner, asks, then sends
  the freeze with the revision it showed;
- `export --output -` receives the bytes, writes them, then sends what happened
  (`export record`);
- files named by `export --output` and `response import --file` are written and
  read by the owner, as the same user.

A message to the owner is at most 5 MiB. A response on standard input reaches a
running desktop up to 4 MiB; give `--file PATH` beyond that. An answer larger
than the channel carries is `over_limit`, and anything already written is kept.

### What the desktop shows

The terminal and a harness get the same words, data, pages, source bytes and
refusals whether the desktop is open or not, because the owner runs the same
command code.

Windows showing the cycle read it again after a command changes it. Reading
again moves no window: Home, a pull request or another cycle in front stays in
front, and a note being written keeps its text and the keyboard. Only the
reviewer's own **Open Project** or resume brings a cycle's page forward.

While the desktop shows the next round's draft:

- A goal typed and not yet entered is saved first. `review` reads it, and a
  `freeze` against the revision before it is `stale_revision`, never a round
  without the goal.
- While an input method is composing in the goal, `review` and `freeze` answer
  `draft_busy` (exit 4) and change nothing. Notes and exports are not held.
- A desktop edit refused because the terminal changed the draft first keeps the
  typed goal in its field to save again. A desktop note is added to the cycle as
  it now is rather than dropped.

Review Cycles have no undo in either interface. Reverse a change with its
counter-change (`--exclude` after `--include`, or a new goal), as an ordinary
edit under the revision you read, from either side.

### Repeating a command safely

`--operation-id ID` identifies a command: 1 to 64 letters, digits or hyphens.
Without it one is generated, and every JSON result names it. The command's
writes are derived from the ID, so repeating a command never applies it twice.

- An owner answers a repeat exactly as it answered the first time.
- The same ID with a different request is `conflict`.
- If an answer is lost, the command exits 6 (`uncertain`). Read the cycle, or
  repeat the command with the same `--operation-id` to get the owner's answer
  again.
- An owner of another protocol version is `unsupported_version` and is sent
  nothing. An owner offering no channel is `capability_unavailable`.
- If the owner exits mid-command, the terminal looks again and runs the command
  itself or at the next owner.

A command the terminal runs itself reads the cycle before it writes. So a repeat
after that command already wrote reads the cycle the first run changed: its
request no longer matches the first one, and the repeat is `conflict`, saying an
earlier run with the ID may have completed. A goal or freeze naming a draft
revision the first run moved is `stale_revision`. Neither applies anything a
second time. A terminal killed after writing, or an owner killed mid-command
when the terminal then runs the repeat itself, can leave the same result. In
every such case, read the cycle.

### When the folder or store is not what it was

- A capture whose folder is no longer the one the cycle was bound to (moved
  away, replaced, recreated, or replaced by a link to another folder) is
  `source_unavailable`. Capturing again reads the same folder, so its `Next:` is
  `bpr cycle rebind RC-n --to PATH`, not a retry; see
  [Moving a project](#moving-a-project). If the folder is moved back, it
  captures into the cycle again. A different folder now at that location opens
  as a cycle of its own.
- A store SQLite cannot read, in its header or its pages, is
  `storage_unavailable` and reported as damaged, never as another version's. It
  is left as it is, and nothing is created in its place.
- Retained bytes that a revision lists but the store no longer holds are
  `source_unavailable` ("are missing"), never "no file". `not_found` is kept for
  a path the revisions do not list.

## Moving a project

A cycle reads its folder where it was opened. If the folder is moved, renamed
or replaced by another checkout, `capture` refuses it, and `bpr cycle open` on
it still names the cycle it belongs to without reading it from there. A rebind
moves the cycle's binding to the folder's new location, or to a replacement
checkout, with the same rules as the desktop's **Rebind Folder…**:

```sh
bpr cycle rebind RC-n --to /new/location/of/the/project
```

1. Without `--confirm`, nothing is bound. The command reads the folder under the
   cycle's scope and compares it with the latest capture: how many files are the
   same, changed, missing and new.
2. It refuses with `interaction_required` (exit 3), giving the check and a
   16-character confirmation code in `detail`, and the full command in `Next:`.
   In a terminal it shows the check and asks for the word `rebind` instead.
3. Run the command again with `--confirm CODE`. The code covers the cycle as it
   stood, the binding it replaces, the folder as observed and the check, so it
   confirms exactly what was shown. If the cycle or the folder changed in
   between, the old code is `stale_revision` (exit 4), with the new check and
   code.

Rules:

- A rebind never takes another cycle's folder: a folder already bound to
  another active cycle is `conflict` (`bound_elsewhere`).
- A folder cycle is never rebound to a file, or a file cycle to a folder
  (`invalid_input`).
- A folder that cannot be read is `source_unavailable`.
- If the cycle is already bound to that folder at that location, nothing changes
  and nothing is asked.
- A rebind keeps every capture, note and round and captures nothing. The next
  `capture` reads the new location.
- Rebinding to a checkout with other bytes is allowed once confirmed. The check
  says how they differ, and the binding records it.

## A harness reading one round

A harness that can run `bpr` can read a frozen round directly, without a copy,
a file or another model.

### 1. The reviewer grants the round

```sh
bpr cycle round grant RC-3 --round F2
```

The grant is stored in the profile and names exactly that round of that cycle
and its digest. It allows reading the round and adding a response to it (with
`--read-only`, reading only), and nothing else.

Granting counts as the reviewer exporting the round through
`external_retrieval`. It is decided the same way as **Copy**, **Save** and
`export`, so for a source of unknown origin that channel must be allowed first;
see [Disclosure policy](#disclosure-policy).

> [!IMPORTANT]
> Naming a round is not permission. Without a grant, `round read` is
> `policy_denied` (`not_granted`) however exactly the round is named. A grant
> for `F2` reads neither `F1` nor another cycle's `F2`.

`round revoke` withdraws the grant. `show` still lists it, as revoked, and every
later read or response is refused. What was read before cannot be recalled.

### 2. The harness reads every page

```sh
bpr cycle round read RC-3 --round F2 --json
```

The result holds:

- the round's identities: `cycle`, `round`, `payload_sha256` and the logical
  project;
- the assignment;
- counts: `items_total`, `suggestions_total`, `sources_total`,
  `instruction_bytes`, `source_bytes`;
- the first page of items, each exactly as the round's canonical payload holds
  it.

A page holds whole items up to `--page-bytes` (1 MiB by default, 4 KiB to 2 MiB)
and never cuts one. `next` is the cursor of the next page, or null on the last,
and `complete` is false until then.

The harness:

1. reads every page with `round page --cursor C` before acting on any;
2. checks that every page names the same `payload_sha256`;
3. checks that the items run from `0` to `items_total`.

A cursor names its round, digest, position and page size. One from another
round or digest, or past the end, is `invalid_input`.

A harness that states how much it can hold with `--budget-bytes N` is told
`requires_partition`, with the round's measure and nothing else, when the
instructions are larger. So is any reader of a round with an item larger than
the largest page. In both cases the reviewer splits the work into smaller
rounds.

A read writes nothing, and repeating one returns the same bytes.

### 3. The harness reads attachments it needs

`round source` lists the round's own attachments (`S1`, `S2`, … with path,
revision, kind, range and digests), 256 at a time. With `--source S1`, and
optionally `--range START:END` within it, it returns one attachment's bytes,
1 MiB at a time, as base64 with their SHA-256 in JSON.

Nothing else of the project is readable this way: not another file of the
capture, not a note or draft outside the round, and not the store. The working
folder can find the cycle (`round read --round F2` inside the folder it is bound
to), but a window's selection never does.

### 4. The harness responds, if allowed

`response append` adds a structured response under a grant that allows it. It
is recorded as received through the local interface (`local_interface` in
`show`).

A response is the recipient's statement. It changes no note, wording or round,
verifies nothing and publishes nothing. Its claimed author, provider and model
are kept as claims.

### What the harness cannot do

`round grant`, `round revoke`, `policy`, `export`, `review`, `freeze`, `note` and
every other command belong to the reviewer. A harness's grant reaches none of
them.

A harness can declare itself for every command it runs by setting
`BPR_CYCLE_ROLE=consumer` in its environment. `bpr` then refuses every command
but `round read`, `round page`, `round source`, `response append` and
`capabilities` with `policy_denied`.

> [!NOTE]
> `BPR_CYCLE_ROLE=consumer` confines a cooperating harness. It does not contain
> another process running as the same user; that needs the operating system's
> or the harness's own policy.

## Disclosure policy

`bpr cycle policy` shows what each channel (`clipboard`, `local_file`,
`terminal_output`, `external_retrieval`, `agent_adapter`) decides for each kind
of source (`unclassified`, `public_source`, `enterprise_restricted`), and
whether each decision is configured or the default.

A folder or checkout is `unclassified` until its classification is observed,
and the channels that reach
another program (`external_retrieval`, `agent_adapter`) are not allowed for it
until configured. This build observes no classification for a local folder or
checkout, so every cycle decides as `unclassified`, and the other two rows apply
to none yet.

To let a harness read granted rounds of a local folder, allow
`external_retrieval`:

```sh
bpr cycle policy --classification unclassified --channel external_retrieval --permission allowed
```

- The desktop's **Send to Agent** uses `agent_adapter`. Allow it the same way
  before a round of a local folder can be sent to an installed agent. Allowing
  the channel does not allow every agent: the agent host policy each profile
  already follows still refuses an agent a corporate host forbids.
- `--permission unconfigured` returns a channel to its default.
- The policy belongs to the profile and is stored with its cycles. Every
  decision combines it with any policy the host imposes, and a denial in either
  wins. The desktop's **Compose feedback** routes, its **Copy**, **Save** and
  **Use in external agent**, `export`, `round grant` and every harness read get
  the same answer.
- A denial made after a grant holds against that grant.

## The bpr-cycle skill

`packaging/agent-skills/bpr-cycle/` is a ready-to-import Agent Skills package
for a harness that consumes a round. It is `SKILL.md` plus three small
references (commands and fields, responses, refusals), with nothing to execute.

It teaches only the consumer commands, each run with `BPR_CYCLE_ROLE=consumer`
and `--json`:

1. `capabilities`, once;
2. `round read` and every `round page`, before acting;
3. `round source`, for the round's own attachments;
4. `response append` on standard input, only when the user asks and the grant
   allows it.

The skill keeps the harness's own tools, approvals and orchestration. It starts
no model and publishes nothing. For a grant, a policy change or a split round,
it sends the user to the reviewer rather than working around the refusal.
Without `bpr`, it works from the round's Markdown and replies in prose by item
ID; the cycle needs no callback.

### How the package is tested

`skill_bpr_cycle_package` (in a build with `-DBPR_BUILD_TOUR_CLI=ON`) checks the
text:

- its identity (`bpr-cycle`, contracts `bpr.cycle.cli-result.v1` and
  `bpr.cycle.response.v1`) and portable frontmatter with no tool grant;
- that it stays thin;
- that every command line it teaches is a declared consumer's, and every other
  `bpr cycle` command it names appears only as a prohibition;
- that a publisher, the database, a second model or an installation appears
  only as a prohibition;
- that every field, value and refusal it names is one the CLI says or decodes.

It then runs the documented lines, placeholders filled, through the real `bpr`
against a local cycle:

- refused without a grant (the harness cannot grant itself);
- read whole at the default page size and at 4 KiB pages, from the bound folder,
  with a partition refusal;
- attachments listed and read;
- the responses reference's own example appended as a `local_interface`
  statement that changes no note or round;
- every non-consumer command the binary lists refused to the declared consumer;
- a read-only grant, a denying or unconfigured policy and a revoked grant (also
  mid-page) each refused as the refusals reference says;
- the reviewer's Markdown export carrying every item for a person with no skill.

Fourteen deliberately broken copies of the package must fail its lint, among
them a description YAML cannot read as a plain scalar. The test cannot show that
a model follows the text.

### Changing the package

The canonical home is the shared AgentSkills repository (`skills/bpr-cycle/`),
installed by MASK, as for [`pr-tour`](tour-cli.md). This repository selects it
from `agentskills` in `.mask.json`. Its `.claude/skills/bpr-cycle` and
`.agents/skills/bpr-cycle` are MASK links into the MASK store and are ignored by
Git. The test fails on a real directory there, on a selection from another
source, or on a deployed copy whose digest differs from the package's.

1. Change the package here.
2. Import it unchanged into AgentSkills.
3. Run `mask upgrade bpr-cycle`.
4. Check the import on both copies with
   `python3 tests/scripts/bpr_cycle_skill_tests.py PACKAGE_DIR BPR --digest`.

## Not in this build

- The desktop does not list **Use in external agent** yet. Its host side stores
  the same grant `round grant` does; for now, give a harness a round with
  `round grant`.
- Publishing is the desktop's alone (**Share notes > Publish as Pull Request…**).
  No `bpr cycle` command plans, publishes, recovers or links a pull request.
- No command imports a `.bpr-review.zip` bundle. `export --format bundle` writes
  one, and BetterPullReview can validate one, but nothing shipped reads it in
  yet.
- A round cannot be sent to an agent from here. **Send to Agent** is the
  desktop's, in a build with agent integrations.

Related: `application_review_cycle_parity_public_path` checks that the desktop,
the terminal and a granted harness give the same answers about one cycle, with
the desktop open or closed. `cycle_cli_documentation` checks this page's
commands, options, exit codes, limits and examples against the built `bpr`.
