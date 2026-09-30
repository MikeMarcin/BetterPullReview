# BetterPullReview Domain and State Model

## Architectural premise

The functional core owns compact interaction state and deterministic policy. The application host owns authoritative external/local sessions, persistence, caches, and effects. The Qt presentation adapter owns visual and Qt-local state. Large GitHub, Git, diff, and ledger data is observed through stable identities and bounded views rather than copied into one global state tree.

## Strong identities

```text
HostProfileId
HostProfileRevision
GitHostId
GitHubAccountId
RepositoryId
PullRequestId
ProviderSnapshotId
CapabilityRevision
AuthorityScopeId
PullRequestSourceRevision
BaseBranchId
CommitOid
TreeOid
BlobOid
EpochCatalogIncarnationId
EpochOrdinal
EpochCatalogRevision
ReviewEpochId
ReviewAssociationId
ComparisonAnchor
ComparisonRevision
EpochGeneration
ComparisonGeneration
FileMaterialId
GitHubReviewId
GitHubThreadId
GitHubCommentId
LocalDefectId
AiSessionId
RequestId
RequestGeneration
ApplicationSessionId
```

Every persisted row, observation, request, completion, cache key, and asynchronous result includes enough identity to prevent cross-host, cross-account, cross-repository, cross-PR, or cross-revision reuse.

RFC-BPR-0003 uses fixed, algorithm-tagged Git object identities. A commit,
tree, or blob OID carries `sha1` with exactly 20 bytes or `sha256` with
exactly 32 bytes. The object kind is part of the C++ type, the algorithm is
part of the value, an all-zero value is invalid, and a repository cannot mix
formats. Hexadecimal text is parsed once at the provider or Git boundary and
is rendered as canonical lowercase text only at an outward projection.

The relevant bounds are normative:

| Type | Valid range or size | Zero meaning |
|---|---|---|
| Opaque runtime identities | nonzero `uint64_t` | invalid |
| Pull-request number | `1..2,147,483,647` | invalid |
| Request and generation counters | `1..UINT64_MAX`, advanced only by checked increment | inactive or not yet issued |
| Epoch ordinal | `1..INT64_MAX` | invalid |
| Catalog revision | `0..INT64_MAX` | accepted empty catalog |
| Comparison revision | `1..INT64_MAX` | no accepted pair |
| Review order | `1..10,000` within one complete accepted snapshot | invalid |
| Authority scope, catalog incarnation, base branch, and review association | exactly 32 nonzero bytes | invalid |
| Host-supplied observed UTC milliseconds | `0..253402300799999` | the Unix epoch, not absence |

No counter wraps, resets inside its authority, or reuses an identity after
exhaustion. An explicitly deleted and recreated epoch catalog receives a new
incarnation before its ordinal can begin again.
The host supplies the nonreversible authority-scope identity and catalog
incarnation from validated authority/persistence state; the core neither
hashes provider text nor reads randomness.

### Review cycle identities (RFC-BPR-0053, in progress)

`bpr/core/review_cycle.hpp` adds a second review subject beside the pull
request: `review_subject` is either the unchanged `pull_request_authority` or a
`local_cycle_subject`. A cycle subject names a BPR profile, a 32-byte cycle
catalog incarnation, a cycle identity and a generation; it carries no host,
account, repository or pull-request field, and none of those is made optional
to accommodate it. Source revisions, feedback items, round drafts, rounds,
responses, verifications and publications each have their own identity type,
allocated from per-cycle counters in `1..INT64_MAX` that only advance, and each
reference names the cycle that allocated it. A revision's identity orders
observations; its material digest says which bytes it holds, so two revisions
can share bytes and stay distinct. A cycle with no capture has no initial
revision, which is different from an empty complete capture. Scope changes are
versioned, and a path missing from a partial capture or outside the scope is
unknown, never deleted. The cycle catalog and its storage are the host's.

`application::review_cycle_store` is the storage port, and the SQLite adapter
keeps it in its own profile-private `state/v1/review-cycles.sqlite3`; no pull
request table changes. The catalog row carries the profile and a random
32-byte incarnation. Objects (capture manifests, the content they reference, frozen round payloads) are
content-addressed by SHA-256 and installed and verified in their own
transaction before any catalog row names them, so a crash leaves a complete
record or none plus at most an unreferenced object. Every write carries a
caller operation identity: an identical retry returns the stored receipt, and
reuse for a different request is refused. Objects that a revision or round
references are pinned; `reclaim` removes only unreferenced ones, and an install
that would exceed the retained budget is refused with the inventory rather
than evicting pinned evidence. Removing a cycle requires a preview whose token
still matches, and a removed cycle's identity is never reissued. Profile purge
owns the file.

`application::review_cycle_runtime` composes that store with a decision lane
and an effect lane and nothing else: no account enrollment, provider, vault,
agent adapter or executable lookup. Store calls run one at a time in request
order. Selecting another review subject cancels work issued for the previous
selection, and a pull-request selection is refused rather than answered from
the cycle store. `application::review_material_bridge` routes a tagged
material request to its own authority: pull-request blobs go unchanged to the
existing `diff_blob_port`, cycle objects come only from this profile's catalog,
only for a revision of the named cycle and only the object that revision
recorded; neither side falls back to the other. The runtime's link closure is
checked by `review_cycle_dependency_closure`.

A cycle binds to one deliberately chosen root (`bpr/core/review_cycle_workspace.hpp`,
RFC-BPR-0054): a plain folder, a single file, a Git checkout or a linked
worktree. The binding holds a logical project identity derived from the cycle,
never from a path, and the root observation the host made: the location as the
user chose it, the root object's native identity (device and inode, or volume
serial and file ID) and, for Git roots, the identities of the shared and
per-worktree Git directories. A single-file root is identified by the directory
holding it, so an editor that saves by replacing the file does not replace the
root. Identity decides whether a root is still the bound one; the location
only says where to look. Every rebind, including a moved root, is an explicit
confirmed command that advances the binding version, and a rebind over captured
material records the material check the reviewer confirmed. Each capture names
the binding version it was read under and is refused when that version is not
current, so bytes from a replaced or same-named checkout never land in the
cycle unannounced. An imported project resolves to a local one only through an
explicit mapping. The core also admits capture paths (root-relative raw bytes,
no `.`, `..`, empty or `.git` segment, nothing normalized), labels non-UTF-8
bytes with `\xNN` escapes, flags names Windows cannot hold, and finds pairs of
paths a case- or normalization-insensitive volume would merge; nothing is ever
combined. The SQLite store keeps the binding in its own table (schema 2, which
migrates the first schema in place).

`application::workspace_source_port` is the read-only host port and
`bpr/adapters/workspace` its native implementation. It opens the chosen
location (which may be reached through links the user chose) and resolves
everything beneath it without following links: `openat` with `O_NOFOLLOW` per
component on POSIX, `FILE_FLAG_OPEN_REPARSE_POINT` per component on Windows
with each ancestor held without delete sharing; a component on another volume
is refused. It never lists `.git`, reads Git metadata only as named files, and
never writes, runs Git, hooks or filters, or reaches the network.

`application::review_cycle_capture_host` turns a bound root into a revision
(BPR-1149). It reads the scope through the workspace port, hashes each file's
raw bytes in chunks and installs them in the cycle store, then observes the
whole scope again: same paths, kinds, identities, sizes and change stamps, the
same index bytes, the same `HEAD` and the same root. Only when both passes
agree does it install a manifest and record the revision; the store pins the
manifest and every content object it references in the same transaction. A
disagreement discards the attempt, and after three retries the capture reports
`unstable` with the paths that moved; cancellation records nothing and leaves
only reclaimable objects. The result is labelled `observed_stable`, a quiescent
handoff observation; `immutable_source` is reserved for a Git tree or a
qualified snapshot and no capture path produces it. For Git roots the tracked
set, staged modes and object identities, conflict stages, sparse entries and
intent-to-add come from parsing the index (versions 2-4) as data; untracked
files are included unless the checkout's own `.gitignore` files or
`info/exclude` exclude them (the reviewer's global excludes file is not read,
so what only it ignores stays visible); tracked paths absent from the working
tree are explicit deletions, sparse entries are visible but unread, and nested
repositories and submodules are boundaries. Folder roots use a disclosed
default set (version-control and tool state, regenerable caches, common
credential stores). `bpr/core/review_cycle_manifest.hpp` holds the canonical
manifest: sorted entries with kind, origin, availability, portable mode, raw
SHA-256, byte profile and staged object; exclusions with their reason;
deletions; scope and scope version; coverage and quality. Paths outside a
capture's scope are `outside_scope` and paths a partial capture did not list
are `unknown`; neither is ever a deletion.

`bpr/core/review_cycle_material.hpp` classifies bytes without changing them
(BPR-1150): UTF-8, UTF-8 with a byte-order mark, unsupported encoding (UTF-16
or UTF-32 marks, malformed UTF-8), binary, and empty; LF, CRLF, CR or mixed
line endings; a missing final newline; and the Git LFS pointer shape. Text
ranges are byte offsets checked at scalar boundaries, with a separate mapping
to presentation columns. A file's mode is its execute bit where the platform
has one; on Windows a tracked file keeps its staged mode and a new file is
never given an execute bit. Links are captured as their target bytes. A
`filter` attribute (LFS included) is recorded rather than run: the Git object
is a transformation of the captured bytes, so a later native publication of
that path needs a qualified transformation path. Devices, sockets and pipes
are visible as unsupported; files over the capture budget are visible as too
large.

Cycle views read only retained material (`bpr/core/review_cycle_comparison.hpp`,
`application::review_cycle_comparison_host`, BPR-1151). A view names its
material exactly: full source is one revision read on its own, never a diff
against nothing; an explicit revision pair (or a round with a named result) is
pinned and never retargets; all-cycle, since-checkpoint and this-round views
resolve to a pair when opened and move only on explicit refresh, with a
newer-revision signal in between. Pages list at most 256 paths in path-byte
order over everything either manifest names, classifying each side by scope,
coverage, exclusion ancestry and deletion, so a widened scope, an exclusion or
a partial capture reads as unknown rather than added or deleted, and
content-equivalent paths share one content identity. A pair's file diff goes
through the shared engine: `compute_material_diff` applies exactly the
decoding, alignment, pairing, move and limit rules of `compute_file_diff`
under a `material_comparison_key` whose identity is derived from the cycle,
pair, path and contents (`BPR-cycle-file-comparison-v1`), with no pull-request
authority or epoch. Every byte comes through the material bridge, which serves
only an object the named revision pinned; a missing or corrupt object is
reported and nothing is read from the workspace, so later edits, deletions or
a restart change nothing a view shows. A side that is unavailable or unknown
yields no diff. The core's file-view state keeps the shown content while a
newer request loads and drops a completion for a superseded request; the
runtime completes work issued for a previous selection as cancelled.

Capture recovery (BPR-1152): content, the manifest and the revision record are
three separate store transactions, and a crash at any of them leaves the cycle
exactly as it was or holding the whole revision; objects installed before the
record are unreferenced candidates that `reclaim` removes, and retrying the
same operation completes once or replays. Installing bytes whose digest is
already stored verifies the stored copy and, when it is damaged, replaces it
with the verified bytes in hand, so a recapture of the same content repairs a
corrupted pin; damage nobody can re-verify stays reported. A full disk or an
exhausted budget refuses the capture as a store failure (quota refusals carry
the inventory) and never evicts pinned material. `explain_capture` turns a
revision's observation and attempt count into a quiet quality indicator
(quality, completeness, unavailable entries, retries); an ordinary stable
capture needs no warning, and only an immutable source may claim one instant.

Private cycle feedback (`bpr/core/review_cycle_feedback.hpp`, RFC-BPR-0055,
BPR-1153) is a versioned private thread about one primary target, with up to 32
related references appended over time. A target is the whole cycle, a module
path, a file of a captured revision (by its content digest), or an anchor: a
half-open byte range of one file of one revision holding its exact bytes and
up to two lines (at most 512 bytes) of context on each side, with a symbol
kept only as a hint. Line numbers are a presentation of those offsets (LF,
CRLF and a lone CR each end a line); module and cycle notes have no line at
all. The original attachment never changes. Where it stands in other material
is a separate observation, `exact_material`, `exact_mapped`, `ambiguous`,
`deleted`, `unavailable` or `unmapped`, derived from that revision's retained
bytes: repeated text is mapped only when the complete original context singles
out one occurrence, and re-attaching a note to newer source appends a
reference that names what it rebinds. `application::verify_feedback_targets`
checks every file and anchor target against the exact bytes its revision
pinned before the store records it, and the store also refuses a digest the
revision did not pin, so a note cannot name bytes that only the live
workspace holds. The shared slice of RFC-BPR-0023's private threads,
`bpr/core/private_thread_lineage.hpp`, is what both thread kinds use: thread,
turn, revision and order identities, the header (version, last order,
lifecycle) and its write, advance and lifecycle rules, completion,
edit/adoption derivation, body digests and bounds, and the four provenance
classes (`human`, `adapter_observed`, `self_reported`, `unknown`).
Pull-request threads keep their own scope, binding, actor and store; nothing
of RFC-BPR-0023's pull-request promotion or composition is involved. Cycle
revisions are numbered per thread, and the SQLite store keeps threads and
revisions in their own tables (schema 3, migrating schema 1 or 2 in place),
pages them at 64 threads, or 64 revisions and 512 KiB of bodies, and removes
them with the cycle.

A note may carry up to 64 exact suggestions (`bpr/core/review_cycle_suggestion.hpp`,
BPR-1155), numbered per thread and kept as immutable revisions: the source
revision, path and content digest of the file, a half-open byte range with
its exact original bytes, the exact replacement bytes (an empty range is an
insertion and an empty replacement a deletion), an intent (`exact_replacement`
or `illustrative`) and a state (`proposed`, or `observed_already_applied` with
the revision that showed it). Previews and applications are pure byte
functions held in memory; nothing writes a workspace, and suggested code is
never compiled or run. `preflight_suggestions` decides a whole set against one
file's exact bytes: every edit must name that file's whole content digest
(matching text in a changed file is `stale_blob`), its range must hold its
original bytes, illustrative and already-applied edits are never applied, and
overlapping or contained ranges, two insertions at one boundary, or an
insertion on a replaced range's boundary conflict. Only a set with no finding
is applied, in descending offset order over original coordinates. A changed
file is followed only by revalidating onto a range that holds exactly the
original bytes; an application is recorded only when a later revision's file
is exactly the suggested result (similar text stays unknown). Both are
checked against retained bytes by `application::review_cycle_feedback`'s
helpers before the store records them. `encode_suggestion_wire` carries both
sides as base64 with their lengths and SHA-256 and refuses any mismatch on
the way back.

A note's body is the author's raw Markdown and stays authoritative
(BPR-1154). Conventional Comments labels are text inside it: cycles store,
render and disclose every label, Plain and malformed prefixes byte for byte,
and no label implies a suggestion, a severity, a permission, a lifecycle or a
verification. Recognizing labels is the shared RFC-BPR-0047 recognizer's
(BPR-1076, not yet built under the owner's start hold for RFC-BPR-0044–0051);
when it lands it reads the same bytes, and cycles add no label field or
cycle-only label set. `bpr/core/review_cycle_disclosure.hpp` decides what one
note may carry out of the cycle: by default only its current wording (the
latest revision of its first turn); replies, pasted or observed answers,
superseded wordings, private references and adapter, provider, model and
effort details stay private until named, and derivation and reply links
never travel. A pasted answer that names a model stays `self_reported`.

The desktop surface (BPR-1156, BPR-1157) composes the same runtime with the
workspace source in `application::review_cycle_workbench`: File > Open
Project… and Home's Review Cycles section observe the chosen root, resume the
active cycle already bound to that root object or create one titled after it,
bind it and capture R0, then show full source. Its tabs are the RFC-BPR-0052
tab gestures over `core::cycle_tabs_state` (`review_cycle_tabs.hpp`): each tab
is one exact revision, path and reading line, keeps its revision when newer
material is captured, refuses the pull-request comparison gestures, and is
kept in the cycle store's `views` table (schema 4) so a restart restores it.
The Qt page (`bpr/qt/review_cycle_view.hpp`) takes the review surface's place
in the window with its own review bar, folding Updates line, tabs, files
drawer, one source pane and notes tray; the title row names the cycle with a
Local cycle badge, and another pull request arriving takes the surface back.
A native selection becomes an exact byte anchor of the retained bytes. The
focused suggestion editor (`suggestion_text_editing.hpp` with
`bpr/qt/suggestion_editor.hpp`) keeps exact lines with their own endings and
offers multiple carets, box selection, virtual space, IME composition and
ordinary undo; its composer previews the edit in context and never writes a
file.

A round (RFC-BPR-0056, BPR-1158) is one assignment frozen from a round draft
(`bpr/core/review_cycle_round.hpp`). The draft stores the reviewer's selection
as canonical JSON (`bpr.cycle.round-draft.v1`): the revision under review, an
action (`discuss`, `plan` or `implement`), goal, constraints, requested write
scope, the chosen notes in the reviewer's order (each pinned to the exact
wording and suggestion revisions selected, with an optional per-item action
and related notes) and explicit source context. `assemble_round` checks every
pin against the store's current records and refuses a stale wording or
suggestion, an archived, unknown or repeated note, a relation outside the
round, context its revision does not hold, and anything over the round
budgets (4,096 items, 16 MiB of instructions, 8 MiB of context unless 64 MiB
is explicitly admitted), naming the item or context and the measure; it never
substitutes, summarizes or drops. The payload (`bpr.cycle.round.v1`) carries
only each note's current wording, its primary and included related locations,
the selected suggestions' exact bytes and the selected source (one entry per
file and revision), under wire names (`RC-n`, `Fn`, `Rn`, `In`, `SGn.m`,
`Sn`) and a logical project derived from identities, never a path. Its digest
is the SHA-256 of its `bpr.cycle.canonical-json.v1` bytes
(`bpr/core/review_cycle_json.hpp`: sorted keys, no whitespace, no floats, no
normalization); the reader refuses duplicate keys, lone surrogates, invalid
UTF-8, fractions and nesting beyond 32. A freeze
(`application::freeze_cycle_round`) needs a flushed editor showing the stored
draft revision, installs the payload and commits the round naming it
(`freeze_named_round`); an interrupted freeze leaves at most an unreferenced
object, the same draft revision reuses its round, and an edit is a new draft
revision and a new round.

The readable form of a round (`bpr/core/review_cycle_round_markdown.hpp`,
BPR-1159) is one Markdown document rendered from the canonical payload alone:
the assignment, every item in order with its raw body, locations, relations
and suggestions' exact bytes, then the selected source. It reproduces the
pack's example `review.md` byte for byte. Fences outgrow every backtick run
they hold; a body whose own Markdown would change the document's structure
(an unclosed fence, a level-one or level-two heading, an HTML comment or raw
block) is shown verbatim in a fence; bytes a fence cannot carry exactly (CR,
NUL and other controls, invalid UTF-8, trailing spaces, no final line break)
are labelled and given as base64. Copy to Clipboard and Save to File emit
these same bytes; a document over its channel's budget is refused with its
size, never shortened. `application::save_round_document` writes one file the
reviewer chose through `cycle_export_destination`
(`bpr/adapters/workspace/local_export_destination.hpp`, a separate
`AdapterExport` library beside the read-only workspace source): a flushed
temporary file moved into place without replacing anything, an identical
file left untouched, and other bytes replaced only when the reviewer confirmed
exactly those bytes and they are still there.

A portable bundle (`bpr/core/review_cycle_bundle.hpp`, BPR-1160) is a
`.bpr-review.zip` of `manifest.json`, `round.json` (the envelope),
`review.md` (the document above) and `objects/<sha256>` attachments holding
each selected source's exact bytes. `build_round_bundle` is deterministic for a
round (stored members, fixed timestamps, UTF-8 names), and the pack's own
reference oracle accepts what it writes; `open_round_bundle` reads stored and
deflated archives from any producer (`bpr/core/review_cycle_zip.hpp`, a
bounded inflater with CRC checks) and validates the container, names,
manifest, round, derived document and attachments before returning anything.
`application::import_round_bundle` admits a validated bundle into the cycle
store's quarantine: the canonical payload and attachments are installed as
objects, and schema 5's `imports` and `import_objects` tables pin them under a
record keyed by the round digest that keeps the sender's cycle, round and
project names exactly as sent. The same round imported again, or after a
restart, finds that record; a refused bundle installs nothing; an interrupted
import leaves only unreferenced objects. Schema 5 migrates schemas 1 to 4 in
place.

A quarantined round is associated with a local cycle only explicitly
(`associate_import` in `bpr/core/review_cycle_import.hpp`, BPR-1162): the
sender's project name must map to the cycle's project through a
`workspace_import_mapping` keyed by `imported_project_key` (never a label,
path or folder name), every sender revision the round names must map to a
captured local revision, and every file the round names by bytes must be
exactly those bytes in its mapped revision as retained. The association is
kept beside the immutable record; repeating it changes nothing, another one is
refused, and removing the cycle releases it. Forking an associated round
(`plan_import_fork`, `application::fork_imported_round`) starts one local note
per item at its mapped locations, with the exact body and `unknown`
provenance (`begin_cycle_feedback::Provenance`), recorded in `import_forks`
against the sender's item name; the reviewer's edits are ordinary successor
revisions and the sender's round never changes. Nothing is applied to any
source. Prose replies need no protocol: a pasted answer is a turn of
`unknown` or `self_reported` provenance, and the next capture needs no
structured response.

Iteration is manual (RFC-BPR-0057, BPR-1163). Capture revision
(`review_cycle_workbench::capture_revision`) is the only way back after
someone else worked: no receipt, response, agent, commit or callback is
waited for, and nothing is captured by itself. Identical bytes record nothing
and say so; a deliberate capture records another handoff observation of the
same bytes. Open tabs keep their revisions; new tabs take the new capture. A
tab may show a named comparison (`bpr/core/review_cycle_views.hpp`): This
round (a frozen round's bound revision, read from its payload, against a named
result or the latest capture), Since my last review (the reviewer's latest
explicit checkpoint, kept in schema 6's append-only `checkpoints` table and
never inferred from files opened or responses read), All cycle changes and
Choose revisions. A chosen pair, and a round with a named result, are pinned
and never move; the other views re-resolve only on the reviewer's refresh and
meanwhile say that newer material exists. Tab records keep the view and its
left revision as a trailer, so records written before comparisons read as full
source. The history (`describe_history`, `application::load_cycle_history`)
keeps local capture order, the checkout head each capture's manifest recorded
(same head, head moved with the commits between unobserved, another branch or
a detached head) and sender revisions mapped by an attached round apart; a
cycle observes no provider upload order and never infers one.

What recipients say back is appended, never merged (BPR-1164,
`bpr/core/review_cycle_response.hpp`). A structured response
(`bpr.cycle.response.v1`) is read as untrusted data with exact field sets, so
no field can verify, approve, publish, grant or rank its own trust; it must
name this cycle, the round it is attached to, that round's canonical digest
and each item's exact revision. A reply the reviewer pastes or types
(`cycle_manual_reply`) needs no JSON, identifier or protocol and may speak
about one item or the round as a whole. Both are stored as canonical objects
under schema 6's `responses` table with the channel this profile observed
(`response_file` or `manual_entry`), separate from the author, model and
result they claim. The same response again changes nothing; its identifier
with other content is refused. A partial response leaves the items it does not
name without a response, which is not "not attempted"; several recipients'
statements about one item sit side by side. Evidence is inert text that
nothing opens or runs, a claimed result revision resolves only once this
profile captured it, and appending a statement writes no source, captures no
revision, changes no note, suggestion or round and records no reviewer
decision. Output completing after the selection changed is dropped.

Verification is the reviewer's alone (BPR-1165,
`bpr/core/review_cycle_verification.hpp`). Reconciling a note against a
revision keeps four facts apart: where its original attachment stands
(exact, mapped, ambiguous, deleted, unmapped or unavailable), whether each
exact suggestion's whole-file result is now present (an illustrative one never
is), what recipients claimed, and the reviewer's decisions. Its status comes
only from the latest decision, or else from the claims ("addressed" asks for
verification); code that moved, disappeared or matches a suggestion never
resolves a note. A decision (`verified`, `needs_work`, `withdrawn`,
`deferred`, or an explicit clearing) is appended to schema 6's
`verifications` table with the note's current wording, the checked revision
and the digest of the material its locations held there (each named file's
blob or absence, or the whole manifest for a module or whole-cycle note).
When that material or the wording changes, the decision stays as history and
the note asks to be checked again. The next round's draft
(`plan_next_round`, `application::prepare_next_round`) reads the latest
frozen round's assignment back from its payload, carries its action, goal,
constraints and write scope and its still-unresolved items at their current
wording and suggestion revisions, adds open notes no round carried, and
leaves out resolved and archived notes, replies and context; it is saved as an
ordinary draft, and nothing is frozen, copied or sent.

Publication keeps three baselines apart (BPR-1172,
`bpr/core/review_cycle_outgoing.hpp`): `R0`, the first capture, which may be
dirty or partial; `Rn`, the selected final capture; and `P`, an exact Git
commit whose complete tree the outgoing candidate starts from, observed with a
target branch `T`. P and T are named by the reviewer and read as data from the
checkout's `HEAD`, loose references and `packed-refs`; P's listing comes from
the installed-Git publication catalog (`publication_candidate_port`), which
reads the checkout's objects as a per-command alternate of a BPR-owned store
and is admitted only when the commit's own bytes hash to P and name the tree
listed, and the listing rebuilds exactly that tree (`bpr/core/git_objects.hpp`
computes Git blob, tree and commit identities in both object formats). Every
changed path is listed with its starting work (`P -> R0`), improvement
(`R0 -> Rn`) and outgoing change (`P -> C`); a path a capture did not read,
left out or excluded keeps P's entry, so a module-only capture never deletes
the rest of the repository. The two starting choices never propose starting
work nothing in the cycle touched or named; under "only the improvements" an
improvement to a path whose starting work is omitted waits for the reviewer's
explicit choice. A change outside the starting capture has no known origin
and is not proposed; filter-managed and submodule paths and file/folder
collisions cannot be built natively; a candidate equal to P is "no change";
and a target that moved past P is shown with the selected paths it changed.
Planning writes nothing, anywhere.

A buildable plan becomes one squashed candidate (BPR-1173,
`bpr/core/review_cycle_candidate.hpp`): a single commit whose only parent is
P, whose tree is the plan's candidate, and whose author, committer, times and
message are exactly what the reviewer confirmed, with no trailer added and no
signature. The only byte-changing transformation is a named CRLF-to-LF
normalization of a selected text path, recorded with its before and after
blobs; it makes a different tree and commit. The host checks every retained
byte against the planned identity before installed Git sees it, stores blobs
with no filter, builds the tree in a private index and writes the commit with
`commit-tree --no-gpg-sign`, all in the BPR-owned store, and refuses the
result unless Git names each object exactly as the core computed. A changed
filter-managed (LFS) path, a submodule change, a file/folder collision,
required signing, an undecided improvement or an unchanged tree is a named
limit, never a silently different candidate. External preparation keeps the
selection usable: a binary patch against P and plain instructions to apply,
stage, commit and push with the reviewer's own tools, naming the paths their
filters must still process and saying it is not native publication.

## Authority by state category

| Category | Authority | Core representation | Qt representation |
|---|---|---|---|
| Host profiles | Host profile registry | opaque selected profile identity | non-secret configuration and selection controls |
| GitHub credentials | GitHub CLI store or native platform vault | none | none |
| GitHub shared state | GitHub/provider session in host | stable IDs, current revision facts, bounded observations | immutable cards/rows/actions |
| Staged GitHub results | Application host until a core acceptance decision | pending request and generation identity | none |
| Private durable state | SQLite ledger in host | current relevant receipt/actionability observations | badges, filters, commands |
| Review interaction | Functional core | active PR/file/change, epoch pair, selected semantic line/boundary/mapping, navigation mode, pending semantic action | selected indexes, focus, visible command state |
| Git/GitHub/diff caches | Host | exact blob/options/algorithm cache identities and availability observations | loading/unavailable projections |
| Layout and focus | Qt adapter, persisted through host settings | only semantic state when behavior depends on it | drawer width, measured alignment axis, live comparison scroll position, hover paths, expanded nodes, animation |
| Pending work | Core and host | request/generation/revision correlation | progress/cancel presentation |

Native credential authority is the exact tuple of profile, profile revision,
host identity, canonical hostname, authenticated account, and server
contract. The application contract adds request, application-session, and
identity-generation correlation to every vault operation and terminal result.
Credential bytes are neither state facts nor observations: they remain in a
move-only host-owned material object until the vault or credential-bearing
transport consumes and overwrites them.

When local and GitHub state disagree about a shared fact, GitHub wins. When a widget and the core disagree about semantic selection, the core wins and the adapter repairs the visual selection.

## Core application state

The core should retain only the state needed to make decisions, for example:

```text
application session identity
active profile/host/account/repository/PR identity
GitHub identity and request generations
compact tri-state host capabilities plus capability revision
accepted provider snapshot identity
selected left/right review epochs
active file material and change group
ordered alignment and selected semantic line/boundary identity
filtered navigation context
semantic selection
command state inputs
pending request identities and generations
review interaction mode
AI session/finding staleness state
compact diagnostics and status state
```

The core does not own all PR comments, all Git blobs, all diff rows, all queue entries, or the SQLite database. It receives the bounded records required for the current transition.

For GitHub reads, full normalized snapshots remain immutable host-owned
records. The core retains only the identity and summary required to decide
whether a staged snapshot belongs to the active session.

RFC-BPR-0003 adds a dedicated `epoch_state`; it does not add Git history to
`github_state`. The epoch state contains only:

```text
active pull-request authority and accepted source revision
active catalog incarnation and catalog revision
checked next-request, epoch-generation, and comparison-generation counters
requested alias or explicit comparison
last accepted exact comparison pair and comparison revision
at most one pending ingestion, comparison-resolution, and maintenance action
compact commands, status, and reason-coded diagnostics
```

It contains no string, path, ref, catalog page, review collection, Git object,
or dynamically sized history. `sizeof(epoch_state)` is capped at 2,112 bytes.
A catalog observation supplied to the core contains at most one latest live
epoch, one preceding distinct-head candidate, one current-viewer review
candidate, one material-match candidate, and the two requested anchors.

## Semantic input/output types

### Intents

```text
ActivateHostProfile
OpenPullRequest
RefreshPullRequest
SelectComparisonEpochs
SelectFile
SelectChangeGroup
NavigatePreviousFile
NavigateNextFile
NavigatePreviousChange
NavigateNextChange
MarkFileRead
AcceptFile
ExcludeFile
MarkConversationRead
AcceptConversation
CreateDraftComment
PublishReview
ClassifyThreadAsDefect
VerifyDefect
CreateObserverRule
StartAiReview
```

### Bounded observations

```text
CurrentGitHubIdentityObservation
CurrentHostCapabilitiesObservation
CurrentPullRequestSnapshotObservation
SelectedFileMaterialObservation
VisibleNavigationSetObservation
ReviewPermissionObservation
ReceiptObservation
ConversationVersionObservation
CurrentHeadObservation
AiProviderPolicyObservation
```

Observations expose facts, not service objects. They carry source identity/revision so the core can reject stale or mismatched decisions.

### Host requests

```text
EstablishGitHubSessionRequest
FetchPullRequestRequest
InvalidateGitHubIdentityRequest
EnsureGitObjectsRequest
ComputeDiffRequest
PersistReceiptRequest
PublishReviewRequest
ResolveThreadRequest
RefreshQueueRequest
ShowNativeNotificationRequest
LaunchAiSessionRequest
PersistSettingsRequest
```

Requests are semantic. The core does not request a `QMessageBox`, `QNetworkReply`, `QProcess`, or SQL statement.

### Result events

```text
GitHubSessionReadyEvent
GitHubSessionFailedEvent
PullRequestLoadedEvent
PullRequestLoadFailedEvent
GitObjectsAvailableEvent
DiffComputedEvent
ReceiptPersistedEvent
ReviewPublishedEvent
ReviewPublishFailedEvent
QueueRefreshedEvent
AiSessionStartedEvent
AiFindingReceivedEvent
RequestCancelledEvent
```

### Semantic deltas

```text
GitHubIdentityDelta
CapabilityDelta
AcceptedSnapshotDelta
PullRequestSummaryDelta
CommandDelta
SelectionDelta
FileReceiptDelta
ConversationDelta
QueueRowDelta
StatusDelta
DiagnosticDelta
```

Deltas describe meaning. Qt-specific row/index updates are derived by the presentation adapter.

`AcceptedSnapshotDelta` has a narrower role than a general content dirty bit:
it identifies the exact staged provider snapshot that the host may commit to
the active cache. A stale or rejected completion emits no such delta.

### RFC-BPR-0003 epoch vocabulary

The dedicated epoch path uses the following exact semantic records.

```text
Intents
    RefreshEpochs
    SelectComparisonAlias
    SelectComparisonAnchor
    CancelEpochRefresh
    PruneEpoch
    DeleteRepositoryCache

Accepted GitHub events
    AcceptedPullRequestRevision
    PullRequestRevisionInvalidated

Bounded observations
    AcceptedPullRequestRevisionObservation
    EpochCatalogWindowObservation
    GitEpochPreparationObservation
    ComparisonAnchorObservation
    MaintenancePlanObservation

Host requests
    InvalidateEpochAuthorityRequest
    LoadEpochCatalogRequest
    PrepareEpochRevisionRequest
    CommitEpochCatalogRequest(append epoch | associate review)
    ResolveComparisonRequest
    CancelEpochWorkRequest
    PruneEpochRequest
    DeleteRepositoryCacheRequest

Terminal events
    EpochCatalogLoaded | EpochCatalogLoadFailed | EpochCatalogLoadCancelled
    EpochRevisionPrepared | EpochRevisionPrepareFailed | EpochRevisionPrepareCancelled
    EpochCatalogCommitted | EpochCatalogCommitFailed | EpochCatalogCommitCancelled
    ComparisonResolved | ComparisonResolveFailed | ComparisonResolveCancelled
    MaintenanceCompleted | MaintenanceFailed | MaintenanceCancelled

Semantic deltas
    EpochAuthorityDelta
    EpochCatalogDelta
    EpochRowDelta
    ComparisonSelectionDelta
    ComparisonResolutionDelta
    EpochStatusDelta
    EpochCommandDelta
    EpochDiagnosticDelta
```

Selecting one side carries the side, exact anchor identity, and catalog
revision observed by Qt. Changing one side of a named alias converts the
selection to an explicit pair using the other last-accepted exact anchor.
Neither labels nor displayed OID text are accepted back as identity.

## GitHub capability state

Each host capability is represented as `Unknown`, `Supported`, or
`Unsupported`. `Unknown` means that the current profile/account session has
not produced sufficient evidence; it cannot enable a command and is not
presented as a proven server limitation.

Capability observations carry the selected profile revision, host, account,
probe-contract revision, and `CapabilityRevision`. Switching profile or
account clears the compact core capability set to `Unknown` and invalidates
the host's identity-sensitive capability cache.

RFC-BPR-0002 observes review-thread reads, viewed-file mutations,
resolve-thread, and unresolve-thread support. Observing a mutation capability
does not authorize its execution before RFC-BPR-0008.

The host cache key also includes the enum-backed server contract. Only a
complete recorded or observed contract can enter the cache; transient or
insufficient evidence is an explicit all-`Unknown` unavailable result and is
never cached. Logical freshness epochs, bounded capacity, generated
capability revisions, and exact/profile/identity invalidation remain
application-host state rather than core timing or eviction policy.

Host capability and target legality are separate. A normalized file carries
an unavailable, dismissed, unviewed, or viewed state. A normalized review
thread carries `viewerCanResolve` and `viewerCanUnresolve`. A false target
flag cannot downgrade the host schema capability, and a supported schema
capability cannot manufacture authorization for a target whose observation
does not permit the action.

## Staged provider snapshots

The provider completion path is transactional at the host/core boundary:

```text
immutable normalized completion
    -> serialized application lane
    -> host staging keyed by complete request context
    -> typed core event
    -> exact accepted snapshot delta
    -> host cache commit and bounded projection
```

The host discards a stage when the core rejects its request, application
session, profile, host, account, repository/PR, or generation identity.
Neither staging nor a provider callback changes visible or authoritative
state. See [GitHub Host Implementation
Architecture](github-host-implementation.md) for the concrete allocation.

## Pull request and epoch

```text
PullRequestAuthority =
    profile and profile revision
    + host and authenticated account
    + repository identity
    + pull-request identity and number
    + authority-scope identity

EpochRequestAuthority =
    application session
    + pull-request authority

PullRequestSourceRevision =
    provider snapshot
    + GitHub identity generation
    + pull-request request generation

PullRequestRevision =
    source revision
    + stable base-branch identity
    + algorithm-tagged base and head commit OIDs

ReviewEpochId =
    authority scope
    + catalog incarnation
    + epoch ordinal
```

The profile revision and complete accepted GitHub request context enter the
RFC-BPR-0003 handoff even though raw profile configuration remains host-owned.
The host validates that the fixed authority-scope identity is bound to that
complete tuple. A source revision is operational correlation; the exact
base-branch/base-OID/head-OID record is the PR revision whose history is
retained.

An epoch records exact base/head commit and tree identities, host-supplied
observed time, ordinal, origin, independent base/head transitions, material
identity, and availability. Ordinal defines observation chronology; wall time
does not.

The update line also has a separate provider-evidenced occurrence history.
Its portable identity uses canonical host, numeric pull-request identity and
typed event/head evidence. A local ordinal never orders that list across
machines. Event UTC, cached commit UTC, and local first-seen are distinct
facts; mixed timestamps use a deterministic tie-break that does not assert
push order. A timeline failure, page cap, missing update boundary or ambiguous
association leaves history explicitly incomplete. A historical occurrence
without an evidenced base stays visible without a comparison anchor.

Cause is composite rather than a lossy enum:

| Axis | Values |
|---|---|
| Origin | initial observation, live observation, explicit historical import |
| Base transition | not applicable, unchanged, base OID changed, base branch changed |
| Head transition | not applicable, unchanged, descendant, non-fast-forward, lineage unknown |

With no prior live epoch the origin is `initial observation`. A historical
import requires an explicit source and an exact base/head pair; a review
commit alone cannot manufacture a historical epoch because it does not prove
the historical base. For a live update, a changed branch identity is a
base-branch change even when the OID happens to match. With an unchanged
branch, a changed base OID is a base update. A changed head is an author
upload only when the prior head is proven to be its ancestor, a
force-push/rebase only when non-ancestry is proven with both objects resident,
and otherwise `lineage unknown`. Unknown lineage is never guessed as a
force-push. An unchanged branch/base/head triple creates no epoch.

The primary presentation label has deterministic precedence:

```text
initial observation
explicit historical import
base-branch change
force-push/rebase
author upload
Head change; lineage unavailable
base update
```

Both axes remain in the record. For example, a simultaneous base update and
author upload presents the upload as primary and retains the base update as a
secondary fact.

Compared material is the ordered tuple of base tree OID, head tree OID, and
comparison-policy revision. Two epochs are materially equivalent only within
one authority when both tuples are present and exactly equal. Equivalence
never suppresses an epoch, changes its cause or chronology, migrates a
receipt, or treats unknown material as equal.

A comparison anchor is either an epoch's exact base/head anchor or an exact
review-associated commit anchor. The latter remains distinct even when its
OID equals an epoch head. Resolved anchors carry availability and the exact
commit OID. Same-authority retained anchors may be compared in either order,
including reverse and identical pairs. Cross-authority, cross-incarnation,
missing, pruned, or unavailable pairs are illegal.

Named comparisons are semantic:

- `Whole PR` resolves the latest live epoch's base to its head.
- `Previous upload -> latest` resolves the greatest earlier live epoch whose
  head differs from latest to the latest head. A force-push epoch qualifies;
  intervening base-only epochs do not hide the preceding distinct head.
- `My last reviewed -> latest` resolves the current viewer's newest eligible
  review-associated commit to the latest head.

Historical imports are selectable explicitly but are not eligible to become
`Latest` or `Previous upload`, because import order is not evidence of PR-tip
chronology. A named alias re-resolves when its authoritative catalog/review
facts advance. It first selects the semantic candidate and then checks
availability; it never skips a pruned candidate to an older one. An explicit
pair remains exact and becomes visibly unavailable rather than silently
jumping to `Latest`.

GitHub's complete normalized review collection carries a one-based
`ReviewOrder` preserving the documented chronological REST order across
pages. `My last reviewed` chooses the maximum order whose author actor
identity equals the accepted viewer, whose state is not pending, and whose
commit is present. Login text, review-ID ordering, timestamps, and incidental
container order are not substitutes. The selected review stays bound to its
accepted provider snapshot; incomplete or rejected pagination changes
nothing.

An accepted snapshot whose base, head, and branch are unchanged creates no
epoch. If its newest-current-viewer review fact is new or changed, the core
instead authorizes an `associate review` catalog mutation. That
expected-revision SQLite-only mutation advances the catalog revision when it
changes the durable association, re-resolves review aliases, and creates no
Git ref or epoch ordinal.

The host owns epoch persistence and retained Git refs. The core owns the current left/right epoch selection and comparison policy. The Qt adapter owns selectors, labels, popup layout, and unavailable-state presentation.

## File material

A file review receipt attaches to immutable compared material rather than a path alone:

```text
FileMaterialId = hash(left blob, right blob, old path, new path, change kind, comparison policy)
```

This preserves state across pure renames where appropriate and invalidates state when compared content changes. Blob contents and diff caches remain host-owned. The core operates on IDs, summaries, and bounded selected material.

## Private receipt states

Files:

```text
Unseen -> Seen -> Read -> Accepted
                     └── changed material -> ChangedSinceRead
Accepted + changed material -> ChangedSinceAccepted
Excluded is an explicit terminal state for the current material only.
```

Conversations:

```text
Unread -> Read -> Accepted
new or edited comment -> Unread
```

GitHub thread resolution is distinct from private acceptance. Receipt/actionability transitions are deterministic core policy; SQLite storage and queries are host effects.

## Defect projection

```text
Draft -> Published/Open -> FixClaimed -> NeedsVerification
NeedsVerification -> Verified | Reopened
Open -> Withdrawn | AcceptedAsIs
```

The underlying published object remains an ordinary GitHub thread. `FixClaimed` and `NeedsVerification` are local interpretations until represented by normal human-visible GitHub replies. The core owns legal transitions; the host correlates GitHub/thread observations and persists local classification; Qt presents badges and actions.

## Asynchronous identity

Every request carries the identity required for deterministic acceptance:

```cpp
struct request_context
{
    request_id Request;
    application_session_id Session;
    host_profile_id Profile;
    git_host_id Host;
    github_account_id Account;
    pull_request_key PullRequest;
    std::optional<review_epoch_id> Epoch;
    request_generation Generation;
};
```

Not every request needs every field, but no completion may be accepted merely because the originating widget still exists.

Session establishment, which precedes authenticated account discovery, carries
request, application session, host profile, and generation. A PR fetch adds
host, account, repository locator or identity, PR number, and all applicable
revision facts. Identity switching advances the generation before cancellation
or invalidation effects are performed.

Epoch catalog mutation and comparison resolution use dedicated contexts:

```text
EpochMutationContext =
    request
    + application session
    + complete pull-request authority
    + accepted pull-request revision
    + epoch generation
    + catalog incarnation
    + expected catalog revision

ComparisonContext =
    request
    + application session
    + complete pull-request authority
    + accepted source revision
    + catalog incarnation/revision
    + comparison generation
    + requested alias or exact anchors
```

All increments are checked before state changes. Exhausted request,
generation, ordinal, or revision space produces a typed terminal diagnostic
and no request; values never wrap or reuse zero.

A completion with no matching pending request or an older generation is
stale. A completion matching request/generation but disagreeing on another
context field is an identity mismatch. Neither case clears current pending
work or publishes catalog/comparison deltas, and a duplicate terminal is
stale. Catalog mutation uses compare-and-swap on the expected revision; a
conflict commits nothing and requires a fresh catalog observation.

Cancellation is advisory and every normally accepted in-process submission
during the live host lifetime produces exactly one terminal. An abrupt process
death instead leaves exactly one typed durable operation outcome for recovery;
it does not promise delivery to the dead application session. Before the host
records `ref_commit_in_progress`, cancellation may return a cancelled terminal
and no accepted epoch. That recorded phase is the point of no cancellation,
not proof of durable success: recovery must resolve the exact operation to
success or a typed failure and cannot call it cancelled. The catalog visibility
transaction is the durable success and linearization point; once it commits,
the terminal is success even when cancellation raced. Switching authority
advances all epoch/comparison generations before cancellation and clears
identity-sensitive projections. A same-authority refresh may retain the last
accepted pair as visibly refreshing, but no pending, failed, cancelled, or
stale pair becomes authoritative for downstream diff work.

## Qt-local state

The following normally stays in the Qt adapter rather than the core:

- focused widget and hover state;
- file-tree expansion;
- scroll positions and horizontal anchors;
- drawer widths and splitter geometry;
- transient selection rectangles;
- animation progress;
- font metrics and text-layout caches;
- native menu/popup placement.

The host may persist user settings, but that does not make the core or database the runtime owner of Qt mechanics.

## Private review core foundation (RFC-BPR-0023 M3)

`private_review.hpp` supplies deterministic local proposals. The M4 SQLite
store, M6/M7 application hosts and M8 workbench composition now implement the
durable path described in [review-ledger](review-ledger.md) and
[review-round-trip](review-round-trip.md). A durable
private reference carries profile, ledger incarnation, host, account,
repository and pull request. New threads retain exact base/head and epoch
anchors; migrated notes use an explicit legacy variant and cannot acquire the
currently displayed revision by restoration. Subjects reuse the immutable diff
semantic index; no display coordinate or nearest-line fallback is accepted.

Revisions are immutable values. Reply records an exact prior revision. Edit
returns a reviewer successor and predecessor edge while retaining the original
actor and body digest. Agent actors require an exact run observation; partial,
failed and cancelled output is ineligible until explicitly adopted as reviewer
text. Open, resolved-locally and archived govern only local lifecycle. Current,
stale and unplaceable are observations and never replace the binding.

The compact request state retains four fixed lanes and checked generations;
terminal acceptance compares complete scope/session/window/request identities.
The host must persist a proposed write atomically before projecting it and must
supply authoritative allocation, latest-revision CAS and source-existence facts.
This foundation has no database, invocation, publication or Qt capability.

`review_composition.hpp` adds exact turn/finding/composition source references
and versioned, length-framed selection/finding digests. A new derivation vertex
may reference only observed, same-scope vertices with smaller creation
ordinals. Edges are admitted with a new vertex; existing vertices reject
later attachment. Transactional allocation and existence remain store duties.

Run provenance snapshots session ID, profile, protocol, entry point, binding,
selected references, input-selection/output digests and terminal facts.
Requested, configured, protocol-reported and content-self-reported model/effort
are separate optional fields. Reports retain their event/field basis; differing
values yield typed conflicts with both locations. Missing values are not filled
from profile labels. A conversation result must match the run's output digest.
Finding snapshots retain all structured fields and the validation verdict;
identity is run, ordinal and structured-content digest, independently of session
catalogue retention.

Composition admits at most 256 exact sources, rechecks digests and displayed
tips, and requires explicit historical selection. Incomplete sources refuse;
known stale sources remain usable locally. Exact target partitioning retains
unassigned members. Only a unique target shared by every member is proposed
automatically; explicit target choice is required otherwise. Reviewer-authored,
exact-copy and explicit agent-assisted modes produce immutable local candidates.
An exact copy retains its source actor, and every edit creates a reviewer
successor. Candidate bodies are at most 16 KiB.

Acceptance emits `accept_composition_as_draft_request` for the existing draft
owner. It carries the exact candidate, target, prose and optional draft CAS;
it cannot publish, resolve a thread or submit a review. Only a matching durable
acknowledgement changes composition lifecycle to `accepted_as_draft`.
`revisioned_review_draft` now carries an optional exact composition link.
Editing linked prose requires a matching successor link; ordinary draft
commands retain the link. Separate existing Publish semantics remain in the
draft reducer. Duplicate source-set/body/target evidence is advisory only.

These M3 core contracts are tested without Qt or effect ports. The later store,
invocation and public-path implementations retain the same authority boundaries.
The raw selected-private run budget is 2 MiB; the invocation boundary separately
admits at most 8 MiB of encoded process input. The local public paths have
fixture evidence; designated GitHub publication readback remains pending.

## Review-tour identities and local authority

`tour_draft_id` and `tour_stop_id` are stable local identities, scoped to the exact
profile, host fingerprint, account, repository and PR. A stop retains its original
comparison/commit/blob/path/side and contiguous inclusive source interval separately
from a suggested later mapping and any GitHub comment binding. Vector order is the
conceptual route; a title, visible ordinal or painted row is never identity.

The bounded tour aggregate belongs to its dedicated host, outside the compact
global interaction state. Host effects persist revision-checked drafts and raw
baseline/local/website versions. The core decides edit/refusal/readiness and
captured AI-result scope; Qt owns native text history and focus. Preparation and
reading progress confer no review correctness, receipt or shared mutation state.
The [tour contract](review-tours.md) specifies ranges, budgets, persistence,
publication grants and migration versions.
