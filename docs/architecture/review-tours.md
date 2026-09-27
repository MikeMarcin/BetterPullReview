# Private review-tour authoring

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

The admitted profile-private `state/v1/review-tours.sqlite3` stores version 1
records under application ID `0x42505254`. The exact owner and local draft ID
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
GitHub comments. Cold import and remote update recovery remain later milestones.
