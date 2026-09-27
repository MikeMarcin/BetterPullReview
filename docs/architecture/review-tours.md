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

The admitted profile-private `state/v1/review-tours.sqlite3` keeps database schema 1 and writes version 3
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
Arrange offers insert, duplicate, split at the text cursor, merge with next,
remove, move and bounded structural undo/redo. `Alt+Shift+Up/Down` reorders while
keeping the stop selected; the existing `Alt+Up/Down` file gestures remain free.
`Save and next` (`Alt+Return`) marks only a complete, resolved stop Ready.
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
readable without rewriting their content; the next save uses v3. The database
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
