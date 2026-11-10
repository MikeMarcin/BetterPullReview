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

BPR-1979 remains incomplete. The editor-repairs milestone is accepted for
source candidate `91830ea7edde0002c7d7a23f05903dbef0fe52df`: per-stop reasons and
selection, exact current-head mapping, previewed relocation, ordinary Undo/Redo,
replanning, and repaired single-review publication are composed and verified.
The user guide explains both repairs and the separate file-operation boundary.
No native appearance, live server, or other-platform acceptance is claimed.

`mixed-publication` must implement the explicitly chosen review/conversation
split, cross-kind overview/navigation, interruption reconciliation and published
maintenance, and publish the authorized RFC-BPR-0041/0042 amendment with its
verified behavior. No mutation sequence or recovery journal was changed here.
Existing compatibility-only behavior remains available; no outcome is selected
silently.

The owner-supplied GHES 3.20.8 pending-line probe answers BPR-1737's second
criterion only. Its separate file-comment criterion remains open, without an
invented build or live-host verdict.

## Editor-repairs milestone result

The desktop preview binds current provider hunk bounds and bounded pinned-source
comparisons to the accepted owner/head. Each historical pair is admitted and read
serially through the existing epoch actor, with 256 KiB per side and cancellable
core computation. Unavailable source stays unavailable. Results for a replaced
draft, owner or revision cannot apply an edit. The core's exact-byte mapper supplies
re-pin candidates; relocation shows the actual retained head text before Apply.
The ordinary review-tour host owns repair, selected identity, Undo, Redo and save.
Typed repair rows are refreshed on edits and history projections.

Frontend-design's installed desktop guidance was loaded before UI work. Repair
rows inherit native fonts/palette and present stop links, reasons and the relevant
actions above the explicit publication choice. No desktop was driven or captured.

A synthetic three-commit fixture, built by `scripts/build_tour_repair_fixture.py`,
retains review.cpp bytes from the historical commit to head; the provider patch
names the complete three-context-line hunk, lines 3–9. The composed ten-stop
`application_review_tour_publication_repairs` course reads its source through
installed Git/cache and uses the public editor controls through publication.
The exact candidate above passes that course in 4.47s, including a negative
control proving that an intervening edit expires the relocation preview. It
asserts one pending creation, one submission, ten native comments, zero ordinary
review shortcuts and zero compatibility comments. The core repair and real host
courses pass; architecture, network manifest, documentation and 67 documentation
regressions pass. LLVM22 format and tidy pass. Offscreen 1200×760 logical renders
were inspected at 1×, 1.25× and 2× (the scaled courses pass in 3.48s and 3.59s).
These are macOS arm64, Debug, Qt 6.12.0, synthetic ten-stop fixture observations,
not native system-appearance or performance qualification. Exact commands,
initial failures and correction evidence are retained in the checkpoint.

The supplied **Publish as file comment** requirement remains mandatory in the
mixed-publication milestone. Existing standalone-file capability facts must not
be mislabeled as pending-review membership. No exact file operation is newly
qualified or enabled in this milestone; unknown/denied facts remain unavailable.
The successor must implement an honest supported file route and its explicit
choice, or retain a concrete unmet obligation. It must not count a standalone
comment as one pending review. AC5/AC7 and both RFC amendments remain open.

### Selected acceptance at the editor boundary

All rows bind source candidate `91830ea7edde0002c7d7a23f05903dbef0fe52df`.

| Criterion | Result and direct evidence |
|---|---|
| AC1 | Pass for the editor: typed historical-exact and off-hunk rows, stop numbers/titles/reasons/actions, affected-stop selection; composed course plus inspected renders. |
| AC2 | Pass: the exact pinned Git comparison enables one re-pin; the native batch uses the repaired effective source. Changed/ambiguous/unknown negatives reuse the accepted core evidence. |
| AC3 | Pass: actual head text previews the nearest accepted context range, Apply preserves prose and selection, Undo/Redo restore/re-plan, intervening edit expires Apply. |
| AC4 | Pass: the repaired ten-stop composed route creates one pending review, submits once, retains ten native comments, and creates zero compatibility comments. |
| AC6 | Pass for the current choice set: compatibility is explicit and follows repairs; the existing composed compatibility course passes. M3 must put mixed publication before it. |

The three editor milestone exits pass through this composition, guide and scoped
verification. These selected records do not accept AC5, all of AC7, or the
remaining supplied file-comment control. The parent stays In Progress.
