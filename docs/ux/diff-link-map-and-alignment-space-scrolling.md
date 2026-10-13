# Diff Link Map and Alignment-Space Scrolling UX

This document defines the user-visible behavior for RFC-BPR-0004.

## 1. Layout

The top comparison region contains:

```text
left epoch/document pane | correspondence link map | right epoch/document pane | shared vertical scrollbar
```

The lower region remains the full-width line-over-line pane defined by the workbench design: Line details ([[BPR-1334]]).

The left and right top panes retain natural document layout. There are no synthetic blank rows inserted merely to align text.

## 2. Correspondence link map

The center divider displays filled neutral ribbons connecting corresponding changed intervals.

### 2.1 Visual grammar

- A replacement connects one nonempty interval to another.
- A right insertion narrows to the left insertion boundary and opens to the right interval.
- A left deletion opens from the left interval and narrows to the right boundary.
- Equal content normally has no filled ribbon; subtle boundaries may be shown only where needed for continuity or selection.
- Adjacent changed runs may merge visually when the semantic group remains recoverable.
- The panes, not the ribbons, carry the main add/delete color language.
- Selected or hovered correspondence becomes more prominent without obscuring code.

Ordinary ribbons derive their translucent fill from the live window-text role,
not from the chrome shade family. Selected ribbons retain the highlight fill
and add a window-text edge, so selection remains distinct while both states
stay visible on light and dark gutter surfaces. Rendered evidence samples the
actual path pixels against the dominant observed gutter pixel: an unselected
path must have at least 24 samples and at least half of its mapped samples at
the 3.00:1 non-text floor, with a median of at least 3.00:1. A selected path
must contribute at least 24 actual edge pixels at that floor and have a
different dominant path pixel from the same path when unselected.

### 2.2 Topology

Ordinary ribbons do not cross. A moved-code relationship is shown through paired markers and only an optional temporary overlay when selected or hovered.

### 2.3 Interaction

- Hover highlights the related changed interval in both panes.
- Click selects the semantic change and updates the lower detail pane.
- Double-click or the standard reveal command centers the selected change.
- A drag that exceeds the platform threshold resizes the split; a click does not accidentally resize it.
- Keyboard previous/next-change remains the accessible and precise alternative to pointer interaction.

## 3. Linked vertical scrolling

### 3.1 Shared authority

The comparison has one vertical scroll position. Wheel or trackpad input over the left pane, right pane, or center link map updates that same position.

The pointer location does not designate a permanent leader. Moving the pointer from one pane to the other and continuing to scroll causes no handoff jump.

### 3.2 Matching regions

Through equal-height matching content, the panes move together and corresponding lines remain horizontally close to the synchronization rail.

### 3.3 One-sided insertion or deletion

When a one-sided block reaches the synchronization rail:

- the side with no content at that correspondence boundary pins;
- the side containing the block continues to move;
- the center wedge visibly expands or contracts as the block passes;
- after the block completes, subsequent matching content resumes moving together.

Reversing scroll direction retraces the same behavior.

### 3.4 Unequal replacement

When both sides contain replacement text of different visual heights, both move, but the shorter interval advances more slowly. Both reach the end of the replacement at the same comparison position.

The user should perceive the documents as stitched along matching blocks rather than one pane chasing the other.

### 3.5 Synchronization rail

The default rail is in the upper-middle portion of the viewport, approximately 43 percent from the top. It need not be drawn permanently. A subtle marker may be exposed during diagnostics or accessibility inspection.

### 3.6 Shared scrollbar

The comparison exposes one vertical scrollbar at the outside edge of the diff region. Its thumb represents the shared comparison path.

The scrollbar must reach all content on both sides. When one document ends earlier, that pane may remain pinned while the other completes.

### 3.7 Input details

- Smooth trackpad deltas visibly retain within-line positions in split and
  unified source readers, including after a gesture stops. Native text,
  selection, hit testing, gutters, marks and connectors share the applied origin.
- Clearly horizontal pixel motion suppresses minor vertical drift before either
  scrollbar clamps. Discarded motion never reappears later. Small vertical-only
  input, diagonal panning and deliberate reorientation remain responsive.
- Up/Down move the source-line cursor one line at a time; Shift extends selection.
  In-view cursor moves retain fractional placement. Keyboard reveals place a
  clipped selected row flush with the nearest displayed edge through the existing
  authority, within one device pixel; subsequent reveals scroll one row per step. A fully visible
  target stays put, including at the bottom edge. Page Up/Down retain page-sized
  movement and line-aligned destinations; Home/End retain exact endpoints.
  Touchpad motion resumes smoothly from the applied origin. See [RFC-BPR-0073](../rfcs/RFC-BPR-0073-smooth-diff-touchpad-panning.md).
- Traditional wheel notches use platform scroll settings.
- Natural-scroll inversion follows Qt/platform semantics.
- Page Up, Page Down, Home, End, and scrollbar drag use the same comparison controller.
- Horizontal scrolling remains independent per pane because long-line position is not semantic correspondence.

## 4. Selection and detail

Selecting a source line or ribbon updates the lower pane from the same semantic mapping.

- Exact pair: both lines appear.
- Similar pair: both lines appear with intra-line changes and a non-exact state.
- One-sided line: the opposite side is explicitly empty at a boundary.
- Unpaired replacement line: the opposite replacement span is identified; no neighboring line is misrepresented as exact.
- Moved line: moved-from/moved-to relation is available separately.

## 5. Programmatic reveal

Previous/next change, file switch restoration, comment reveal, search reveal, and future defect reveal all set the shared comparison position. They do not independently set pane scrollbars.

The target should land near the synchronization rail unless beginning/end clamping prevents it.

## 6. Loading and invalidation

During epoch or file change:

- stale ribbons and mappings disappear or remain visibly disabled according to the workbench loading policy;
- the previous file's late diff result cannot flash into the new file;
- accepted content and geometry appear together without a transient mismatched link map;
- selection is preserved only when the stable semantic identity remains valid.

## 7. Theme and high DPI

- Link-map width and path geometry use device-independent coordinates.
- Painting is antialiased at supported scale factors.
- Ribbons meet pane edges without seams.
- Dark, light, and system themes preserve sufficient contrast.
- Light, Dark, High-contrast Light, and High-contrast Dark captures measure
  known selected and unselected paths from the composed map raster against
  the actual observed gutter background.
- Color is not the only indication of insertion, deletion, replacement, selection, or unavailable state.

These are product acceptance requirements. RFC-BPR-0004's deterministic
forced-palette renders prove palette inheritance and geometry only; a real
target-native System-theme transition on the full selected composition and
RFC-BPR-0005's presentation-polish pass retain their separate rendered gates.

## 8. UX failures that block closeout

- two visible independent vertical scrollbars in default linked mode;
- copying one scrollbar value to the other;
- follower jumps when a one-sided block enters or leaves view;
- inserted blank rows in the top documents;
- center lines that connect arbitrary points rather than intervals;
- permanent crossing move lines;
- selected detail that disagrees with the highlighted ribbon;
- smooth input reduced to coarse notches through repeated rounding;
- a screenshot-only demonstration without two-direction interaction evidence.
