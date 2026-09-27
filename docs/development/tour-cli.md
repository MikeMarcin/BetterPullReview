# Tour terminal development

The `tour-cli` preset builds the separate `bpr` executable with tests disabled:

```sh
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix /absolute/install/root
/absolute/install/root/bin/bpr tour capabilities
/absolute/install/root/bin/bpr tour capabilities --json
```

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
Concurrent profile ownership returns `owner_busy` (exit 4). `status` pages stop
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
