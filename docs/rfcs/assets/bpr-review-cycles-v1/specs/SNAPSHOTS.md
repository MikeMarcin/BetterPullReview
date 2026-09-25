# Workspace capture and source-first material

## Source modes

Support an explicitly chosen local Git checkout (including linked worktrees and
uncommitted content) and a plain folder/file. No GitHub sign-in, remote discovery,
branch creation or commit is required. A single file is a legitimate scope. A module
is a named path set; do not require semantic project indexing to start reviewing.

Git origin may add a pinned commit/tree, object format and stage/working-tree
observations. The bytes actually inspected remain authoritative: `HEAD` is not their
identity when they contain edits. The initial working-tree projection uses current
file bytes, not the index's staged bytes; staged-only differences remain separately
observable and are never silently substituted. An unmerged index is visible and
publication-blocking, not a reason to lose the readable source.

Preserve repository/folder logical identity apart from absolute paths. Rebinding to
a different checkout requires an explicit mapping and material check. A short cycle
alias plus whatever happens to be the active window is not a workspace binding.
Untrusted exports cannot select a filesystem root. Cross-machine imports require the
recipient to map the logical project to their own checkout before source lookup.

## Scoped manifests, not inferred deletion

Capture an immutable, sorted inventory of admitted paths. Each entry describes
`file | symlink | gitlink | directory_marker` as supported, raw content hash/size,
portable Git mode where known, encoding/display eligibility and origin. Paths absent
from a *partial* capture are **not deleted**. Record scope, enumeration completeness,
explicit deletions, ignored/unavailable items and any size refusal separately.
Changing review scope produces a new scope version; it must not look like a mass
file deletion. A full-source tab can extend the browsing scope after explicit selection;
that does not retroactively claim the original baseline contained that file.

Default selection uses tracked files plus visible nonignored untracked candidates
for Git roots; show inclusion before capture and allow remembered per-cycle exclusions.
For folders use a small disclosed exclusion set for VCS/admin/cache/build artifacts,
not an undocumented crawl of everything. Never ingest `.git`, credential stores or
BPR's own private state. Ignored/untracked files can contain secrets; inclusion is
explicitly visible. Filename heuristics are guidance, not a promise of secret scanning.

## Safe reading and capture quality

Open the root deliberately; validate descendants against the retained root identity.
Use handle-relative/no-follow semantics and platform equivalents to prevent symlink,
junction/reparse, root-replacement and time-of-check races from escaping the selected
scope. Preserve a symlink's target bytes as data; do not traverse it to capture outside
material. Gitlinks are boundaries, not automatic recursive submodule initialization.
Sparse and missing objects do not trigger lazy fetch or checkout scripts. Network and
credential capabilities are absent from capture.

Read ordinary files in bounded chunks on workers, hashing raw bytes. Record and
recheck stable file/root identities, size and change metadata around reads, re-enumerate
scope and detect concurrent mutation. Do not rely on timestamp equality alone. Where
necessary rehash/retry within a finite budget. A detected unstable capture is discarded
as a candidate and offers retry; it never advances Latest or sends a partial round.

**Honest guarantee:** ordinary recursive filesystem copying cannot prove that all
files coexisted at one instant in the presence of arbitrary writers. Two agreeing
passes do not prove global atomicity. Record `capture_quality=observed_stable` for
this path and describe it as a quiescent handoff observation, not an atomic snapshot.
An immutable Git tree or a separately qualified filesystem snapshot can use
`capture_quality=immutable_source`. Cooperative pause/end-of-turn handshakes improve
stability but are optional; no integration is required. Capture after the recipient
finishes is the normal manual workflow. A per-cycle nonintrusive quality indicator is
sufficient; do not require a warning dialog on every stable manual capture.

## Byte and platform rules

Content identity is algorithm-tagged hash of the captured raw bytes. A raw content
hash is not a Git blob OID; Git blob hashing has its own encoding and object header.
Retain line endings, BOM and missing-final-newline. UTF-8 text ranges use byte offsets
checked at scalar boundaries; UI columns and rendered rows are a separate mapping.
A decoder must supply exact byte correspondence. Unsupported encodings/binary files
remain file-level reviewable, with a truthful preview limitation and no lossy patch.

Admit supported path bytes without lossy normalization. Wire paths for non-UTF-8
names carry raw bytes plus a safe display label. Detect case-fold, Unicode-normalization
and reserved-name collisions before materializing on a different platform. Never
quietly combine distinct entries. File mode changes are material changes. Windows
metadata must not invent executable bits for newly created files without an explicit
Git-mode policy. Large/binary/unsupported files are visible, not silently skipped.

For LFS/filter-managed content, distinguish pointer blobs from materialized working
bytes. Capture does not execute filters or LFS tools. Native publication of changed
material requiring those transformations is capability-blocked unless a separately
qualified transformation path is available; preserve the local review and offer
external preparation. Existing untouched base-tree entries remain unchanged.

## Retention and cost

Store immutable content-addressed raw blobs under a local-cycle namespace with
reference-count/pin discipline, not an authenticated PR-cache alias. Reuse identical
blob bytes within the admitted local scope; metadata/path history remains separate.
The diff pipeline consumes exact material providers and options already used by BPR.
Capture and enumeration are cancellable, paged and bounded. Keep file trees and notes
virtualized; no GUI-thread whole-project hashing and no full transcript in each core
transition. Watchers only signal possible changes; they do not create revisions,
rewrite tabs, save a round, or dispatch an agent automatically.

See [LIMITS.md](LIMITS.md) for initial budgets and [ITERATION.md](ITERATION.md) for
comparison rules. Proposed numeric budgets require measured native tuning at intake,
not silent removal when a fixture is inconvenient.
