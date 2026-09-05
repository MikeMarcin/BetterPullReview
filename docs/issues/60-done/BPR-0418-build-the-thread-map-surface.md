---
schema: "implement-rfc.card.v1"
id: "BPR-0418"
title: "Build the thread map surface"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0416","BPR-0417"]
---

# Build the thread map surface

The view the owner chose on 2026-08-21: a map of one thread's reply structure,
with the path to where you are visible, and one reply in focus. The accepted
mockup is
`docs/verification/reviews/conversation-view-mockups-v001/mockups/03-treeverse.svg`.

## What the owner asked for beyond the mockup

Verbatim, on the verdict:

> I prefer this view, it still needs to show the full branch that you clicked
> on, or at least up to the noce you selected. I think this could use some more
> iteration but that can come after experience.

So the mockup's highlighted root-to-focus path is the **floor**, not the design.
The full branch that was clicked must be visible; root-to-selected-node is the
minimum acceptable reading. And iteration after use is expected — this card
should ship a first version and learn, not hold open pursuing a final shape
nobody has used yet.

## Known before starting

- The map legends a per-node read/unread state that **nothing carries**.
  `github_conversation_projection` has no read flag, and the review ledger's
  unread notion is a per-conversation cursor, not a per-comment one. Either
  build that state or drop the distinction from the view; do not fake it.
- `RFC-BPR-0005:58` requires that comments "remain in the right drawer" and
  "never become inline full-width rows in the code view". A map surface is
  compatible with that; check any placement decision against it.
- Depth exists only once [[BPR-0416]] lands and [[BPR-0417]] supplies content.

## Verification

A rendered check is required. This is a visual and interaction claim, and a
passing signal test would not show that the branch is actually visible or that
the focused node is the one clicked.

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'github_conversation|workbench'
```

## Done — 2026-08-25

The drawer draws the thread now. `qt::github_thread_map`
(`src/adapters/qt/src/github_thread_map.cpp`) sits above the conversation tree
in `github_conversation_panel`, on a vertical splitter, reading the same
projection the tree reads.

### What the owner asked for, and what it does

> I prefer this view, it still needs to show the full branch that you clicked
> on, or at least up to the noce you selected.

Selecting a node highlights the **whole branch it belongs to**, in two weights.
The root-to-selection path is heaviest — the floor, drawn as
`03-treeverse.svg` draws it. Everything downstream is drawn lighter. Nodes
neither above nor below stay plain, which is the assertion that stops "show the
branch" from being satisfied by highlighting the tree.

Selecting also **scrolls the map to the selection** and keeps as much of the
path above it as fits. That half was missing until review found it, and it is
the difference between highlighting a branch and showing one.

### The read/unread legend is gone, not approximated

Build the state or drop the distinction, do not fake it. Nothing carries
per-comment read state — the ledger's unread notion is a per-conversation
cursor — and building it is a ledger schema change under RFC-BPR-0007. So the
distinction is absent, and `test_no_read_state_is_invented` fails if the
wording returns. It checks whole words: a thread's own anchored filename
contains "read", and a guard that could not tell `thread_map.cpp` from a read
receipt would be deleted by the first person it stopped.

### Three decisions worth recording

**Selection has one owner.** A press on a map node sets the drawer tree's
current index, and the code reveal, anchor marker, and defect surface keep
reading the selection from where they did before. The map holds no selection
authority, performs no effect, and adds no projection.

**Layout is pre-order by depth, and that is load-bearing.** Slots within a
depth are assigned in pre-order, so within one row every node of an earlier
subtree precedes every node of a later one — precisely the condition under
which no two edges cross.

**The map is always visible rather than revealed on demand.** A second view
nobody finds teaches nothing, and this is explicitly a first version. The
splitter collapses it. If use shows it is noise on shallow threads, that is the
iteration the owner already authorised.

### What the independent review found, and it was a lot

An `rfc-reviewer` pass on the uncommitted slice returned **four blocking
findings**, all reproduced and all repaired before this commit. Three of them
were tests that passed while the thing they named was false, which is the
failure mode this board cares about most. They are recorded in full because the
first version of this card claimed evidence it did not have.

**The map never scrolled to the selection.** `set_focus_identity` recomputed
relations and repainted; only a press revealed. So a drawer selection moved the
map's state and not its view, and the reviewer measured the consequence: at
1400x900 the composed map viewport was 61 px, the focused node sat at content y
188, and **2 of 7** branch nodes were on screen. Every assertion still passed,
because they all asked about state. Repaired with `reveal_branch`, called on
every focus change *and* on resize — the drawer settles its layout after the
projection lands, so a reveal computed against the earlier viewport was undone
before anyone looked. The band's minimum also went from two rows to four.
`test_selecting_in_the_drawer_moves_the_map` now requires the selected node and
its parent to be inside the visible content rect.

**`test_no_two_edges_cross` compared nothing.** Two edges can only cross if
their parents share a depth, and in `deep-thread-v1` every edge-owning parent
sits at its own depth — the assertion was reached **zero** times. Worse, it
compared `Slot` to `Slot`, and nothing tied `Slot` to `Bounds`: the reviewer
linked a `place_nodes` that mirrored each depth's x order while leaving slots in
pre-order, and the suite still passed. Repaired three ways — a locally
constructed forked shape so two parents do share a depth, assertions on
`Bounds` rather than `Slot`, an explicit `Slot`-implies-x-order check, and a
count that fails if no pair was ever compared.

**The raster row proved only that the focused box repaints.** `focused !=
unfocused` is satisfied by one node changing fill. The reviewer erased the
entire two-weight branch highlight — every edge and every ancestor and
descendant painting as unrelated — and all ten rows still passed. Repaired with
region comparisons: an ancestor's pixels must change, the sliver an on-path
edge enters through must change, and an off-branch node and an off-path edge
must be **byte-identical** between the two rasters.

**The map answered a cycle differently from the drawer's own tree.** The
promotion pass wrote `Parent = npos` while later iterations still read it, so
only the first-indexed member of a cycle was promoted;
`github_conversation_model::apply` promotes all of them in a genuinely
read-only pass, and its own comment rejects breaking a cycle in place as the
order-dependent alternative. Measured through the shipped panel: 3 root rows in
the tree, 2 on the map. The card's comment claiming a read-only pass was
false. Repaired, and the test now compares the map's root count against
`github_conversation_model`'s on the same projection.

Two non-blocking findings were also taken: an unchanged projection is now
skipped rather than relaid (the drawer re-adopts on every anchor change), and a
press on the already-focused node re-reveals and re-reports instead of
returning early.

### Verification

Content is the constructed `deep-thread-v1` fixture from [[BPR-0417]], read
from its JSON rather than restated, plus one locally constructed forked shape
that the fixture cannot supply. Both are Qt layout inputs. This card claims only
that the view renders real reply structure — not that any recorded pull request
carried one.

| claim | command | proves it | result |
|---|---|---|---|
| every projected node is mapped, one edge per non-root | `ctest --preset dev -R 'qt_github_thread_map'` | counts against the fixture's own size | pass, 3/3 scales |
| the summary states the thread's real size | same | exact string `8 comments · depth 5` | pass |
| the clicked branch is complete, and nothing else is on it | same | root-to-selection set equality, plus four off-branch nodes asserted unrelated | pass |
| no two edges cross | same | on a shape where two parents share a depth: slot order implies x order, and parent x order implies child x order; fails if no pair was compared | pass |
| an unexpected parent costs nesting, never a row | same | both hostile cases keep every row, and the map's root count equals `github_conversation_model`'s | pass |
| the shipped drawer composes the map | same | published through `github_projection_port` on a real `qt::workbench_window`; the press point is asserted inside the visible viewport first | pass |
| the selected node is **visible**, not merely highlighted | same | the focused node and its parent inside the map's visible content rect after a drawer selection | pass |
| **the branch highlight is actually painted** | same | ancestor pixels and on-path edge pixels change; off-branch node and off-path edge pixels are identical | pass |
| an unchanged projection costs nothing | same | `Layouts` unchanged and `SkippedRebuilds` incremented on a repeat adopt | pass |
| the keyboard walks the tree | same | Home/Down/Right/Left/Up reach the exact expected identities; a move with nowhere to go is inert | pass |
| a screen reader is told where it is | same | the description carries size, who is in focus, the path, and the full anchored path the short box omits | pass |
| no read/unread claim | same | word-boundary match on both captions | pass |
| nothing adjacent regressed | `ctest --preset dev -R 'github_conversation\|workbench\|thread_map'` | panel, model property, three window scales, three diff-workbench scales, three public-path courses | 19/19 pass |
| formatting | `cmake --build --preset dev --target coding-format-check` | 425 files | pass |
| documentation | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK`; 107 documentation tests OK | pass |

### The tests fail when the claim is false

Every repair above was confirmed by replaying the exact defeat that beat the
first version:

- removing `reveal_branch` fails *"the node the reviewer selected is on screen
  in the map, not merely highlighted somewhere inside it"*;
- mirroring each depth's x order while leaving slots in pre-order fails *"a
  later slot at one depth is drawn further right"*;
- erasing the branch highlight while keeping the focused box fails *"a node
  between the root and the selection is repainted"* — the run that passed
  before;
- restoring the in-place cycle promotion fails *"the map promotes exactly the
  nodes the drawer's own tree promotes"*;
- suppressing branch relations entirely fails *"the clicked node is the one in
  focus"*.

### Simplification

Applied before review and re-verified after the repairs. `rebuild_layout` split
into `depth_first_order`, `place_nodes`, and `build_edges`; the two paint
switches became palette-derived `edge_pen` and `node_paint`, so `paintEvent`
draws rather than decides; the extent loop became one `QRectF` union. A
`sibling` extraction from `neighbour` was considered and declined — that
function has no loop and its one braced case reads fine.

### Deliberately left

The map announces its own focus change while the drawer tree does not re-announce
a restore; the reverse direction passes `aAnnounce = false` so one gesture does
not speak twice. The `githubConversationLoaded` splitter's sizes are not
persisted — RFC-BPR-0005 requires drawer widths to restore, not intra-drawer
splitters. The drawer's band stays short in an ordinary window; the map scrolls
to follow the selection rather than claiming a size it cannot have, and giving
it more room is exactly the iteration the owner authorised.

### The focused selector was not wide enough

Recorded here rather than only on the card that repaired it. The 19/19 above is
what `-R 'github_conversation|workbench|thread_map'` runs, and it is green. The
full `dev` matrix, run afterwards out of caution rather than obligation, found
`qt_composed_github_scale_{100,125,200}` red: this slice moved the drawer's
loaded stack page from the tree view to the splitter holding the map and the
tree, and that suite asserted the old identity. [[BPR-0453]] repaired it.

The selector was chosen by subsystem name, and `qt_composed_github` builds the
same window through the same drawer while matching none of those words. A
change inside a widget the shipped window composes has a blast radius the
widget's own name does not describe.

### Not done here

No per-comment read state. No whole-review conversation view — recorded *not
chosen*, not rejected. No revision axis; [[RFC-BPR-0021]] owns it. [[BPR-0211]]
closes when its slices do, and this was the last of them.
