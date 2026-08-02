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

### 2.1 Canonical segments

Canonicalization should coalesce compatible adjacent operations while retaining internal anchors needed by mapping and scrolling. Avoid two extremes:

- one segment per raw edit operation when adjacent operations express one coherent interval;
- one giant replacement hunk that erases useful equal/similar anchors.

### 2.2 Mapping result

Use a tagged result rather than sentinel line numbers:

```cpp
using mapped_target = std::variant<
    exact_line_target,
    similar_line_target,
    opposing_span_target,
    opposing_boundary_target,
    moved_target,
    unavailable_target>;
```

An insertion maps to a boundary. Returning the nearest line as though it were exact would contaminate detail, comments, and review receipts.

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

### 3.2 Cancellation

Cancellation may stop computation or merely suppress publication. Stale rejection remains mandatory even when cooperative cancellation is requested.

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

## 9. Incremental updates

A new accepted diff result invalidates semantic-derived Qt projection. Routine changes should be narrower:

- scrolling: update origins, visible candidate range, and paint;
- hover: update old/new hit regions and pane highlights;
- selection: update selected IDs, action state, detail, and relevant paint regions;
- divider resize: update ribbon x geometry and viewport metrics;
- font/zoom/DPI: remeasure line boundaries and rebuild measured axis;
- epoch/file change: invalidate old axis and wait for tagged result.

Do not reset unrelated drawers, file models, or conversations for a scroll event.

## 10. Complexity targets

Let `S` be semantic segment count and `V` visible changed segment count.

- Build semantic indexes: `O(S)` after diff computation.
- Build measured axis: `O(S)` when layout metrics change.
- Project one comparison position: `O(log S)` segment lookup plus constant interpolation.
- Select visible ribbons: `O(log S + V)` using ordered indexes.
- Paint: `O(V)` paths.
- Wheel event semantic work: no diff or group recomputation.

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
- stale completion changes the active comparison.
