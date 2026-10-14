# Diff Correspondence and Alignment-Space Scroll Architecture

This document expands the implementation allocation in RFC-BPR-0004. The RFC is normative when wording differs.

## 1. One semantic path, several projections

The central design invariant is that BetterPullReview has one ordered semantic alignment path per text-file comparison.

```text
exact blobs + diff options
            ↓
     semantic alignment
        ├── line mapping
        ├── change grouping
        ├── navigation order
        ├── detail-pane pairing
        ├── link-map intervals
        └── measured scroll axis
```

Separate algorithms for these surfaces are prohibited because they can disagree. Presentation may aggregate adjacent intervals for visual clarity, but it must retain stable references to the semantic segments and groups it represents.

## 2. Functional-core model

A practical core shape is:

```cpp
struct file_diff
{
    file_comparison_key key;
    std::uint32_t left_line_count;
    std::uint32_t right_line_count;
    std::vector<alignment_segment> segments;
    std::vector<line_pair> pairs;
    std::vector<change_group> groups;
    std::vector<move_relation> moves;
    diff_diagnostic diagnostic;
};
```

The vectors are immutable after publication. Indexes may be built by the host or adapter, but they cannot change semantic meaning.

The live standard-library-only record boundary is declared in
`src/core/include/bpr/core/diff_model.hpp`. It binds every result to a complete
validated pull-request authority, matching repository/PR and anchor scopes,
optional added/deleted-file blobs, stable file, normalized option digest, and
algorithm-revision identity. The publication retains those normalized options
and validates their digest, so disabled pairing or move policy cannot publish
the corresponding records. Its public validator proves
structural coverage, bounds, kind/span consistency, group membership and
summary, monotonic secondary-pair order, one-to-one equal-extent exact-move
topology, and replay-stable
record IDs before publication.

The live pure computation boundary is declared in
`src/core/include/bpr/core/diff_compute.hpp`. It accepts borrowed immutable
byte spans whose presence must agree with the optional blob identities, copies
exact display bytes into the result, validates canonical UTF-8, and retains
content/terminator spans plus LF, CR, CRLF, and final-newline facts. Matching
normalization never changes those retained bytes. NUL input becomes a typed
binary result; malformed text and bounded materialization or matrix failures
publish diagnostics without partial semantic records.

The computation builds deterministic primary matches, monotonic accepted
secondary pairs, UTF-8-boundary intraline ranges, and optional unambiguous
exact move metadata. It assigns IDs only after refinement, constructs the
maximal contiguous changed-run groups required by the live structural model,
and invokes both the alignment and retained-document/intraline validators
before publishing. Retained-result validation also proves normalized equality
for every equal segment, accepted exact pair, and exact move relation, so a
stale or mutated document cannot preserve a false exact correspondence.

The live `diff_semantics` core boundary validates an immutable result once and
builds retained segment, group, pair, move, and intraline indexes for repeated
queries. It derives exact-line, similar-line, opposing-span, opposing-boundary,
and unavailable mapping results from that bound result, projects the canonical
group vector without regrouping, navigates directly over that vector, retains
segment/group identity in line and group selections, and builds lower-detail
inputs by re-resolving the canonical mapping. The overloads that accept an
unindexed result retain the fail-closed public boundary by validating and
building an index before answering. Separate exact move correspondence may
annotate a selected line, but never replaces its monotonic primary boundary
mapping.

Exact move discovery indexes normalized lines from one-sided segments, accepts
only anchors unique on both sides, extends each accepted anchor to its maximal
unclaimed exact run, and splits only the affected one-sided segments. Repeated
ambiguous content remains ordinary deletion plus insertion. The primary path
therefore remains monotonic even when optional sub-block relation metadata is
present. Discovery charges an explicit structural work budget for bounded
index probes, sorting, comparisons, fills, and refinement; exhaustion discards
all optional move metadata, while the independent segment ceiling is reported
separately. The counter is a deterministic upper-bound accounting unit, not a
CPU-instruction or wall-clock measurement.

A live `change_group` stores one half-open index interval into the immutable
ordered segment vector instead of allocating a second member-ID vector. The
group's ordered member IDs are exactly that segment slice; validation requires
every slice member to name the group and every changed segment to resolve to
one published slice. Same-kind adjacent segments are otherwise coalesced, but
an accepted secondary pair or move relation may retain a finer boundary whose
separate stable identity is semantically required.

### 2.1 Canonical segments

Canonicalization should coalesce compatible adjacent operations while retaining internal anchors needed by mapping and scrolling. Avoid two extremes:

- one segment per raw edit operation when adjacent operations express one coherent interval;
- one giant replacement hunk that erases useful equal/similar anchors.

### 2.2 Mapping result

Use a tagged result rather than sentinel line numbers. The live public shape
uses an enum plus optionals equivalent to:

```cpp
using mapped_target = std::variant<
    exact_line_target,
    similar_line_target,
    opposing_span_target,
    opposing_boundary_target,
    unavailable_target>;
```

An insertion maps to a boundary. Returning the nearest line as though it were exact would contaminate detail, comments, and review receipts.

Move correspondence is a separate optional relation from an exact deletion
line to an exact insertion line. This preserves delete-plus-insert primary
topology and prevents navigation, detail, or ordinary ribbons from acquiring a
crossing alternative mapping.

### 2.3 Group projection

Groups are stable semantic units. A visual ribbon may represent a whole group or a canonical subset, but selection must resolve back to stable IDs.

## 3. Host execution seam

The host schedules pure computation over immutable input:

```text
core request
    ↓
host resolves blobs and text facts
    ↓
worker invokes deterministic diff computation
    ↓
host publishes tagged completion
    ↓
core accepts or rejects by request context
```

A representative context:

```cpp
struct diff_request_context
{
    request_id request;
    session_id session;
    repository_id repository;
    pull_request_id pull_request;
    file_identity file;
    comparison_revision comparison;
    request_generation generation;
    diff_options_digest options;
};
```

The exact fields may differ, but every state transition that can make a completion irrelevant must be represented or captured by a revision/generation.

### 3.1 Cache

Cache immutable `file_diff` values by exact `file_comparison_key`. Cache values are derived and subordinate to Git objects and selected epochs.

A path-only key is invalid. A cache hit must not cross account/repository/PR/blob/options identity.

[[BPR-1327]] Comparisons prepared before the reviewer opens them live in this
cache under the same exact keys, unpublished, within a prepared share of it
(16 entries and 8 MiB by default). They use only room the reviewer's own
comparisons leave free and are evicted first; opening one is an ordinary cache
hit. The [system architecture](system-architecture.md#concurrency-and-stale-work)
describes the preparation window and its cancellation.

### 3.2 Cancellation

Cancellation may stop computation or merely suppress publication. Stale rejection remains mandatory even when cooperative cancellation is requested.

[[BPR-1366]] The engine stops cooperatively. `compute_file_diff` and
`compute_material_diff` have a stoppable overload that takes a
`diff_compute_control` (a caller's `diff_compute_stop` predicate and a check
interval). The engine reads no clock or thread primitive: it counts steps
(matrix cells, move-detection work, bytes scanned or normalized, weighted
validation records) and polls the predicate every 64 Ki of them, so a stop is
honoured within the interval plus the largest single increment.
`build_diff_semantic_index` has a stoppable overload too.

[[BPR-1485]] A stop abandons the computation (RFC-BPR-0082, the owner's
restart-only answer). Nothing is kept: a stopped run returns no outcome, and
the comparison is computed again from the start the next time it is wanted.
BPR-1366's paused states and resume points are gone.

[[BPR-1485]] The primary line matches come from the application's
`diff_line_matcher_port`, through the engine's component seam
(`diff_compute_components.hpp`); the engine has no primary matcher of its own,
and a comparison without the matcher's matches is refused as a host failure.
The product's matcher is Git 2.56.0's xdiff (Myers with the indent heuristic),
built as its own shared library and called only by `src/adapters/xdiff`. Under
any option other than exact comparison it is given each line already
normalized as the engine normalizes it, with no xdiff whitespace flag, and the
engine checks every match against its own line classes. xdiff cannot be
interrupted, so the host runs it on a thread of its own with its own copy of
both sides and polls the stop while it waits (every millisecond); a stop
returns at once and abandons that thread, which runs on detached and whose
result is discarded. The pairing, intraline and move budgets are unchanged and
may still make an alignment approximate.

### 3.3 Selected application and Git cache composition

The live GitHub composition constructs one `diff_application_host` with the
accepted `epoch_application_host` as its blob port and the production Qt
`diff_workbench` as its projection. The diff worker therefore never owns a Git
session or a widget pointer.

`epoch_application_host::materialize` is a synchronous worker-facing bridge
onto the existing repository cache actor. It finds the actor by repository
authority, queues the read on that actor's FIFO, and admits it only when the
complete `git_cache_open_request` equals the actor's already retained
lifetime-exclusive session. It does not open or switch a session for diff
work. A stale same-repository PR request consequently fails without disturbing
the live epoch/cache authority.

The public workbench supplies no blob guesses. Its bounded file rows retain an
opaque semantic file identity plus provider-free current path, optional
previous path, and change kind. Selection asks the epoch host to resolve the
current path independently at both exact selected commits through the retained
cache actor. A rename checks both exact path candidates at each endpoint and
refuses an endpoint where both exist; a copy still resolves only the current
path. If a rename candidate does not bind uniquely, the same retained session
runs one bounded NUL-delimited selected-pair rename query. The raw scan and the
retained candidate set have separate fixed caps, so unrelated renames do not
consume the smaller causal-candidate budget. Exactly one retained record must
match the accepted current/previous lineage and every already-bound endpoint;
this recovers intermediate names such as `a -> b -> c` without following
history or guessing. Multiple, malformed, or unbound candidate records fail
closed. Installed Git uses one literal, NUL-delimited `ls-tree` lookup, accepts
only regular, executable, or symlink blob entries, and returns absence as a
typed optional. One absent selected endpoint remains a valid diff input. Only
after the workbench authority, comparison, file facts, generation, normalized
options, digest, and algorithm still match does the host form
`file_comparison_key` and send it through the existing diff host.

The workbench replays its retained semantic selection after an accepted epoch
comparison becomes available or changes, so an early selection and a reverse
pair need no second Qt gesture. A changed workbench authority/generation first
retires selected work and publishes empty rows and cursor state; a failed
replacement observation cannot leave an old row selectable under new
authority.

The cache session validates its exact repository ownership before and after
calling installed Git. Installed Git first checks object identity, type, and
declared size with `cat-file --batch-check`; only a valid blob within the 16 MiB
caller ceiling is read, under an exact process-output limit. Empty blobs remain
available as an exact empty byte vector.

Accepted handoffs cancel the selected diff only when authority or semantic
revision facts that can change the selected anchors differ. A metadata-only
refresh with the same authority, identity generation, origin, base branch,
base/head objects, and current-viewer review candidate preserves the exact
result and semantic selection. Comparison-selection changes cancel the
selected diff.
Whole-cache deletion invalidates a matching diff authority before maintenance.
Shutdown closes the diff host before epoch blob ingress, waits for diff, epoch,
GitHub, and cache-actor quiescence, drains diff effects before epoch effects,
and destroys the Qt window later by visible composition-root member order.

## 4. Qt pane measurement seam

The Qt adapter needs a narrow pane geometry surface independent of the specific editor widget:

```cpp
class diff_pane_geometry
{
public:
    virtual qreal document_y_for_line_boundary(std::uint32_t boundary) const = 0;
    virtual qreal document_height() const = 0;
    virtual qreal viewport_height() const = 0;
    virtual qreal document_origin_y() const = 0;
    virtual void set_document_origin_y(qreal origin) = 0;
};
```

This is illustrative, not a required virtual interface. Direct typed composition is preferable when a runtime abstraction is unnecessary.

A `QPlainTextEdit` implementation can measure `QTextBlock` geometry. A custom virtualized code pane can expose equivalent line-boundary facts. The alignment controller depends on the facts, not on fake cross-platform widget abstractions.

## 5. Measured axis construction

For each semantic segment:

1. measure left begin/end line boundaries;
2. measure right begin/end line boundaries;
3. assert finite, ordered values;
4. compute left and right extents;
5. assign comparison extent as their maximum;
6. append to a cumulative ordered array;
7. build binary-search indexes for comparison position and visible document ranges.

Empty spans produce equal begin/end coordinates. If both sides measure to zero, retain the semantic boundary but omit that segment from positive-length comparison lookup or coalesce it with the adjacent boundary; never manufacture scroll extent or divide by zero.

```cpp
struct measured_segment
{
    alignment_segment_id id;
    double comparison_begin;
    double comparison_end;
    double left_begin;
    double left_end;
    double right_begin;
    double right_end;
};
```

Use `double` or `qreal` consistently in the Qt region. Do not round to integer scrollbars during internal projection.

## 6. Projection mathematics

For segment `s` and comparison position `q`:

```cpp
const auto extent = s.comparison_end - s.comparison_begin;
const auto t = extent > 0.0
    ? std::clamp((q - s.comparison_begin) / extent, 0.0, 1.0)
    : 0.0;

const auto left_y = std::lerp(s.left_begin, s.left_end, t);
const auto right_y = std::lerp(s.right_begin, s.right_end, t);
```

The function is monotonic and continuous at canonical segment boundaries.

### 6.1 Right-only insertion

```text
left_begin == left_end
right_begin < right_end
```

`left_y` is constant; `right_y` advances.

### 6.2 Left-only deletion

```text
left_begin < left_end
right_begin == right_end
```

`left_y` advances; `right_y` is constant.

### 6.3 Unequal replacement

Both advance, with velocity proportional to their extent divided by the common maximum extent.

### 6.4 Reverse stability

Projection is derived from retained `q`, not accumulated pane origins. Reversing `q` therefore recomputes origins from the same canonical basis and avoids drift.

## 7. Comparison scroll controller

The controller owns:

- retained high-precision `q`;
- legal comparison range and presentation end caps;
- references to current measured axis and pane geometry;
- synchronization-rail policy;
- translation from user input to comparison delta/target;
- feedback suppression while applying pane origins;
- shared-scrollbar projection;
- per-frame repaint coalescing.

It does not own:

- semantic selection policy;
- blob or diff computation;
- GitHub state;
- durable settings beyond ordinary appearance/input preferences;
- independent semantic mapping.

### 7.1 Input receiver versus authority

The widget under the pointer only identifies where the wheel event arrived. The event is converted to a delta in comparison space and consumed. Normal independent vertical scrolling is suppressed in linked mode.

### 7.2 Shared scrollbar adapter

`QScrollBar` exposes integers while the controller retains subpixel state. Use a stable scale or normalized mapping with residual-free projection. Never update `q` by repeatedly reading back a rounded pane scrollbar value.

### 7.3 Programmatic target

To reveal a semantic target:

1. find its segment/group anchor in the measured axis;
2. compute the desired `q` that places it on the rail;
3. clamp to legal range;
4. project both pane origins in one guarded update;
5. update the shared scrollbar and repaint.

## 8. DiffLinkMap composition

Preferred Qt structure:

```text
DiffComparisonWidget
├── left code pane
├── DiffLinkMap divider (wide custom splitter handle preferred)
├── right code pane
└── one shared vertical scrollbar
```

The center component is one custom painter. It receives:

- immutable semantic IDs and kinds;
- cached document-boundary coordinates;
- current left/right origins;
- visible-range index results;
- selected and hovered IDs;
- theme values.

It does not call the diff engine or choose group semantics.

### 8.1 Ribbon geometry

For each visible changed interval:

```cpp
QPainterPath ribbon;
ribbon.moveTo(x_left, left_top);
ribbon.cubicTo(
    x_left + c, left_top,
    x_right - c, right_top,
    x_right, right_top);
ribbon.lineTo(x_right, right_bottom);
ribbon.cubicTo(
    x_right - c, right_bottom,
    x_left + c, left_bottom,
    x_left, left_bottom);
ribbon.closeSubpath();
```

Use actual `QPointF` overloads in implementation. Slightly overlap pane edges to avoid seams. Clip to the divider.

### 8.2 Aggregation

Adjacent segments may be visually aggregated when:

- aggregation preserves topology;
- hit testing can still resolve to group/segment identity;
- selection and detail remain deterministic;
- insertion/deletion wedge structure is not obscured.

### 8.3 Move overlays

A move relation may be represented by matching glyphs or a temporary overlay outside the ordinary monotonic ribbon set. Ordinary scroll ribbons remain uncrossed.

Pane backgrounds have a deliberate compositing order: whole-line diff band,
translucent selected-row wash, then opaque intraline span. Metadata may still
contain all three selections when the order is wrong, so the public capture
counts pixels equal to the exact semantic-role colour inside each bound span.
A generic “different from the pane base” count is not evidence of intraline
paint; text glyphs and the selection wash satisfy that weaker predicate.

## 9. Incremental updates

A new accepted diff result invalidates semantic-derived Qt projection. Routine changes should be narrower:

- scrolling: update origins, visible candidate range, and paint;
- hover: update old/new hit regions and pane highlights;
- selection: update selected IDs, action state, detail, and relevant paint regions;
- divider resize: update ribbon x geometry and viewport metrics;
- font/zoom/DPI: remeasure line boundaries and rebuild measured axis;
- epoch/file change: invalidate old axis and wait for tagged result.

Do not reset unrelated drawers, file models, or conversations for a scroll event.

The live `diff_workbench` accepts a publication only for its exact current
loading context. Failure, invalidation, revision/file/options replacement, and
clear remove all measured geometry and close or replace that context before a
late publication can be considered. It retains one semantic index beside the
accepted immutable result and routes selection, navigation, mapping, and
detail through that index. Compact line/group/navigation selection is reduced
by `diff_interaction` in the functional core; Qt only renders the accepted
stable IDs and remeasures pixel geometry after explicit layout invalidation.
Ordinary frames reuse the cached axis extents and do not walk either pane's
text blocks.

An unwrapped pane measures each block in a transient layout that keeps no
shaped text ([[BPR-1032]]), so each source pane keeps the block heights it
found ([[BPR-1123]]). A later measurement, after a divider, viewport, style or
DPI event, takes a block's height from that record while the document and its
revision, the fonts, the device pixel ratio, the margin, the tab stop and the
layout properties of the block's own formats are unchanged, and shapes only
the blocks that changed. Recolouring a block does not change its layout, so
it is reused. Each block costs 16 bytes ([[BPR-1142]]): a 64-bit key over its
formats' ranges and layout properties, and its height and natural width in
Qt's 1/64 px layout units, which hold every value a layout produces exactly.
A block the record cannot hold, or one measured natively, carries a key no
format list hashes to and is measured again. Replacing a pane's text empties
its record and frees its storage.

## 10. Complexity targets

Let `B` be retained source bytes, `R` the total retained semantic record count
(segments, groups, pairs, moves, and intraline records), `S` the semantic
segment count, and `V` the visible or near-visible changed segment count.

- Validate and build semantic indexes: `O(B + R log R)` once per accepted
  immutable publication; retained query lookup is logarithmic in its indexed
  record set.
- Build measured axis: `O(S)` when layout metrics change.
- Project one comparison position: `O(log S)` segment lookup plus constant interpolation.
- Select visible ribbons: `O(log S + V)` using ordered indexes.
- Paint: `O(V)` paths.
- Selection/navigation: indexed segment, pair, group, and move resolution plus
  only the affected group/pair records.
- Wheel event semantic work: no diff, semantic-index, group, pane-measurement,
  or unchanged-axis recomputation.

The implementation may optimize further, but it cannot regress routine wheel/paint work to an unconditional full-file scan.

## 11. Falsifying tests

Architecture tests should fail if:

- core headers include Qt;
- a worker retains a widget pointer;
- pane scrollbar values become the shared authority;
- a right-only segment moves both projected document coordinates;
- reverse projection accumulates origin error;
- a mapping API returns a neighboring exact line for an insertion;
- ordinary move ribbons cross;
- painting visits all `S` segments for a viewport containing `V << S` changes;
- stale completion changes the active comparison;
- a focused-review capture crops away visible chrome instead of hiding it
  through shipped controls, or the two panes and center map cease to dominate
  the retained full-window viewport;
- either named UIforETW replacement span is outside its pane, has no exact-role
  pixels, or its selected ribbon fails to terminate at both pane edges while
  vertically overlapping the corresponding visible rows.

## Semantic source-line comment gesture

RFC-BPR-0022 exposes the ordinary comment gesture on every materialized
current/right text line in the selected changed file, including unchanged
lines outside a painted hunk. Exact retained endpoint, path, blob, line and
current-head admission remain application-host responsibilities. The provider's
bounded patch observation determines legacy/off-hunk/unknown placement; missing
or truncated provider context stays unknown. Qt geometry is never evidence of
what GitHub accepts.

Accepted compatibility threads carry a separate semantic anchor while keeping
the honest remote FILE subject. Selection in either direction validates the
exact comparison before revealing a line, and canonical GitHub IDs drive
incremental changes. See [review round trip](review-round-trip.md#file-subjects-and-selected-source-placement-rfc-bpr-0022).
