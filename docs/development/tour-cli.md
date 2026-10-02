# Tour terminal development

The `tour-cli` preset builds the separate `bpr` executable with tests disabled:

```sh
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix /absolute/install/root
/absolute/install/root/bin/bpr tour capabilities
/absolute/install/root/bin/bpr tour capabilities --json
```

The same executable serves local Review Cycles as `bpr cycle`; see
[Review Cycles terminal](cycle-cli.md).

`BPR_BUILD_TOUR_CLI` is independent of `BPR_BUILD_QT`. Core-only remains Qt-free;
the terminal uses `QCoreApplication`, Qt Core/Network, the existing GitHub and Git
adapters, and the profile-private SQLite services. It does not initialize a
desktop window or load Qt GUI, Widgets, QPA, WebEngine, syntax, tray, or updater
services. The Git adapter retains its existing isolated process supervisor.

The macOS development install deploys Qt Core, Qt Network and its native TLS
plugin with paths relative to `bin/bpr`. Other platforms currently use their
installed Qt runtime. This development install is not a signed release package;
release dependency notices, corresponding source, native qualification and
distribution are tracked by the extension's later verification milestone.

Capability discovery reports implemented commands explicitly. Human text is the
default; `--json` writes one `bpr.tour.cli-result.v1` envelope. Unsupported options
refuse before command execution. Available commands include `connect`, `open`, `status`, `show`, `edit`, `add`,
`move`, `remove`, `ready`, `undo`, `redo`, `walk`, `preview`, `publish`, `recover`, and `capabilities`.
`open` accepts a full HTTPS PR URL from any directory, or a decimal PR number
with `--repo HOST/OWNER/REPO`. Multiple roots require `--root` with a returned
candidate label or a unique review URL/ID. `open --offline` uses one exact saved
target and reports stale material; it never switches accounts or signs in.

The first open creates an owner-private local default profile through the shared
initializer. Native sign-in is explicit: `bpr tour connect --host github.com
--transport native` in a terminal uses the existing device flow and vault. It
prints the verification URL without launching a browser. Unsupported native
vaults and GHES device enrollment refuse with setup instructions. Explicit
`--transport gh` opts into the existing macOS gh executable/configuration trust
boundary and selects its configured identity; it never extracts tokens.

Machine reads after open require `--profile` and the opaque `draft_id` returned
by open. Every ID and revision is a decimal string (the opaque draft handle is a
SHA-256 lookup key). The target catalog includes profile, host authority,
account, repository, PR and draft; it contains no global last-opened pointer.
A profile another process owns is not opened a second time: local draft commands are forwarded to that owner (see "Working while the desktop is open"). `status` pages stop
summaries using `--cursor`; `show problem|solution|stop:ID` preserves raw Markdown
in JSON and neutralizes terminal controls in human output. `show stop:ID --source` requests bounded pinned source; unavailable material is
reported explicitly while the current comment remains readable. No worktree
file is substituted. Source uses the same pinned endpoint/blob services exercised by the installed-Git fixture.

`--data-root` selects an absolute private application root for isolated work.
Existing non-private roots are refused, never chmodded into ownership. Local
schemas 1 and 2 migrate atomically to schema 3 while retaining legacy tour
payloads. Corrupt or unknown catalogs remain errors, not empty drafts.
`edit problem|solution|stop:ID[:title|body] --file PATH|-` imports one exact
UTF-8 field (maximum 65536 bytes), retaining line endings. Machine edits require
`--expected-revision` from the returned draft. The whole document revision is a
conservative compare-and-swap boundary: even a different field changing requires
another snapshot. Profile ownership is released while input is awaited. An
explicit empty stdin clears the selected field. Invalid UTF-8, NUL, oversized
input and stale revisions leave the draft unchanged. Unix stdin and remote
waits respond to SIGINT/SIGTERM with exit 130 after draining owned work.

`add PATH:START-END`, or `add --path PATH --lines START-END`, pins one-based
inclusive lines, up to 200. Default side is RIGHT at the draft's pinned head;
LEFT and historical `--commit` are explicit. It uses the existing epoch and Git
cache to verify the selected commit/blob/path. Optional `--file PATH|-` imports
the comment body; title defaults to the path and can be edited separately.
Source inspection can make authorized remote reads and never content writes.
`move ID --before ID|--after ID` and `remove ID` retain stable identities and
published discussions. `ready ID` records an explicit relayed current-content
decision in payload version 8; it does not invent a human visit or a publication
grant. Older payloads retain an unspecified origin. Status distinguishes
`ready` from `can_ready`.

Desktop and terminal authoring use `application::review_tour_host::execute` with
the exact owner, draft and expected revision. Admission and durable save are
separate observations. The parent importer, source semantics and staged
publication services remain authoritative.

## Trusted editor and local history

An interactive `edit FIELD` opens one private Markdown projection. Set
`BPR_TOUR_EDITOR_ARGV` to a JSON array containing an absolute executable followed
by its arguments, for example `["/absolute/path/to/editor", "--wait"]`.
`VISUAL` and `EDITOR` accept one absolute executable path; arguments require the
JSON form. Configuration is user supplied; repository editor settings and shell
commands are not evaluated. Choose a blocking editor mode. Its own plugins,
backups and synchronization remain governed by that editor.

The profile lease is released while the editor runs. Successful import requires
the captured whole-draft revision, valid bounded UTF-8, and an ordinary private
projection file. Cancellation, editor failure, invalid input or a competing edit
retains the candidate and prints its absolute path. Read the latest draft before
using `--file` to import a retained candidate. Noninteractive commands require
explicit `--file PATH` or `--file -` and never open an editor.

`undo` and `redo` use the same local journal as the desktop. Each successful
import is one labeled transaction; content and history are saved in one SQLite
compare-and-swap record. Reopening preserves up to 16 transactions and 8 MiB in
each direction. Revisions always advance, including undo. Observing a changed
head or remote publication invalidates the local history. Undo changes local
content; it grants no authority to change a published discussion. Schema 3 reads
legacy draft payloads and initializes an empty journal without inventing history.

## Walk and local preview

`walk` is a line-oriented terminal session. Each page shows the current comment
and up to 200 numbered lines of exact pinned source, or a visible unavailable
reason. Choose Edit comment/title, Ready and next, Next, Previous, Move before
or after a stable ID, Remove, or Quit. The profile lease is released at each
prompt; a stale answer refuses instead of changing a newer revision. EOF and
SIGINT cancel without supplying a decision. Ready in this attended flow records
an observed response bound to the displayed revision/source. Merely printing
source or JSON never sets a visit or readiness decision.

`preview` pages the local narrative: Problem, Solution, then one stop per page.
Follow `--cursor` for each next page. It uses the parent Markdown label and
readiness/destination policies, reports incomplete or unqualified publication,
and performs no provider reads or writes. `preview --refresh` explicitly reads
the current PR and the existing publication journal to present the exact plan.

Terminal text has no ANSI color. `--no-color` is accepted; `COLUMNS` controls
plain wrapping from 20 to 240 columns (80 by default). Control/escape and bidi
control characters are displayed as literal escapes. JSON preserves raw text.
Recovery paths remain unwrapped for copying. Status/preview pages name their
continuation, and bounded notices report truncation. No pager is launched;
capabilities explicitly reports this behavior. Width is an approximate Unicode
cell count; native terminal font/emoji behavior remains qualification work.

Local commands have no model or remote-mutation capability. Source reads use the
existing epoch actor, retained Git objects, exact commit/blob/path/side and typed
source lines. Added, left, historical and unchanged files follow the same path;
a dirty checkout cannot supply replacement text.

## Publication and read-first recovery

`publish` refreshes the exact profile/account/PR and previews the shared desktop
plan. `--description` selects the separately named PR body edit; `--compatibility`
selects visible non-atomic comments where qualified. The preview names the actor,
host, repository, head, text, source links and operations. Type `publish` in the
terminal to authorize that finite plan. The profile lease and provider session
are released during the prompt. A fresh owner then rechecks the draft revision,
full plan and live authority before the existing application host issues its
one-use child grants. An intervening edit or account/head change refuses.

Machine/JSON publication returns `interaction_required` (exit 3). There is no
agent approval bridge or `--yes` flag. Readiness, a saved plan ID, piped input and
an agent's prose cannot authorize publication. Preview/recovery/local edits make
zero GitHub writes. Refused or oversized exact previews authorize nothing.

`recover [--attempt ID]` discovers and reconciles the existing durable journal.
It retains actual review/comment identities and reports staged author-only data,
unknown outcomes, partial updates, conflicts and completed publication separately.
`publish --resume ID` requires a fresh terminal preview and confirmation for the
remaining captured operations. An ambiguous possibly-transmitted write is never
retried. A newer local draft is preserved for explicit reopen/reconciliation;
a publication receipt cannot replace newer local wording.

Terminal SIGINT/SIGTERM drains the publication host; uncertain or incomplete
writes report exit 6 and retained status. After abrupt process death, recover
before resuming. A server pending review without a matching local attempt is a
candidate only: it grants no adoption, edit, deletion or submit authority. Private
unsent text/history and lost approval cannot be reconstructed from it.

Production Tour operations retain the parent's unqualified host/transport gate.
The native and admitted gh provider compositions exist, but fixture acceptance
does not enable live Tour writes. Unsupported vaults, platforms and authority
remain typed refusals. Live host, native package and actual human/agent
qualification remain later acceptance work. The current exact-plan preview is
bounded to 400,000 UTF-8 bytes; larger plans refuse before prompting.

## Fresh profile reconstruction

Online `open` uses the same ordinary-comment importer and scoped pinned-link
parser as the desktop. It preserves current description text, website route
order, raw baselines, remote discussion IDs and source commit/path/side/range.
Multiple roots require an explicit `--root`; later publication batches can belong
to the original index. A clean profile does not need a copied manifest or cache.

JSON includes `published_root`, per-stop `remote` and `source_binding`, and
`published_observation` (`complete`, `partial`, `stale`, `no_published_root` or
`not_refreshed`). A complete collection may still contain unresolved deleted or
inaccessible route entries; these remain visible and read-only. Missing source
identity remains null, and source bytes require an independent pinned read.
Private WIP, prior AI provenance, readiness and approval absent from this profile
are unknown. An observed source binding does not invent a blob or author visit.

Incomplete collections or raw bodies cannot prepare publication. Accessible
partial routes remain visible and read-only; an incomplete empty response does
not initialize an empty replacement. If a previously bound index disappears,
retained local text is marked stale. Pending reviews and their private comments
are excluded from the published projection and supply no recovery authority.

`recover --attempt ID` can read a completed exact plan from the existing private
journal even after local attribution advanced the draft revision. It reobserves
the provider and reports `publication_state` and real receipts without writes.
Without that journal, a pending server candidate cannot recreate lost approval.

## Working while the desktop is open

The profile has one owner. When the desktop owns it, `status`, `show`, `edit`,
`move`, `remove`, `ready`, `undo` and `redo` are forwarded over a same-user local
socket beside the desktop's activation endpoint; the CLI never opens the store,
takes the lease or starts the desktop. An owner that predates the channel answers
`owner_protocol_unsupported` (exit 4). Provider-reading commands (`add`, `open`,
`show --source`, `walk`, refresh, publish, recover) answer `interaction_required`
while a desktop owns the profile; forwarding source reads is BPR-1227. The
channel is a same-user local socket on macOS and Linux and an owner-private named
pipe on Windows ([[BPR-1168]]), protocol 3; an owner of another protocol version
answers `owner_protocol_unsupported` before anything is sent. The same channel
carries `bpr cycle` commands ([Review Cycles terminal](cycle-cli.md)).

Every local edit carries a request ID (`--request-id`, generated when omitted; at
most 64 letters, digits or hyphens). The owner commits the outcome with the edit,
so repeating the same ID and payload returns the stored outcome and the same ID
with another payload is refused. If an answer is lost the CLI exits 6 with
`outcome_unknown` and does not retry; run `status --request-id ID` to see whether
the owner committed it (`operation_recorded`, `operation_revision`) before
repeating anything. A retry that names its own ID is not refused merely because
the revision has moved.

A draft the desktop has open is edited where its author is. While that author holds
uncommitted input-method text the owner answers `editor_busy` (exit 4) and changes
nothing; retry after the composition ends. A mutation is answered once it is durable,
and `outcome_unknown` covers the rare save that has not landed within five seconds.
Your edit appears in the open editor without moving the author's caret, focus or
selected stop, and a stale edit is a `revision_conflict` that keeps both texts.

## Hooks and assistance for the active agent

`hooks show [HOOK]`, `hooks edit HOOK [--file PATH|-]` and `hooks reset HOOK` read
and change the profile's four writing hooks (problem, solution, outline, comment),
the same overrides the desktop's Agents & context settings edit; each change advances
the hook revision. `hooks show` also works beside a running desktop; `edit` and
`reset` need the profile to themselves (a running desktop's settings are its own).

`assist context` prepares a request for text and launches no model. It resolves the
target, reads the draft, takes the effective hook and returns one JSON packet: the
hook's name, version, revision and instructions, the parent result contract, the
exact field and byte range, the bounded sources, what was omitted, and the full
prompt the desktop would send. `--hook problem --field problem`, `--hook solution
--field solution` or `--hook comment --field stop:ID` name the field. With no
selection the whole field is the target and an empty field is drafted. `--quote
TEXT` resolves one exact current match (zero or several is an error, never widened)
and `--range-bytes BEGIN:END` names half-open UTF-8 bytes that must sit on character
boundaries and never inside a CRLF pair. The outline hook takes the whole draft and,
for its code destinations, `--sources PATH:START-END,...` (standalone only: a running
desktop does not yet read a provider for the CLI).

The request is one ticket in the draft's journal. Issuing another supersedes it. It
is bound to the draft revision, the field's bytes and the hook revision, and expires
after 30 minutes; any edit to the draft, even one that returns the text to identical
bytes, or a hook change makes it stale. It is durable, so the answer may come from a
later process or another owner. `assist cancel` closes it explicitly.

`assist complete --ticket ID --result PATH|-` (the imported contract sketch says
`--input`, which the architecture gate reserves for the GitHub transport) takes exactly
the JSON object the prompt describes. It supplies replacement text and notes only, never a target, range, hook,
approval or ready flag, and any such field makes the result invalid. A valid result
replaces only the ticketed bytes, at once, as one undoable transaction (`Rewrite
Problem`, `Rewrite Solution`, `Rewrite comment`); an identical result changes nothing.
Answering spends the ticket whether or not the result was usable. An outline answer
is a body-free proposal, and `outline use PROPOSAL --indices 0,2 [--after STOP]`
inserts the chosen suggestions as Suggested stops that are neither visited nor ready.
Stop reasons are `stale`, `expired`, `unknown_ticket`, `unknown_proposal`,
`invalid_result`, `invalid_indices`, `ambiguous_quote`, `quote_not_found`,
`invalid_range`, `insufficient_context` and `context_too_large`.

An owner serves `assist` and `outline` like other local commands, from the desktop's
open draft when there is one, and answers with its own clock and hook revision.

## The pr-tour skill

`packaging/agent-skills/pr-tour/` is the ready-to-import Agent Skills package for the
conversational wrapper: `SKILL.md` plus three small references. It teaches only
`bpr tour` commands, keeps no draft, publisher or hook copy of its own, and starts no
model. The canonical home is the shared AgentSkills repository, installed by MASK;
BPR keeps no hand-maintained `.claude/skills` or `.codex/skills` copy. Until that
import is authorized the package here is the candidate, not an installed skill.
`skill_pr_tour_package` checks the package identity (`pr-tour`, contract
`bpr.tour.cli-result.v1`), that it stays thin, that every mention of a publisher, raw
HTTP, the database or an approval bypass is a prohibition, that every command it teaches
exists, and that the real parser accepts each documented command line. It cannot show
that a model follows the text.

## Boundaries an agent session meets

Reading is not visiting. `status`, `show` (with or without `--source`), `preview` and
`assist context` never mark a stop visited or ready; `status` reports each stop's
`suggested`, `visited`, `ready` and `completion_origin`. `ready` names exactly one stop
and the revision just read, so each readiness is its own step against the current
draft; it needs a non-blank title and comment and a valid source binding, so a
body-free outline skeleton cannot be marked ready; and a command-line caller's
completion is always `relayed`. There is no ready-all and no way to declare an
`observed` origin: that comes only from a terminal walk or the desktop.

Publication has no bypass. A machine call refuses with `interaction_required` and the
exact command for the author to run in a terminal; `--yes`, `--approve`, `--provider`,
`--model`, `--agent`, `--token` and `--force` are not options, and a piped `yes` is not
a confirmation. The context packet marks `agent_identity` as `unverified`; the
`--session` label is a caller's claim and is used for nothing. `assist complete`
accepts exactly the parent result contract: an extra key such as `target`, `range`,
`approval`, `provider` or `ready`, a repeated key, or text after the object makes it
invalid and spends the ticket. Untrusted text reaches the agent only as escaped strings
inside the prompt's evidence JSON. The packet carries the draft's own problem and
solution, the selected passage and its neighbours and, for a comment, that stop's own
source; never another stop's comment, an outline's private prompts, earlier text or
undo history, and it lists what it omitted. A request too large to send
(`context_too_large`) is refused before it can disturb a pending one.

A managed host that cannot verify the agent may set `BPR_TOUR_ASSIST_POLICY=manual-only`.
Then `assist context`, `assist complete` and `outline use` answer `context_export_refused`
(exit 5) whatever identity is claimed, an unrecognised value refuses too, and manual
authoring, hooks, preview and publication by the author are unaffected. `capabilities`
reports the policy and `verified_agent_identity: unavailable`. This is a host setting, not a
sandbox: BPR cannot verify who is calling, and a same-user agent with a shell can make any
claim.

### Distribution status

The canonical import is not done, and this repository does not claim it is. The shared
source is the local AgentSkills repository (`ssh://git@github.com/~/MikeMarcin/AgentSkills.git`,
selected here by `.mask.json` as `agentskills`), which has its own contribution contract:
its own RFC and card, explicit-path commits, no unrequested push, and MASK deployment by
links. Importing `pr-tour` is therefore that repository's work, authorized by its owner.
To do it: copy `packaging/agent-skills/pr-tour/` unchanged to `skills/pr-tour/` there,
commit it under that repository's process, add `{"id": "pr-tour", "source": "agentskills"}`
to this repository's `.mask.json`, and run `mask install pr-tour`. Verify the import with
`python3 tests/scripts/pr_tour_skill_tests.py PACKAGE_DIR bpr-fixture-cli --digest`, whose
digest must equal the one recorded in the verification record for the candidate revision.
The package needs only a released `bpr` CLI, not the desktop or this source tree. Which
agent hosts load it, and whether the skill activates for a real user, are separate attended
observations that are not made here.
