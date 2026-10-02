# Review-tour authoring and GitHub import

RFC-BPR-0039's manual-authoring milestone adds a purpose-specific core aggregate
in `review_tour.hpp`. A draft belongs to one application profile, accepted host
fingerprint, account, repository and pull request. Its local ID and revision
are distinct from GitHub identities. Problem and Solution are independent raw
Markdown fields. Vector position defines stop order; titles never identify stops.

Each destination is one inclusive interval in an exact comparison,
side, commit and repository path, with a blob identity when known. Both endpoints
must have the same comparison and side. Semantic `source_line_ref` endpoints stay
zero-based; the author sees one-based inclusive lines. Original source remains separate from a
current mapping. Representing historical or unchanged source grants no native
comment-write authority. Unknown source permits local saving but prevents manual
completion. Observing a changed head marks stops for attention without rewriting
the authored head or original source.

Commands bind owner, draft and expected revision. Reorder retains IDs; duplicate
and split allocate from a nondecreasing high-water mark. Split operates at a UTF-8
byte boundary and preserves related sources in both results. Merge concatenates
bodies and unions related source destinations. Restore supports local structural
undo while retaining the high-water mark, so an undone creation cannot recycle an
ID. Refused commands return the original draft intact. These operations emit no
publication, viewed-file, resolution, approval or correctness-receipt requests.

The core admits up to 256 stops, 64 KiB UTF-8 bytes per text field, 32 related
sources per stop, and a 4 MiB conservative encoded-size budget for authored prose,
retained baselines, identities, paths and metadata. A destination path is at most
4,096 bytes. Bounds refuse the whole command; they never truncate prose. Incomplete
local drafts, including an empty brief or skeleton stop, are valid saved state.

Persistence, workbench composition, public navigation and remote publication are
separate acceptance surfaces. The core tests alone do not establish any of them.

The admitted profile-private `state/v1/review-tours.sqlite3` keeps database schema 1 and writes version 7
payload records under application ID `0x42505254`. The exact owner and local draft ID
form the primary key. Payloads are bounded binary records with an integrity
digest; unknown schemas or corrupt payloads refuse rather than pretending that
the draft is absent. At most 128 drafts are retained with no automatic eviction.
The existing private-root admission, hardened connection, WAL and full synchronous
commit policy apply. Saves compare the expected revision inside one transaction.

The application host coalesces edits while a save runs off the decision lane.
A later acknowledgement never replaces newer editor text. Orderly shutdown
drains the last accepted edit. A failed save remains visibly unsaved and requires
an explicit retry; a conflicting save preserves both copies and never overwrites
the other writer automatically. Local structural undo keeps at most 16 snapshots
within 8 MiB per history direction; native text editors own text undo. Editing text
clears structural history so a later structural undo cannot erase a newer edit.
Qt routes standard Undo/Redo in the stop editor and outline to that history while
available, then to native text editing. Each structural history entry also
retains the current stable stop; a selection revision distinguishes a fresh
Undo/Redo selection intent from an ordinary save acknowledgement. Right-click
retains an existing selected group or selects the clicked stop. Ordered stable
ID sets extend the existing core move, duplicate, merge and remove commands;
validation refuses an invalid set atomically. Merge requires adjacent stops.
An internal drag from the dotted handle emits one revision-bound semantic move;
only the accepted projection reorders rows. Tab and Shift+Tab explicitly traverse
visible enabled controls, including assistance buttons, in all three prose fields.

Authored text and bounded raw remote baselines are explicit private retention
classes, outside the metadata ledger. Baselines retain raw body, source span,
remote identity/version and local generation; they confer no current remote
truth or mutation authority. Profile purge includes the exact tour store and
admitted sidecars. Local loss can lose unsent work; it never deletes shared
GitHub comments. Cold import uses the published objects described below; remote
update recovery belongs to the publication and maintenance milestones.

Prepare review tour is available from Overview and the Pull Request menu
(`Alt+T`). It opens the Tour page alongside the current Comments and Notes tray.
Problem and Solution use independent native plain-text documents; coaching never
becomes source. Their safe Markdown preview states its HTML/disclosure limits.
Section changes and preview perform no remote operation or agent launch.

The composition binds the store to the admitted application data root and exact
accepted owner. Identity invalidation clears the displayed copy. Pending or failed
private copies remain associated with that owner and can be resumed by reopening
the pull request. At most eight such suspended copies are retained per window;
further new tours are refused with a recovery explanation rather than evicting
unsaved prose. Clean saved copies are reconstructed from the private store.
Merely opening a PR does not consume a durable tour slot: an untouched empty
seed is observed and discarded without a save.

`Add tour stop` is in the source context menu and Pull Request menu
(`Alt+Shift+T`). The Qt surface supplies real source-line endpoints. Composition
checks the exact accepted endpoint provenance, owner fingerprint, commit, blob
and repository path before inserting a stop. Unified selections spanning both
sides are refused rather than converted from painted rows into a false range.
Related source capture uses the same admission and stores no credential-bearing
fetch destination.

The Tour stops page retains native explanation documents by stop ID, with a
wrapping outline and separate title, explanation and related-source controls.
The context menu offers insert after, duplicate, merge selected adjacent stops,
source replacement and remove. Add also inserts after the current stop and
selects the new one. Command/Control+D and Delete act only with the outline
focused, preserving native text editing in the fields. `Alt+Shift+Up/Down`
reorders while keeping the selected group; the existing `Alt+Up/Down` file
gestures remain free. The owner removed the separate Arrange and split controls
at attended hand-back 70; the underlying split algebra remains tested.
`Ready and next` (`Alt+Return`) marks only a complete, resolved stop Ready.
An oversized edit remains in its native document for shortening, copying or Undo.
Other fields and tour mutations wait until that edit fits, preventing a later
projection from silently replacing refused text.

Previous, Next and Return to tour preserve the independent conceptual route.
Normal file exploration leaves the selected stop and editor intact. Navigation
reuses the workbench file/diff path, then compares immutable source material
before restoring the range under the current runtime comparison identity.
Unavailable or changed material remains explicit; it never selects a nearby
line as a substitute. The ordinary opt-in “mark read when fully seen” trigger is
suppressed while Tour is shown. Neither readiness nor visiting a stop creates a
review receipt, resolution, approval, remote mutation or agent invocation.

The preview is private and lists the brief, ordered explanations, exact source
links and compatibility requirements. Remote publication has its own later
explicit Preview/Publish and recovery boundary.

## Raw Markdown and cold import

The existing paged provider reads raw comment bodies separately from display
prose, with returned IDs, web addresses, actors, review membership, versions and
available source coordinates. Numeric identities preserve the full uint64 range.
Pagination must finish before an accepted snapshot is complete. Missing raw
observations in an older recording leave a candidate read-only.

The core enumerates ordinary ordered lists in review summaries and PR comments.
An explicit description link prioritizes a candidate; multiple roots/lists require
selection. Index order, local stop IDs and displayed comment numbering remain
separate. Missing, repeated, malformed, foreign, self and mixed-author entries
remain visible; none grants mutation authority. Links are compared to exact
returned same-PR destinations and are never recursively fetched. Import refuses
to overwrite an existing private draft.

The bounded Markdown scanner excludes fences, quotes, nested lists and HTML
regions from top-level mapping. It preserves ambiguous sections/lists for manual
selection. Every editable section retains the complete raw baseline and a byte
span. Native selection translates UTF-16 positions and CRLF normalization back
to exact UTF-8 boundaries; a changed display or split surrogate is refused.
Disjoint edits validate expected span bytes and preserve all untouched material.
A custom stop body is retained whole unless its generated heading and final
navigation paragraph are confidently recognized.

Payload v2 added remote attribution, index baselines and read-only state. Payload
v3 adds a comparison-independent source destination. Versions 1 and 2 remain
readable without rewriting their content; the next save uses the current payload version. The database
schema and private-root contract are unchanged.

## Exact source and compatibility

An imported destination retains commit, optional blob, path, side and inclusive
zero-based range. It has no invented comparison digest. Native right-side
comments prefer the original commit/range. Missing original left-side source
facts remain unavailable. File/PR comment fallbacks use the provider's canonical
commit-pinned first-line permalink. The explicit tour codec extends the existing
single-line codec with ranges and historical commits, sharing canonical path
escaping and Markdown `?plain=1` handling. Parsing establishes an address;
workbench navigation checks the actual admitted commit, blob, path and side before
constructing current comparison line references. Unavailable source retains its
explanation and visible source link.

| Intended subject | Required exact operation | Publication boundary |
|---|---|---|
| Current diff line/range | Pending line batch | One pending review only when qualified |
| Current off-hunk range | Pending off-hunk batch | Separately qualified from standalone line creation |
| Changed-file permalink fallback | Pending file-thread operation | Joins the pending review only with exact support |
| Standalone file fallback | Standalone file-comment operation | Explicit compatibility plan |
| Historical/base/unchanged source | Tour PR-comment operation | Explicit compatibility plan |

Unknown, denied, throttled and indeterminate outcomes do not authorize another
representation. Exact unsupported evidence and exact fallback support are
separate facts. Suggestion bodies cannot silently become file/PR permalink prose.
The capability key separates operations and exact source digests in addition to
host, actor, repository, API/classifier revision and freshness. Existing
standalone evidence never qualifies pending file staging. The preparation host
has no live-qualified pending-operation observation and displays that limit;
loading source or importing an own comment does not create write authority.

## Staged publication wire contract

BPR-0999 extends the existing review-submission/mutation family with finite
pending-review operations. A batch sends the exact head and line/range comments
without an event. The recorded review ID receives COMMENT only through its
separate submission operation. Body updates use explicit PATCH/PUT; owned
pending deletion uses DELETE. Native HTTPS witnesses each mutation before
creating its Qt request. Ordinary summary and source-line publication retain
their contracts.

Raw responses retain complete numerical IDs, returned same-PR web URLs, actors,
review membership and inclusive source coordinates. A page continuation must
name the exact collection and next bounded page; incomplete membership cannot
establish ownership. The wire ceiling remains 64 KiB including JSON overhead,
100 comments per batch, 256 KiB per response and ten pages per collection.
These are fixture-tested contracts. Pending comment editability and website
visibility are not yet qualified on a live host, and pending file-thread staging
remains unavailable. No compiler, parser or read observation grants write
permission; the staged coordinator owns the journal and author command boundary.

Staged publication extends the existing review-submission family. A frozen plan
contains ordered sources/prose, exact live binding, owner/generation and any
separate description edit. The host consumes one bounded parent grant; each
transmission consumes an exact child journal revision. SQLite recovery v5 stores
one finite pending step and confirmed returned identities in the existing
private recovery database. Unknown writes are reconciled by complete reads and
are never blindly repeated. Stop/restart retains receipts and requires explicit
Resume for further writes. Live pending mutation endpoints stay unavailable
until their separate qualification is recorded.

## Published maintenance

An explicit update preview freezes the observed owned root, original stop bodies
and desired route in the existing staged-publication journal. New destinations
use a later pending batch; confirmed IDs survive Stop and restart. Body-only
edits preserve original threads and replies. Derived navigation changes only
where its bytes change, and the original root index is updated last. Every stop
retains an overview link through a partial update. Retirement removes membership,
not the shared discussion. Explicit source replacement creates a linked new
comment and preserves the old source binding. Published split/merge operations
retain the original discussions and prepare local replacements.

The Tour payload reads versions 1–7 and writes version 7, retaining bounded retired
stop baselines, exact baseline/local/remote conflict versions, the observed head,
per-stop reaffirmation and locally observed website changes. The existing recovery database retains its schema; staged payload
version 3 additionally stores the frozen maintenance draft and confirmed edit
prefix, while still reading versions 1–2. Existing profile purge and private-store
retention cover these bounded prose records; no shared object is deleted by purge.

Before each write the session reads exact actor/head, current owned bodies and
relevant review memberships. Website replies are preserved. A changed baseline
stops the plan and retains its desired text. These endpoints provide no evidenced
conditional body write: preflight and reconciliation cannot eliminate a concurrent
website edit between them. No operation is qualified merely by these fixture
results; production Tour writes remain unavailable until the exact operation's
live qualification.

Refreshing a bound tour compares its previous raw bodies, current draft and
complete website observation. Equal versions adopt website truth without writes;
two disjoint contiguous byte edits can merge. Overlap retains all three versions
in the profile store and exposes explicit local/website choices. Source spans are
mapped again from the fresh raw body. Unavailable or ambiguous observations keep
local work visibly stale, and unresolved conflicts withhold publication. Store
revision checks continue to serialize local windows independently of website
conflicts. Description changes patch the mapped brief sections and preserve
unrelated template bytes. For a first publication with neither section present,
the explicit preview inserts the brief at the beginning while retaining every
original byte; partial/duplicate sections require mapping. imported custom stop layouts retain a whole-comment
editor. Reimport from a clean profile retains only shared identities and source
addresses, without private preparation provenance or inferred viewing history.


## Revision attention and observed changes

An admitted selected epoch comparison supplies the existing deterministic line and
move correspondence. Proposed later ranges retain their exact comparison, commit,
blob and renamed path separately from the original public source. They never
create replacement subjects without the author's explicit retarget. Missing or
ambiguous correspondence stays explicit. Observing another head clears previous
reaffirmations for every stop, because identical selected bytes cannot prove an
explanation about a changed caller or contract. The author can deliberately
reaffirm an explanation while retaining its original source, including a bound
imported address whose old blob is unavailable locally.

The locally observed website-change list compares retained raw baseline versions
and ordered remote URLs. It distinguishes added, removed, reordered, changed-text
and changed-source stops without counting unpublished local edits as shared
history. A repeated equal observation retains the last differing observation;
successful publication clears that difference list. A clean profile states
“No prior local tour version.” Changed-stop navigation uses the existing outline
and does not create viewed-file receipts, approval, acceptance or resolution.

The complete fixture manual lifecycle and the separately pending operator course
are recorded in the [tour verification ledger](../verification/RFC-BPR-0039-0043.md)
and [website/native course](../verification/RFC-BPR-0039-0043.manual-course.md).


## Optional outline and author completion

The Tour stops page offers **Suggest tour outline** through the same selected
profile, versioned outline hook and restricted writing process as per-field
assistance. It sends at most eight bounded sources, including the current exact
selected source when available. Returned references resolve only against those
supplied identities. A reference to prose has no invented code destination; its
skeleton remains unresolved until the author selects a real source. Unknown or
duplicate references, extra bodies, Ready flags and tool directives are refused.

The author chooses retained suggestions, then **Insert selected skeleton stops**
adds them as one local structural undo transaction. Every inserted explanation is
empty and Suggested/unvisited. Private reasons and author questions are shown
beside its editor and retained in payload version 7; they never enter published
Markdown. Older payloads remain readable without inventing provenance.

A visible visit shows the selected editor with its exact code or an explicit
source-unavailable explanation. It never acknowledges later generated prose.
**Generate comment** invokes the existing in-place editor only for that visible
stop; native Undo/Redo remains unchanged. **Ready and next** supplies a fresh
observation of that editor and the exact displayed source to the core. Hidden
or background editors, other source ranges, and unavailable source cannot complete
a generated stop. The next stop is merely selected; it triggers no inference.

Initial publication and published updates refuse any retained generated stop
without current author completion. Text, title, mapping or website narrative
changes clear readiness; derived numbering and navigation do not. Duplicating,
splitting or merging a generated stop preserves its completion requirement.
Removing a stop removes that outstanding obligation. Cold import cannot reconstruct
private generated provenance or author visits that were never published.


## Shared local history and resumption

The CLI and desktop use database schema 3 and the same atomic tour-state
record. Its version-two envelope adds the originating presentation to each
reversible transaction and an optional section/stable-stop resume hint. Legacy
draft payloads and version-one journal envelopes remain readable; opening them
does not invent prior history or provenance. The resume hint is private workflow
position, never a mutation target or proof of an author visit.

Each direction retains at most 16 transactions and 8 MiB. Eviction affects only
old undo/redo entries. Current text, unresolved conflict candidates, source and
readiness bindings remain in the current draft; remote publication records stay
in their separately admitted journal. Undo restores exact stored content with a
new revision, and redo needs no AI or network. No-op edits add no transaction.
Both history and current content commit under the existing exact revision CAS,
so a failed or killed commit cannot retain only one half of an edit.


## Owner forwarding

While the desktop owns a profile, `bpr tour` forwards each local draft command to
it instead of opening the store. The CLI acquires the profile lease first; only
when that lease is busy does it try the owner's tour endpoint, which is a
separate rendezvous beside the activation endpoint. Activation frames keep their
fixed meaning and an owner without a tour endpoint answers
`owner_protocol_unsupported`. The CLI never opens a second writer, steals a lease
or starts the desktop.

The endpoint is a per-user local socket bound only while the owner holds the
exact-root lease. Each connection is authenticated as the same OS user and
begins with an owner-advertised 64-byte binding: protocol version, profile, root
digest and a per-owner-instance nonce. The caller sends nothing until the binding
matches what it resolved for that profile and root, and the owner accepts a frame
only if it echoes that binding. Frames declare a length of at most 5 MiB and are
decoded by a closed codec: `load`, `mutate` and `request_status` for one opaque
draft selector, with local editing commands only. Observation, restore, import,
publication and provider operations cannot be encoded. A path, a PID or a JSON
actor string is never authority.

The owner serves one request at a time on its effect lane from its own store, as
one bounded transaction. Every mutation carries a request ID and a payload digest
the receiver computes itself. The outcome is committed in the same compare-and-set
save as the edit, so the same ID with the same digest returns the stored outcome
and the same ID with another payload is refused. A caller that lost an answer
learns of it as `outcome_unknown` and is told to inspect the exact ID
(`status --request-id`) before any retry; nothing is replayed on its behalf.
The journal keeps at most 4096 outcomes per draft and refuses further requests
rather than forget one, and it records only the local outcome, never that GitHub
accepted anything.

On Windows the same framing runs over an owner-private named pipe beside the
activation pipe, in byte mode, rejecting remote clients; the caller checks the
owner's user and the pipe's descriptor before sending ([[BPR-1168]]). Protocol 3
added the `cycle` operation: one finite `bpr cycle` command and that command's
own options, run by the owner on its cycle store (see
[the cycle terminal](../development/cycle-cli.md)). An owner or caller of another
version is told so before a payload is sent.

### A draft the desktop has open

A draft that a desktop window holds, current or retired, is read and written
through that window's host, on the decision lane, so there is one committed
history and never a second copy read from the store beside it. The endpoint is
polled on the effect lane; each request is routed to the window session that holds
the draft's opaque selector. The desktop lists its own draft in the profile's
target catalog when it composes the pull request, so the CLI and the owner name
the same draft by the same selector.

Ordinary typing is committed to the host on every change, so there is no pending
buffer to flush. Uncommitted input-method text is the exception: while any field of
the shown draft holds preedit text the owner answers `editor_busy` and changes
nothing, and the CLI reports that nothing was changed. A forwarded mutation is
answered only after the host has saved it durably; if the save has not landed
within five seconds the caller is told `outcome_unknown` and inspects the request
ID, exactly as for a lost answer. A draft nobody has touched is never persisted
and counts as saved.

Another presentation's edit reaches an open editor as a projection. The author's
caret, selection and scroll carry across the change, focus is not taken or given
up, the selected stop stays selected unless the edit removed it, and no selection
change is announced. A stale command, whether from the CLI or an old AI reply,
is refused as a conflict; both texts remain. Structure follows stable stop IDs, and
undo from any presentation restores the same order and IDs through the one shared
history.

Commands that read the provider (`add`, `open`, `show --source`, `walk`, refresh,
publish and recover) are not forwarded and answer `interaction_required` while a
desktop owns the profile. BPR-1227 tracks forwarding the source reads.

## Assistance tickets

The active agent of RFC-BPR-0046 asks for writing help through two short-lived
requests rather than a nested model. An assist ticket is a bounded record in the
draft's shared journal, committed under the same compare-and-set as the draft. It
fixes the hook, the field, the exact byte interval, the draft revision, the field's
digest, the hook revision and a snapshot of the sources, and it expires after
thirty minutes. The draft holds one live ticket; a new one supersedes the old.
Answering names the ticket and supplies only a result. The result is decoded by the
parent RFC-BPR-0043 contract, the ticket is spent, and only a result whose revision,
field bytes and hook revision still match is spliced in through the shared text
transaction as one undoable edit. Outline answers become body-free proposals kept
in the same journal, and a chosen subset is inserted as Suggested stops through the
existing structural command. Read paths issue nothing, and nothing here reads a
provider or starts a process.
