---
id: "RFC-BPR-0004"
title: "Diff Computation, Correspondence Mapping, Grouping, and Alignment-Space Scrolling"
status: "Auto-Verified"
area: "BPR"
milestone: "M1 Native Read-Only Workbench"
pack_version: 2
rfc_base_sha: "243295a70756b2e63a785ed11b14647b6b0a4c6d"
replacement_pack: "betterpullreview-rfc-bpr-0004-diff-alignment-scroll-replacement-v1"
revision: 2
replaces_revision: 1
depends_on: ["RFC-BPR-0003"]
required_by: ["RFC-BPR-0005","RFC-BPR-0006"]
architecture_contract: "The core decides; the host performs; Qt presents"
---

# RFC-BPR-0004 — Diff Computation, Correspondence Mapping, Grouping, and Alignment-Space Scrolling

## 1. Status and replacement scope

This document is the complete revision-2 replacement for the earlier `RFC-BPR-0004-diff-computation-line-mapping-grouping.md`. It preserves the RFC ID, canonical filename, predecessor, dependency position, and downstream numbering.

The replacement makes one architectural fact explicit:

> BetterPullReview scrolls a comparison, not one document plus a follower document.

The implementation owns one monotonic position in a shared alignment space and projects that position into both source documents. The same semantic alignment model also drives line mapping, change grouping, previous/next-change navigation, selected-line detail, and the center correspondence link map.

This is a product behavior and architecture requirement, not a later polish option.

## 2. Motivation

A conventional side-by-side diff often chooses one of two compromises:

1. insert synthetic blank rows so matching lines remain horizontally aligned; or
2. retain each document's natural layout but couple independent scrollbars with approximate follower logic.

The first damages source continuity, wastes vertical space, and makes line identity less direct. The second breaks down precisely where a reviewer needs help: insertions, deletions, and unequal replacements. It can jump, drift, choose the wrong anchor, or produce discontinuity when the pointer moves from one pane to the other.

BetterPullReview instead treats a two-file diff as an ordered correspondence path through two document coordinate systems. A center link map visualizes changed intervals along that path. A measured presentation axis projects a single comparison position into both panes.

This yields the desired behavior:

- matching regions move together;
- a right-only insertion advances on the right while the left remains pinned at the corresponding boundary;
- a left-only deletion advances on the left while the right remains pinned;
- unequal replacements move proportionally until their endpoints meet;
- reversing direction retraces the same path without correction jumps;
- wheel input over either pane acts on the same comparison position;
- neither document is permanently designated the master.

## 3. Goals

RFC-BPR-0004 must provide:

1. deterministic file-level text diff results for any exact comparison identity and option set;
2. an immutable, ordered, monotonic semantic alignment covering both documents;
3. explicit line and boundary correspondence without fabricated exact matches;
4. deterministic change groups and stable identities for selection and navigation;
5. secondary monotonic line pairing within replacement regions;
6. intra-line ranges for paired replacement lines;
7. inputs for the lower full-width line-over-line detail pane;
8. a measured alignment-space axis for linked scrolling;
9. a single shared vertical scroll controller and scrollbar;
10. interval-to-interval center ribbons attached to the real document layout;
11. bounded computation, paint, and interaction cost;
12. direct core, host, Qt, and composed-workbench verification.

## 4. Non-goals

This RFC does not add:

- a three-way merge editor;
- conflict resolution or patch authoring;
- fake alignment rows in the top panes;
- a permanent graph of crossing moved-code edges;
- autonomous semantic code understanding;
- inline conversation cards inside the code panes;
- an independent-scroll mode; such a mode may be designed later but cannot weaken the default linked behavior;
- exact visual cloning of P4Merge assets, icons, colors, or proprietary implementation details.

The supplied screenshot and recording are behavioral references only.

## 5. Governing architecture

The governing rule remains:

> **The core decides. The host performs. Qt presents.**

### 5.1 Deterministic functional core

The core owns:

- comparison and file identity vocabulary;
- normalized diff options and their digest;
- the pure line-diff and secondary-pairing result contract;
- ordered alignment segments and their invariants;
- stable segment, line-pair, change-group, and move-relation identities;
- line and boundary mapping semantics;
- deterministic grouping and navigation policy;
- selected change and selected semantic line state;
- request generations and acceptance/rejection of asynchronous results;
- reason-coded diagnostics for invalid or unsupported comparisons.

The core has no Qt, Git process, GitHub, filesystem, clock, thread, future, or database dependency.

### 5.2 Application host

The host owns:

- acquiring exact left and right blobs for the selected epochs;
- binary and text materialization limits;
- normalizing immutable computation inputs;
- scheduling expensive diff computation;
- computation cache entries and invalidation;
- algorithm revision and option-digest cache keys;
- request context, cancellation, supersession, and stale completion handling;
- marshaling immutable completion events to the serialized application decision lane;
- diagnostic detail from Git, decoding, resource, or worker failures.

Workers never access widgets, Qt models, mutable core state, or live selection.

### 5.3 Qt presentation adapter

Qt owns:

- text-pane widgets and viewport layout;
- measured line-boundary and document-coordinate geometry;
- the derived measured alignment axis;
- the retained subpixel comparison-space scroll position;
- the shared vertical scrollbar and input translation;
- viewport-origin projection, end caps, clamping, and repaint scheduling;
- the center `DiffLinkMap` widget or splitter handle;
- ribbon paths, clipping, hover, selection hit testing, and divider resizing;
- visual theme, focus, accessibility, high-DPI behavior, and local scroll animation.

Measured pixels and the current scroll position are Qt-local state. They are not persisted as semantic review truth and do not enter the core.

## 6. Comparison identity and cache key

Every diff result is bound to an exact identity. A representative standard-library-only record is:

```cpp
struct file_comparison_key
{
    repository_id repository;
    pull_request_id pull_request;
    epoch_id left_epoch;
    epoch_id right_epoch;
    object_id left_blob;
    object_id right_blob;
    file_identity file;
    diff_options_digest options;
    algorithm_revision algorithm;
};
```

The live implementation may use different names, but the key must distinguish:

- repository and pull request;
- left and right epoch identities;
- exact left and right blob/object identities;
- stable file identity across rename when available;
- whitespace, line-ending, context, moved-code, and pairing options;
- the implementation algorithm revision.

A cached result for one key cannot satisfy another key by path coincidence or active-widget identity.

## 7. Semantic source coordinates

### 7.1 Half-open line spans

Line intervals use zero-based half-open coordinates:

```cpp
struct line_span
{
    std::uint32_t begin; // inclusive line boundary
    std::uint32_t end;   // exclusive line boundary
};
```

Empty spans are valid and essential:

```text
right-only insertion: left  [17, 17), right [17, 25)
left-only deletion:   left  [40, 46), right [40, 40)
```

A line span refers to logical source lines, not synthetic visual rows.

### 7.2 Line and boundary references

The model distinguishes a line from the boundary before or after it. Mapping an inserted line to the opposite side often yields a boundary, not a line.

Representative vocabulary:

```cpp
enum class diff_side { left, right };

struct source_line_ref
{
    file_comparison_key comparison;
    diff_side side;
    std::uint32_t line;
};

struct source_boundary_ref
{
    file_comparison_key comparison;
    diff_side side;
    std::uint32_t boundary;
};
```

User-visible line numbers may be one-based, but internal mapping remains unambiguous and zero-based.

## 8. Ordered alignment model

### 8.1 Alignment segments

The semantic result contains an ordered sequence covering both documents:

```cpp
enum class alignment_kind
{
    equal,
    replacement,
    left_only,
    right_only
};

struct alignment_segment
{
    alignment_segment_id id;
    alignment_kind kind;
    line_span left;
    line_span right;
    std::optional<change_group_id> group; // absent for equal segments
};
```

Moved text is represented in the monotonic path as a deletion plus insertion, with an additional move relation described below. It does not violate the ordered path.

### 8.2 Coverage invariants

For a valid text comparison:

1. Segments are ordered by both left and right boundaries.
2. Concatenating all left spans covers `[0, left_line_count)` exactly; empty spans are permitted but nonempty spans do not overlap or leave gaps.
3. Concatenating all right spans covers `[0, right_line_count)` exactly under the same rule.
4. No segment has both sides empty.
5. `equal` segments have equal line counts and exact normalized-line equality under the selected options.
6. `left_only` has a nonempty left span and empty right span.
7. `right_only` has an empty left span and nonempty right span.
8. `replacement` has both sides nonempty and is refined by any accepted internal anchors.
9. Adjacent segments are canonicalized so the same exact inputs and options produce the same ordered result and stable IDs.
10. Ordinary center ribbons derived from the ordered path never cross.

The implementation must provide a validator used by tests and debug evidence.

### 8.3 Stable identities

For identical comparison inputs, options, and algorithm revision, stable identities must repeat deterministically.

Identity may be derived from comparison identity, segment kind, canonical spans, and canonical ordinal. It must not depend on pointer addresses, worker completion order, Qt indexes, current scroll position, or incidental allocation order.

Stable identities are required for:

- selected change preservation;
- previous/next navigation;
- incremental Qt projection;
- hover and hit-region continuity;
- detail-pane selection;
- deterministic evidence and replay.

## 9. Diff and refinement pipeline

The specific line-diff algorithm is not a public product contract. An existing deterministic implementation may remain if it satisfies this RFC. The normative pipeline is:

1. Decode exact left and right text with explicit failure status.
2. Split logical lines while retaining final-newline and line-ending metadata.
3. Apply the selected matching normalization without destroying original display text.
4. Compute an ordered primary line diff.
5. Canonicalize primary operations into alignment segments.
6. Within replacement regions, perform a deterministic monotonic secondary line pairing.
7. Split large replacement regions at accepted exact or similar anchors so scrolling follows internal structure rather than treating the entire hunk as one coarse ratio.
8. Compute intra-line changed ranges only for paired replacement lines.
9. Build deterministic change groups and navigation order.
10. Validate coverage, monotonicity, identity, and bounds before publishing the result.

Whitespace and line-ending policy must be explicit in `diff_options`. Display text remains faithful to the selected blobs even when matching ignores a category of difference.

### 9.1 Secondary pairing

Secondary pairing must be monotonic. It may use deterministic similarity scoring, but it cannot pair lines in a way that causes the primary correspondence path to cross.

The pairer must:

- prefer exact normalized matches when available;
- use deterministic tie breaking;
- reject weak pairings rather than inventing certainty;
- expose whether a pair is exact or similar;
- keep unpaired lines explicit;
- split the parent replacement into finer alignment segments around accepted anchors.

### 9.2 Intra-line ranges

Intra-line output is attached to an accepted line pair. Ranges use offsets into the immutable original line text and must end on valid text boundaries.

Intra-line computation cannot create a line pair that the secondary pairing step rejected.

### 9.3 Move relations

Optional moved-code detection produces metadata such as:

```cpp
struct move_relation
{
    move_relation_id id;
    alignment_segment_id source_deletion;
    alignment_segment_id destination_insertion;
};
```

The monotonic alignment path remains deletion plus insertion. The default link map does not permanently draw a nonmonotonic ribbon between them. A move marker and selected/hover overlay may reveal the relation without turning the center divider into a crossing-edge graph.

## 10. Mapping semantics

Mapping is not always a function from one exact line to another. The API must report the truth.

Representative result kinds:

```cpp
enum class mapping_kind
{
    exact_line,
    similar_line,
    opposing_span,
    opposing_boundary,
    moved_relation,
    unavailable
};
```

Rules:

- An equal line maps to its exact counterpart.
- An accepted secondary pair maps as `similar_line` or `exact_line` as appropriate.
- An unpaired line in a replacement maps to the opposing replacement span, not an arbitrary guessed line.
- A line in a one-sided insertion/deletion maps to the opposite boundary.
- A moved line may expose a separate move relation while retaining its monotonic deletion/insertion mapping.
- Binary, unavailable, or invalid results map as `unavailable` with a reason code.

The lower line-over-line detail pane must display mapping confidence honestly. When no opposing line exists, it shows an explicit empty side or boundary state rather than pretending that a neighboring line is the counterpart.

## 11. Change grouping and navigation

A `change_group` is a deterministic navigation and presentation unit built from contiguous non-equal alignment segments under the selected context/grouping policy.

Each group contains:

- stable group ID;
- ordered member segment IDs;
- left and right covering spans;
- first and last comparison boundaries;
- summary kind and counts;
- any move-relation markers;
- deterministic previous/next order.

Grouping cannot be recomputed independently in Qt. The same groups drive:

- top-toolbar previous/next change;
- selected change highlighting;
- center-ribbon selection;
- lower-detail positioning;
- review receipt/actionability integration in later RFCs;
- test evidence.

Navigation to a group sets semantic selection in the core. Qt then chooses the comparison-space position that places the group's preferred anchor on the synchronization rail.

## 12. Natural document layout

The top panes display each source document in its natural logical order.

Normative rules:

- Do not insert placeholder or blank rows to force line alignment.
- Do not mutate source line numbers to match the other pane.
- Do not make synthetic rows addressable as source lines.
- Folding, if later supported, is a presentation projection over real source ranges and must invalidate measured geometry explicitly.
- Word wrapping is off by default for code comparison. If enabled later, the measured visual extents, not logical-line count alone, govern alignment-space scrolling.

The center link map and shared scroll projection explain displacement instead of changing the documents.

## 13. Measured alignment-space axis

### 13.1 Semantic versus measured state

The core publishes semantic alignment segments in logical line coordinates. Qt measures the current visual document coordinate for every required line boundary.

A measured segment contains:

```cpp
struct measured_alignment_segment
{
    alignment_segment_id id;
    qreal left_begin_y;
    qreal left_end_y;
    qreal right_begin_y;
    qreal right_end_y;
    qreal comparison_begin;
    qreal comparison_end;
};
```

No Qt type crosses into the core; this record is illustrative of Qt-local state.

### 13.2 Segment extent

For each measured segment:

```text
left_extent       = left_end_y  - left_begin_y
right_extent      = right_end_y - right_begin_y
comparison_extent = max(left_extent, right_extent)
```

The comparison intervals are cumulative and ordered. If both measured extents are zero because a presentation feature hides the entire segment, it contributes zero presentation extent and is omitted from positive-length lookup while its semantic boundary remains available; projection must never divide by zero.

For a comparison position `q` inside one segment:

```text
t = (q - comparison_begin) / comparison_extent
left_document_y  = lerp(left_begin_y,  left_end_y,  t)
right_document_y = lerp(right_begin_y, right_end_y, t)
```

If one side is empty, its begin and end coordinates are equal, so its projection remains constant while the other side advances. No special follower correction is required.

If a replacement has unequal measured heights, each side advances at the rate necessary to reach its own endpoint at the same comparison boundary.

### 13.3 Fine-grained segments

The measured axis uses the finest accepted monotonic alignment segments. A replacement containing internal anchors must not remain one giant undifferentiated ratio when the semantic model can split it into:

```text
paired region
one-sided insertion
paired region
small replacement
one-sided deletion
paired region
```

This is what makes scroll motion follow actual matching blocks.

### 13.4 Presentation end caps

Qt may add presentation-only leading and trailing caps so:

- the first content is reachable without dead travel;
- the last content on either side is fully reachable;
- one side may pin at its legal end while remaining content on the other side completes;
- the scrollbar range contains no region where neither pane nor connector changes.

End caps do not alter semantic line mapping, groups, or ribbon identity.

## 14. Shared scroll controller

### 14.1 One retained comparison position

The linked comparison owns one retained subpixel position, conceptually `q`. It is the sole vertical-scroll authority while linked mode is active.

Both pane viewports and the shared scrollbar feed this controller:

```text
wheel / trackpad / keyboard / scrollbar action
                         ↓
              comparison-space delta or target
                         ↓
                    retained q
                         ↓
             measured-axis projection
              ↙                         ↘
      left document origin       right document origin
                         ↓
                 link-map repaint
```

The pane under the pointer is only the input receiver. It is not the semantic master.

### 14.2 Synchronization rail

Each viewport has a fixed horizontal synchronization rail. The initial presentation default is approximately `0.43 * viewport_height`, centralized as a Qt presentation constant and adjustable during polish without changing the semantic contract.

For each pane:

```text
document_origin_y = projected_document_y(q) - rail_y
```

The origin is then clamped to the pane's legal content range. Presentation end caps ensure clamping does not create dead scrollbar travel.

During a right-only insertion at the rail:

```text
left projection  = constant
right projection = advancing
```

The left document visibly pins while the inserted right block passes through. The reverse operation retraces the same coordinates.

### 14.3 Input translation

The Qt adapter must:

- consume vertical wheel events from both text viewports and the link map;
- use `pixelDelta()` for high-resolution trackpads when present;
- use `angleDelta()` plus platform style hints as the fallback;
- honor Qt's inversion and scroll-phase semantics;
- accumulate subpixel movement rather than rounding each event independently;
- coalesce redundant paints without dropping accumulated motion;
- route Page Up, Page Down, Home, End, and shared-scrollbar actions to the same controller;
- prevent recursive pane-scrollbar feedback;
- keep horizontal scrolling pane-local.

[RFC-BPR-0073](RFC-BPR-0073-smooth-diff-touchpad-panning.md) adds visible
within-line placement in split and unified source readers. Retained fractional
origins must reach native text, selection, hit testing, gutters, marks and ribbon
geometry together, including a partially clipped first row. Clearly horizontal
pixel motion discards minor vertical drift before scrollbar clamping, without
banking it; small intentional vertical motion, diagonal motion and deliberate
reorientation remain available. Forwarding preserves device identity and phases.

Up/Down remain source-line cursor navigation, including Shift selection. An
in-view cursor move preserves the fractional origin. A keyboard action that
actually moves the viewport aligns the receiving pane to a source-line boundary
through the shared controller; the other pane retains its normal projection.
Page destinations align, Home/End keep their exact endpoints, and subsequent
pixel input starts from that aligned origin. The amendment's acceptance is tracked
by RFC-BPR-0073 without replacing this RFC's historical verification.

Copying `verticalScrollBar()->value()` between panes is nonconforming.

### 14.4 Shared scrollbar

The default linked diff view exposes one vertical scrollbar for comparison space. Its thumb represents the comparison axis, not either document's raw pixel height.

The controller retains high-precision `q`; any integer `QScrollBar` mapping must avoid accumulated rounding drift. Updating the scrollbar from `q` is projection, not authority transfer.

Switching wheel input from left to right must not cause a handoff jump because both already address the same `q`.

### 14.5 Programmatic navigation

Selecting a change, mapped line, search result, comment anchor, or epoch-restored selection requests a semantic target. Qt resolves that target to an alignment-segment position and sets `q` so the target appears at or near the synchronization rail, subject to legal end clamping.

Programmatic navigation cannot bypass the shared controller by independently positioning the two panes.

## 15. Center correspondence link map

### 15.1 Component

The Qt adapter owns one custom-painted `DiffLinkMap` occupying the center divider between the top panes.

The initial implementation should prefer a wide custom `QSplitterHandle` when it fits the live workbench composition. A fixed-width center widget flanked by splitter handles is acceptable only when it preserves the same geometry, hit testing, resize behavior, and visual continuity. Widget class choice is not an excuse to weaken behavior.

An initial logical width of roughly 48–64 device-independent pixels is appropriate; the exact theme constant is not semantic API.

### 15.2 Interval ribbons

A changed segment or canonical changed run is drawn as a filled interval-to-interval ribbon. Given:

```text
left interval:  [left_top, left_bottom)
right interval: [right_top, right_bottom)
```

the painter creates:

- a cubic upper boundary from `left_top` to `right_top`;
- a cubic lower boundary from `right_bottom` back to `left_bottom`;
- closed pane-edge boundaries;
- a filled path clipped to the link-map widget.

An empty interval collapses one edge to a point, naturally producing an insertion/deletion wedge.

The default path should emerge horizontally from each pane edge. A control offset near 40–45 percent of divider width is an initial presentation value, not a semantic requirement.

### 15.3 Visual treatment

- Editor backgrounds retain add/delete/replace color identity.
- Normal ribbons use a subordinate neutral fill.
- Selection and hover may increase opacity and add a subtle outline or accent.
- Adjacent fills are painted without antialiasing cracks at shared boundaries.
- Ribbons meet pane edges without a one-pixel seam at supported scale factors.
- Moved-code relations use markers and selected/hover overlays rather than permanent crossing ribbons.

### 15.4 Interaction

The link map supports:

- hover highlighting of the corresponding group in both panes;
- click selection by stable group or segment ID;
- optional double-click or command action to center the selected change;
- splitter resize after the platform drag threshold;
- accessible nonvisual equivalents through change navigation and status text.

Hit regions are derived from visible painted paths. Do not create one child widget per ribbon.

### 15.5 Visibility and caching

The painter processes only visible or near-visible changed runs, found through ordered indexes and cached document-boundary geometry.

A wheel event may change pane origins and screen coordinates, but it must not recompute the semantic diff or rescan every segment in a large file.

## 16. Detail-pane integration

The lower full-width line-over-line pane consumes the same mapping model.

When a top line or ribbon is selected:

- exact and similar pairs display both lines with intra-line ranges;
- a one-sided line displays an explicit empty opposing side and boundary identity;
- an unpaired replacement line exposes the opposing span and the lack of an exact pair;
- a moved relation may offer a separate moved-from/moved-to action without changing primary alignment;
- the detail selection retains stable IDs across repaint, scroll, and incremental projection updates.

The detail pane cannot maintain a competing line-pair heuristic.

## 17. Asynchronous request path

A representative flow is:

```text
select_comparison_epochs_intent / select_file_intent
    ↓
core validates identity and increments diff generation
    ↓
compute_file_diff_request(request_context, comparison_key)
    ↓
host materializes blobs and schedules pure computation
    ↓
file_diff_completed_event(request_context, immutable_diff)
    ↓
core rejects stale generation or accepts and publishes deltas
    ↓
Qt rebuilds only affected document, alignment, group, action, and status projections
```

The request context must be sufficient to reject a completion after repository, PR, epoch, file, blob, options, or generation changes.

Cancellation is a normal outcome. A cancelled or stale result cannot replace the current comparison because its file path happens to match.

## 18. Diagnostics and unsupported states

The result surface must distinguish at least:

- text comparison available;
- binary file;
- missing left or right blob;
- decode failure;
- file exceeds configured materialization or compute limit;
- unsupported generated-file policy;
- cancelled;
- stale/superseded;
- invalid diff result or invariant failure;
- host computation failure.

Machine-readable reason identity is stable. Provider, Git, decoder, or Qt detail remains diagnostic text and does not become semantic control flow.

Binary and unavailable states do not create fake alignment segments.

## 19. Performance contract

Routine behavior scales with the active set plus changed/visible set.

Required properties:

- semantic diff computation occurs off the Qt UI thread for nontrivial inputs;
- scroll input does not recompute semantic diff, grouping, or line pairing;
- measured document-boundary geometry is cached and invalidated explicitly;
- visible ribbon candidates are found without scanning all file segments on every frame;
- no per-ribbon child widgets or general-purpose scene graph are required;
- high-frequency scroll processing retains subpixel state and coalesces repaint;
- stale results are rejected in constant time by request identity/generation;
- large-file evidence records processed segment counts, visible path counts, allocations or equivalent structural counters, and interaction latency.

A benchmark that merely calls a low-level math function without the composed controller is insufficient.

## 20. Geometry invalidation

The measured alignment axis and link-map geometry must be invalidated when any relevant presentation fact changes, including:

- accepted semantic diff result;
- file or epoch selection;
- font family, font size, zoom, line spacing, or tab-width geometry;
- device-pixel ratio or screen change;
- viewport resize or divider resize;
- line wrapping policy;
- fold state;
- any future variable-height inline presentation;
- document content or line-layout rebuild.

Simple vertical scrolling updates origins and visible-range selection; it does not require remeasuring unchanged document boundaries.

## 21. Threading and lifecycle

- Semantic diff workers receive immutable inputs.
- Workers never touch Qt objects or mutable core state.
- Measured geometry and painting occur on the Qt GUI thread.
- Closing a PR, changing files, changing epochs, or destroying the workbench cancels or supersedes pending work.
- Late completions are harmless and ignored deterministically.
- A destroyed pane cannot remain the target of an asynchronous callback.

## 22. Security and authority boundaries

This RFC is read-only with respect to GitHub. It does not publish comments, reviews, approvals, labels, branches, or commits.

Diff caches and evidence must not leak:

- credentials;
- authorization headers;
- secret-bearing Git URLs;
- private source text outside approved test fixtures or designated local evidence paths.

The GitHub pull request and selected Git objects remain authoritative for shared content. The alignment model is a derived local projection.

## 23. Downstream contracts

### RFC-BPR-0005

The workbench RFC must compose this linked diff view. It cannot reintroduce two independent vertical scrollbars, follower-scroll heuristics, or fake alignment rows.

### RFC-BPR-0006

Syntax and theme work may style panes, ribbons, selection, and detail ranges. It cannot change semantic alignment, mapping confidence, or scroll projection.

### Review-state and comment RFCs

Later receipt, comment, defect, and AI surfaces anchor to stable semantic file/side/line or change-group identities. They must not persist Qt y-coordinates, scrollbar values, or synthetic rows.

## 24. Acceptance criteria

RFC-BPR-0004 is complete only when all applicable criteria are directly proven through the live public path or closest deterministic composed harness. A static screenshot does not satisfy these visual and interaction criteria.

### Semantic diff and mapping

- [x] Identical inputs and options produce identical canonical segments, groups, mappings, and stable IDs.
- [x] Segment coverage and monotonicity validators pass for representative and generated cases.
- [x] Equal, replacement, left-only, and right-only regions are explicit.
- [x] Empty spans represent insertions/deletions without fake source lines.
- [x] Secondary pairing is deterministic and monotonic.
- [x] Weak/unpaired lines remain explicit rather than being falsely paired.
- [x] Intra-line ranges attach only to accepted pairs and remain in valid text bounds.
- [x] Move relations preserve a monotonic primary path.
- [x] Previous/next change uses the same deterministic groups displayed in the link map.
- [x] The detail pane reports exact, similar, span, boundary, and unavailable mapping honestly.

### Host execution

- [x] Diff work uses exact blob/epoch/file/options/algorithm identity.
- [x] A completion for an old repository, PR, epoch, file, options set, or generation is rejected.
- [x] Cancellation and shutdown cannot update destroyed Qt state.
- [x] Cache selection is directly proven rather than inferred from target names.
- [x] Workers do not include or touch Qt.

### Alignment-space scrolling

- [x] One retained comparison-space position drives both panes.
- [x] Wheel/trackpad input over either pane advances the same position with no handoff jump.
- [x] A pure right insertion pins the left pane at its boundary while the right content advances.
- [x] A pure left deletion pins the right pane while the left content advances.
- [x] Unequal replacements advance at proportional measured rates and meet at the same segment boundary.
- [x] Reversing the same input path retraces the same pane origins within defined numeric tolerance and without cumulative drift.
- [x] Beginning and end content on both sides are reachable with no dead scrollbar region.
- [x] Page, Home, End, change navigation, and shared-scrollbar drag use the same controller.
- [x] Horizontal scroll remains independently usable in each pane.

### Link-map presentation

- [x] Changed intervals render as filled cubic ribbons attached to actual pane line boundaries.
- [x] Insertions and deletions render as wedges through empty spans.
- [x] The top documents contain no synthetic alignment blanks.
- [x] Ribbons remain attached while scrolling, resizing, changing DPI, and changing font metrics.
- [x] Hover and click select stable semantic identities.
- [x] Move relationships do not create a permanent crossing tangle.
- [x] Painting is bounded to visible/near-visible runs and does not instantiate one widget per ribbon.
- [x] Splitter resizing and ribbon selection are both usable.

### Integration and evidence

- [x] The real BetterPullReview composition or closest deterministic rendered integration harness demonstrates the behavior in both scroll directions.
- [x] The lower detail pane, change navigation, selected line, and link map agree on identity.
- [x] Large-file evidence proves no full-file semantic recomputation per wheel event.
- [x] High-DPI and supported platform rendering checks pass.
- [x] Product, architecture, UX, agent, and verification documentation is current.
- [x] A conservative simplification pass is complete.
- [x] All live RFC-BPR-0004 issue cards are in the terminal done lane before Auto-Verified closeout.

## 25. Required verification families

At minimum:

1. pure core unit and property tests for coverage, monotonicity, identities, mapping, grouping, and reverse projection math;
2. host tests with deterministic manual executor for cancellation, reordered completions, stale generations, cache identity, failures, and shutdown;
3. Qt tests for event translation, shared controller authority, subpixel accumulation, scrollbar projection, geometry invalidation, ribbon hit testing, high DPI, and rendering;
4. composed integration tests with representative source fixtures;
5. a rendered/manual interaction check against the supplied behavioral recording;
6. structural performance evidence for large files and many changed segments;
7. dependency scans proving the core remains Qt-free and workers do not touch Qt;
8. final `git diff --check` and all live repository documentation/index validators.

## 26. Rollout and rollback

Rollout replaces the active RFC-BPR-0004 before implementation, then proceeds through tracked slices.

Rollback during implementation means returning to the last verified post-RFC-BPR-0003 repository state and removing only the RFC-BPR-0004 replacement/import commits and their tracked implementation slices. It does not mean reverting RFC-BPR-0003 or discarding unrelated user changes.

The old follower-scroll or fake-row design is not an acceptable product fallback. If the complete linked view cannot yet be proven, leave RFC-BPR-0004 open and report the narrower implemented state.
