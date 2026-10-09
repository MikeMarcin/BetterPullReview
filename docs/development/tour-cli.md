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
plugin with paths relative to `bin/bpr`. The Linux install does the same with
Qt Core, Network, their ICU and the OpenSSL TLS plugin under `lib/`
([[BPR-1267]]), so it needs no Qt on the system. Windows currently uses its
installed Qt runtime. On every platform this is a development install, not a
release package: `packaging/package.py` packages only the desktop's `Runtime`
component, and the `TourCLI` install carries no notices, SBOM or
corresponding-source reference ([[BPR-1068]]). To remove it, delete the files
listed in `build/tour-cli/install_manifest_TourCLI.txt`.

Capability discovery reports implemented commands explicitly. Human text is the
default; `--json` writes one `bpr.tour.cli-result.v1` envelope. Its `platform`
object, and the human Platform line, name this build's platform, its native
vault and whether the gh transport exists there, and say "development install;
no release package" and "not yet qualified" on every platform (see "Platform
status" below). Unsupported options
refuse before command execution. Available commands include `connect`, `open`, `status`, `show`, `edit`, `add`,
`move`, `remove`, `ready`, `undo`, `redo`, `walk`, `preview`, `publish`, `recover`, `hooks`, `assist`,
`outline`, `labels` and `capabilities`.
`open` accepts a full HTTPS PR URL from any directory, or a decimal PR number
with `--repo HOST/OWNER/REPO`. Multiple roots require `--root` with a returned
candidate label or a unique review URL/ID. `open --offline` uses one exact saved
target and reports stale material; it never switches accounts or signs in.

The first open creates an owner-private local default profile through the shared
initializer. Native sign-in is explicit: `bpr tour connect --host github.com
--transport native` in a terminal uses the existing device flow and vault. It
prints the verification URL without launching a browser. Unsupported native
vaults and GHES device enrollment refuse with setup instructions. On Linux the
vault is the Secret Service default collection on the session D-Bus, which BPR
never unlocks, creates or prompts for. `connect` asks it first and, with no
session bus, no Secret Service on the bus or a locked collection, refuses
`vault_unavailable` (exit 5) with "Nothing was requested from GitHub." and the
setup or unlock step, before any code is shown ([[BPR-1068]]). Without a
desktop, start a session with an unlocked keyring first, for example
`dbus-run-session -- sh`, then in that shell `gnome-keyring-daemon --unlock
--components=secrets` with the keyring password on standard input, then
`bpr tour connect`. Explicit
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

## Platform status

Nothing below is a qualification. Each row is what the shipped `bpr` does on
that platform and what has actually been observed ([[BPR-1068]] and the
[RFC-BPR-0044–0046 ledger](../verification/RFC-BPR-0044-0046.md)).

| Capability | macOS | Linux | Windows |
|---|---|---|---|
| `tests-off` install runs with no display | Observed (BPR-1054) | Observed: Ubuntu 26.04 under WSL2, private Qt, `ldd` has no GUI library | Not run |
| Release package | None | None; `package.py` also refuses Ubuntu 26.04 | None |
| First run creates the private default profile | Fixture courses | Observed from the install on a fresh root | Fixture courses |
| Native device sign-in | Composed (Keychain); not run live | Composed (Secret Service, checked first); completion, stored grant and restore pass against the loopback course ([[BPR-1302]]); live completion not run | Composed (Credential Manager); not run live |
| `--transport gh` | Explicit opt-in; loopback course gh rows owed | `transport_unavailable` | `transport_unavailable` |
| Online `open`, refresh, publish, recover | Fixed by [[BPR-1302]]; never-signed-in `open` row owed | Loopback course passes; live not run | Fixed by [[BPR-1302]]; never-signed-in `open` row owed |
| Local draft commands, editor, owner channel | Courses pass | Courses pass | Courses pass (BPR-1227) |

Before [[BPR-1302]] the live remote reader waited on a shutdown-only
predicate, so an online `open`, a gh `connect` and the end of a native sign-in
did not return until interrupted, and a completed native sign-in was not
stored for the next command. No platform has an observed live sign-in, read or
publication; live publication stays behind the unqualified host/transport gate
above.

## Loopback provider course

`tour_cli_loopback_provider` (`tests/scripts/tour_cli_loopback_tests.py`,
[[BPR-1302]]) runs the built `bpr` through the real remote reader, provider
and native transport, with no fixture reader. A tests-enabled `bpr` reads
`BPR_GITHUB_NATIVE_HTTPS_TEST_LOOPBACK=PORT:ABSOLUTE-PEM-PATH` and sends every
native exchange through that one 127.0.0.1 CONNECT proxy, trusting that one
disposable authority beside the system roots: the GitHub adapter's existing
test seam. A tests-disabled build has neither the variable nor the seam. An
invalid value refuses `transport_unavailable`; it never falls back to the
network.

Behind the proxy the course answers as github.com and api.github.com: the
device flow, `/user`, the nine capability parts and the github-com contract
fixture's pull request read, and Git's smart HTTP protocol v2 from a real
`git upload-pack` over the cli-read-v1 objects. On Linux the vault is a
private Secret Service (a private bus and a test-only keyring, as the
`linux_secret_service_*` rows use) and the course runs, each as one `bpr`
process under a 30-second bound: a never-signed-in `open` (exit 5
`authentication_required`, no request), `connect --transport native` on a
pseudo-terminal through the fake device flow, the online `open`, the first
pinned-source read (`add`, which fetches the pinned commits), `show --source`,
`preview --refresh`, `publish` on a pseudo-terminal (a complete draft is read
and refused at the unqualified gate, exit 5, with no write reaching the
fake), and `recover`. A step that does not return is interrupted and reported
as `timeout` with the requests the fake saw; the course goes on to the next
step. Elsewhere only the never-signed-in `open` runs, because the native vault
there is the user's own. On macOS, outside ctest, `--gh` adds `connect` and
`open` over `--transport gh` with the fake `gh` from
`tests/end_to_end/rfc0015_cli_read_fixture.py` on a temporary `PATH`; inside
ctest gh's own `sandbox-exec` cannot start, so those rows report `not-run`.
`--seed-binary` lets a second build prepare the profile for the
failing-before controls.

`connect --transport native` now ends only when the renewal material is stored
under the account `/user` named and the profile's binding is remembered, as
the desktop binds it, and refuses `vault_unavailable` if GitHub authorized a
sign-in that could not be stored. The CLI composes no renewal, so once the
access lifetime has passed the host's restore rule fails the held session at
once and `open` answers `authentication_required` until `connect` runs again
(read from the host; the course does not exercise expiry).

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
`move`, `remove`, `ready`, `undo`, `redo` and `labels edit` are forwarded over a same-user local
socket beside the desktop's activation endpoint; the CLI never opens the store,
takes the lease or starts the desktop. An owner that predates the channel answers
`owner_protocol_unsupported` (exit 4). `add`, `show --source` and `walk` read
pinned source through the owner ([[BPR-1227]]): the desktop window that has the
draft's pull request open reads the exact range with its own provider composition
and the CLI builds the stop as it does standalone, so only the stop is recorded.
A draft no window has open answers `interaction_required` (exit 3); open the pull
request in the desktop and retry. `open`, refresh, publish and recover answer
`interaction_required` while a desktop owns the profile, and `assist --sources`
and `labels counts` do too. The channel is a same-user local socket on macOS and Linux and an
owner-private named pipe on Windows ([[BPR-1168]]), protocol 6; an owner of
another protocol version answers `owner_protocol_unsupported` before anything is
sent. The same channel
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
and change the profile's five writing hooks, the same overrides the desktop's Agents
& context settings edit; each change advances the hook revision. Four are the tour's
(problem, solution, outline, comment, version `review-preparation-v1`). The fifth,
`feedback` ([[BPR-1090]]), is the review-feedback conventions guidance
(`review-feedback-v1`): it writes no tour field, so `assist context --hook feedback`
is `invalid_hook`, and comment workshop requests carry it only while the label helper
is on. Each listed hook gives its `version`, whether it is `customized` and whether it
is `active`; the listing gives `label_helper`, and its text marks the feedback hook
off while the helper is off. An override is kept exactly as written across upgrades;
a hook with none follows the shipped default, and `reset` returns it there. A running
desktop older than the feedback hook answers `hooks show feedback` with
`owner_protocol_unsupported` (exit 4). `hooks show` also works beside a running desktop; `edit` and
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

## Feedback labels

`bpr tour labels` ([[BPR-1092]], RFC-BPR-0051 R38, R39, R41) gives a terminal or an
agent the Conventional Comments rules the desktop uses. Each subcommand is decided by
the core and application functions the desktop calls; none reads a label with a
pattern of its own, keeps a store or publishes anything. The RFC asks for these to be
documented in a `CLI-INTEGRATION.md`; no such page exists, and this page is the CLI's
contract.

| Command | What it answers | Needs |
|---|---|---|
| `labels` or `labels catalog` | The twelve labels in catalog order, each with its description, guidance and icon role; the three standard decorations; the catalog (`bpr.feedback-label-catalog.v1`, revision 1) and grammar revision (1). | nothing |
| `labels parse --file PATH\|- [--published]` | How the shared recognizer reads one body (at most 65536 bytes of UTF-8). | nothing |
| `labels edit stop:ID CHANGE --expected-revision N` | One explicit label edit of a local tour stop's comment, through the stop's owner. | `--profile`, `--draft` |
| `labels counts [--columns ID,ID\|none] [--mode open\|all]` | The Overview's label counts for the draft's pull request. | `--profile`, `--draft` |

CHANGE is `--label ID|plain`, or one of `--expectation
blocking|non-blocking|if-minor|none`, `--decoration TOKEN` or `--remove-decoration
TOKEN`, or a label with one of those three. `--label plain` removes the whole header
and takes no decoration. Human text is the default; `--json` writes the usual envelope.

**Catalog and parse** open no profile and create no file, so they also work beside a
running desktop; they refuse `--profile`. `parse` reports the status (`recognized`,
`plain`, `unknown_label`, `malformed` or `limited`), label, guidance, expectation and
header form; the raw label, header, subject and discussion as half-open UTF-8 byte
spans of the exact bytes given; each decoration as written with its kind; the
diagnostics; and the body's SHA-256. The text is never rewritten. `--published` reads
a published post as the counters do: a published tour stop only inside the narrative
its envelope maps, and an envelope that does not map gives no author text
(`classification_unavailable`), never a guessed label.

**Edit** plans `core::plan_feedback_tour_stop_edit` against the revision you read. Its
header decision is the one the desktop's label chip makes
(`decide_feedback_composition_edit`), and the result is the ordinary `edit_stop`
command `review_tour_host` executes, so the owner's revision check, undo journal and
request IDs apply unchanged. Only the header's bytes change: the subject, the
discussion, any suggested-change fence, line endings and UTF-8 are kept, and a comment
that is still only a header can be labelled before its subject is written. An edit
that changes nothing sends nothing. Refusals: `revision_conflict` (exit 4, the newer
text is kept), `read_only` (2), `invalid_label`, `invalid_decoration`, `invalid_edit`
and `invalid_input` (2), `input_too_large` (2), and `unsupported_header` or
`restructuring_required` (5): a malformed header, or a comment that begins with a
quote, list, heading, fence, HTML or table, stays exactly as written; change it with
`edit stop:ID:body`. After `outcome_unknown`, repeating the command with the same
`--request-id` and the revision you read returns the owner's record
(`operation_recorded`) when it committed and the stop already reads as that edit
leaves it, and sends nothing; the same ID with another edit is `command_refused`, and
an unrecorded ID with an old revision is an ordinary `revision_conflict`.

Review comment drafts have no headless owner. `labels edit comment:ID` (or `draft:ID`)
answers `capability_unavailable` (exit 5), and `capabilities` says so under
`labels.comment_drafts`; label them in the desktop's comment editor.

**Counts** read the draft's exact pull request once through the reader `preview
--refresh` uses, then count it as the desktop window does: `feedback_counter_model`
observes the accepted snapshot and `project_overview_feedback` answers the Overview's
request. The chips and the mode default to the profile's Settings (Reviews & queue ›
Comment labels: Overview chips and Overview counts); `--columns` and `--mode` choose
them for one command only. Reading Settings the first time creates the profile's
settings store with its defaults, as `hooks show` does. JSON `data.counts` gives
`unit` (`conversation`), `columns`, `columns_source` and `mode_source` (`settings` or
`option`), `mode`, `generation`, `epoch`, `collection`, `exact`, `limits`,
`tour_index_references`, the rows `pull_request` and `not_on_a_file`, the `folders`
and `files` that hold a conversation, and `quiet_files` and `quiet_folders` (`count`,
and whether their zeros are `exact`). Each row gives `kind`, `path`, `included`,
`scope_total`, `uncertain` (the ? count), `unattributed`, `unclassified`, `exact`,
`limits`, `states` (`unresolved`, `resolved`, `unknown`, `not_applicable`), `labels`
(each chip's `id`, `count` and `states`) and `other` (`count`, `states` and its parts
`unselected`, `plain`, `custom`, `unclassified`). The chips' counts and `other` add up
to `included`. A count is a conversation, counted once by its root's label; local tour
stops are drafts and are not counted. An incomplete answer is never a clean zero: a
partial read, unknown resolution or an unreadable label sets `exact` to false and names
the limit, with a `counts_not_exact` warning (on standard error in text). A read the
counter cannot admit is `counts_unavailable` (exit 5). While a desktop owns the
profile, counts answer `interaction_required` (exit 3) and read nothing; its Overview
shows the same numbers.

The additions are additive: the envelope stays `bpr.tour.cli-result.v1` and
`cli_contract` stays 1. `capabilities` lists `labels` and describes it in a `labels`
object.

## The pr-tour skill

`packaging/agent-skills/pr-tour/` is the ready-to-import Agent Skills package for the
conversational wrapper: `SKILL.md` plus three small references. It teaches only
`bpr tour` commands, keeps no draft, publisher or hook copy of its own, and starts no
model. The canonical home is the shared AgentSkills repository (`skills/pr-tour/`),
installed by MASK; BPR keeps no hand-maintained `.claude/skills` or `.codex/skills`
copy. The package here is the source of each import.
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

The package is in AgentSkills as `skills/pr-tour/` (commit `16992cd`, on that
repository's `main`), byte for byte the package here. This repository selects it
from `agentskills` in `.mask.json`, and `.mask.lock.json` pins the AgentSkills commit
MASK resolved. Its `.claude/skills/pr-tour` and `.agents/skills/pr-tour` are MASK
links into the MASK store and are ignored. `skill_pr_tour_package` fails on a real
directory there, on a link the selection does not name, on a selection from another
source, and on a deployed copy whose digest differs from the package's. Change the
package here, then import it unchanged into AgentSkills under that repository's own
process and run `mask upgrade pr-tour`; check the import with `python3
tests/scripts/pr_tour_skill_tests.py PACKAGE_DIR bpr-fixture-cli --digest` on both
copies. The owner authorized landing it on AgentSkills `main` on 2026-10-06
([[BPR-1067]]). The package needs only a released `bpr` CLI, not the desktop or this
source tree. A host loading the skill is an observation of that host, not something
the row shows.
