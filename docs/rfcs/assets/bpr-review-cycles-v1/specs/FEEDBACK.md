# Source-anchored feedback and exact suggested edits

## Author naturally

From full source or a diff, select a range and write a note. File-, scope/module- and
cycle-level notes are first-class. One item can name a primary source location plus
several related references, including other files and immutable earlier revisions.
Do not duplicate a design concern into unrelated threads solely because it spans files.
A title, label, severity, recipient, model or action field is not required per note.
The round-level assignment normally supplies the requested action once.

Reuse the source editor, native Markdown reader, comment composer and label model
from the current tree. The label inventory is exactly praise, nitpick, suggestion,
issue, todo, question, thought, chore, note, typo, polish and quibble, plus Plain.
Labels do not imply patch presence, apply authority, blocking severity or verification.
Do not create a separate set of cycle-only label icons. The shared optional-label
parser and raw Markdown remain lossless.

A cycle's required private threads must not depend on shipping all of deferred
RFC-BPR-0023. Extract/reuse its minimal author/version/lineage concepts behind a shared
provider-independent component, or adapt the component if it has since landed.
Record exactly which narrow common slice this pack schedules. Leave the rest of
0023's PR promotion program and its deferred status alone; never implement a second
incompatible provenance system or falsely mark that RFC complete.

## Original attachment versus current correspondence

Store the original source revision, exact file/blob and half-open byte range, original
text/context and optional symbol/navigation hints. Source line ranges are a projection
(one-based inclusive for display), not the durable patch coordinates. Module/cycle
notes have scope identities rather than arbitrary line zero anchors. Related references
retain their own source revisions and inclusion/disclosure decisions.

Correspondence to newer material is an observation: `exact_material`, `exact_mapped`,
`ambiguous`, `deleted`, `unavailable` or `unmapped`. A symbol name or nearest line is a
hint, not proof. Repeated snippets and moved/copied blocks must not choose silently.
Present original source beside a candidate, and let the reviewer bind a new reference
without rewriting the old one. Exact historical views remain pinned while new
material arrives. Discussion on old source is allowed in a local cycle; actionable
replacement against today's source needs a new exact binding.

## Suggest change

`Suggest change` opens a focused code editor prefilled from the selected bytes. The
user changes that small section and can add rationale. Preview shows before/after
without touching the working tree. Proposals are local review data until separately
applied by the recipient or through an explicit supported apply action.

Each suggestion revision contains:

```text
suggestion_id, revision, item_id
source_revision_id, file_identity, original_blob_sha256
start_byte, end_byte                 # half-open, insertion permits equality
original_bytes, replacement_bytes    # exact byte vectors, even empty
intent = exact_replacement | illustrative
state = proposed | observed_already_applied
optional already_applied_result_revision
```

Wire bytes use base64 with hashes and byte lengths; Markdown is a lossless-readable
presentation for supported text, not the source of truth for arbitrary bytes. Empty
replacement means deletion; empty range means insertion. A round may carry several
suggestions from one note. References to user-edited code already in the working tree
must be labelled already applied after observation, never emitted as another apply
instruction. An unknown state stays unknown rather than inferred from similarity.

## Exact application and conflict policy

A proposal must still match its entire expected original blob and range before an
exact application. Matching some nearby text is insufficient. If the file changed,
show/retrieve a mapped candidate and create a revalidated suggestion revision; do
not silently fuzzy-apply. The recipient may implement the intent differently and
explain that choice. `illustrative` snippets must never be advertised as automatic
patches. Exactness is a byte property, not proof that an edit is semantically correct.

Multiple exact edits to the same blob use coordinates in the same original version.
Check the full set first and apply nonoverlapping edits in descending offset order.
Overlap, contained ranges, or two insertions at the same boundary are conflicts
unless the author explicitly combines them into one new suggestion. Insertions on a
replacement boundary are conservatively treated as conflicts in v1. Do not order
conflicting edits by UI position or silently make the last writer win. Suggested code
must not be compiled or executed while previewing or importing it.

## Editing ergonomics and history

Provide genuine virtual space, per-line box/multi-caret selections, native clipboard
behavior, undo and stable IME handling where the shared editor supports them. Ensure
the user's Alt+Shift+Down then Shift+End workflow remains controlled and no caret
silently traverses a newline. Inventory the actual current editing RFC/component at
intake; RFC-BPR-0036 is native Markdown **rendering**, not the multi-caret RFC.
Do not label an unrelated renderer as fulfillment of editing requirements. Fill any
necessary narrow suggestion-editor gap in this pack and test it natively.

Changing a label, rendering Markdown, freezing/exporting, switching interfaces or
receiving an optional AI wording result cannot alter source bytes, suggestion fences,
replacement bytes or intent. Imported/authored prose is preserved. Optional text
assistance, if present, must be explicitly invoked and use existing scoped hooks;
no summarizer or model call is needed to construct a round.

## Provenance and visibility

Record human-authored, observed adapter-attributed, self-reported and unknown provenance
separately. Provider/model/effort fields are optional observations, not invented facts.
A manually pasted agent answer is self-reported unless independently corroborated.
Local derivation links survive composition but are not exported automatically.
Freeze only selected item revisions and explicitly selected context; exclude private
scratch, earlier hidden turns and identity-sensitive metadata by default. The public
PR description/tour later uses its own explicit selection and publication path.
