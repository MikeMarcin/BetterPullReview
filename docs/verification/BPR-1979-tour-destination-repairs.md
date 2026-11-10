# BPR-1979 tour destination repairs

Mandate: `tour-destination-repairs-r1`; immutable baseline/RFC_BASE_SHA
`cf9413b9f76dd34e299bc46518a52127b18b175c`. The imported card and
[adopted plan](../rfcs/plans/tour-destination-repairs-v1/milestone-plan.json)
preserve the owner's complete repair and mixed-publication request.

## Repair-policy milestone (BPR-1980)

The core now exposes `tour_destination_repairs` and `plan_tour_stop_repair`
from `bpr/core/review_tour_repair.hpp`. They return stable stop identity,
route number/title, typed problem, the destination plan, exact-head re-pin
and nearest-hunk range candidates, and an exactly qualified file-comment fact.
No message text is parsed to determine a destination problem. Callers supply
operation observations and current-head provider hunks under the tour's exact
observed-head context. Missing/mismatched source observations stay unavailable;
unknown/denied/rate-limited/indeterminate operations grant no fallback write.

`prepare_tour_source_repair` recomputes a selected repair and returns one
ordinary `tour_command_kind::restore` command with the captured draft owner,
revision and selected stop. The existing local host owns Undo, Redo and
autosave. Published tours/remote subjects remain outside this source-edit seam.
The immutable original and explanation are retained; the effective source is
an author-selected mapping. Changed explicit line citations (`line N`, `lines
N–M`, `LN`, source-link anchors and their ranges) require reaffirmation; ordinary
prose and unrelated numbers preserve readiness. No generated visitation or
publication authority is conferred.

Both accepted-stop relocation and agent proposal relocation use
`nearest_tour_hunk`, with provider context bounds, deterministic provider-order
ties, preserved length where it fits and shortening within a smaller hunk.
Malformed, deleted or unknown hunk evidence supplies no relocation.

The existing revision mapper additionally compares retained line bytes,
including terminators. A semantic exact-line match under ignored whitespace or
normalized line endings cannot claim `unchanged_material` and enable re-pin.
An exact historical mapping is offered only at the current head, on its right
side, with a complete blob/range and known commentable hunk.

A definitively unsupported off-hunk, pending-file and standalone-file sequence
may plan a supported conversation permalink. Unknown or failed operations do
not qualify that fallback. The author's explicit compatibility selection is
still required by `assess_tour_publication`; these policy values perform no
remote effect.

## Verification

The focused core course builds a ten-stop route with one historical exact stop
and one stop immediately outside a hunk. It identifies both, prepares and
applies two ordinary edits, retains all narratives and identities, and assesses
all ten as `single_review`. Negative cases retain changed/ambiguous/missing or
stale source mappings, mismatched observations, unknown/denied operation facts,
read-only protection and stale command rejection. Boundary cases cover context,
range length, deterministic ties, malformed ranges and upper-bound arithmetic.

The actual `review_tour_host` course dispatches the prepared accepted-stop
repair, preserves selection, undoes/redoes exactly, and drains the resulting
ordinary autosave on shutdown. No provider, network or desktop is composed.

Exact candidate and final command results are recorded in the
[checkpoint](../issues/_checkpoints/BPR-1979/handoff.md). Runtime-only logs live
under `/tmp/bpr-tour-repair-run`; no build output or transient log is committed.

## Remaining parent acceptance

BPR-1979 remains incomplete. `editor-repairs` must wire per-stop reasons and
selection, obtain exact current-head source/mapping and qualified operation
facts, preview new source text before relocation, dispatch the prepared local
command, re-plan after edit/Undo, and prove repaired single-review publication
through the real composed public editor. It also owns the operator guide and
frontend-design/offscreen rendered evidence. No UI, live server or native
appearance acceptance is supplied by this core milestone.

`mixed-publication` must implement the explicitly chosen review/conversation
split, cross-kind overview/navigation, interruption reconciliation and published
maintenance, and publish the authorized RFC-BPR-0041/0042 amendment with its
verified behavior. No mutation sequence or recovery journal was changed here.
Existing compatibility-only behavior remains available; no outcome is selected
silently.

The owner-supplied GHES 3.20.8 pending-line probe answers BPR-1737's second
criterion only. Its separate file-comment criterion remains open, without an
invented build or live-host verdict.
