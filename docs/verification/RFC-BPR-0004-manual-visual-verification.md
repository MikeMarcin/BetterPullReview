# Manual and Rendered Visual Verification

The supplied P4Merge screenshot and recording are non-normative behavioral references. BetterPullReview must not ship them as product assets.

## Reference behaviors to inspect

### Center link map

The validated replacement pack supplies
`p4merge-block-correspondence-reference.png` as an external, non-product
reference. It is intentionally not copied into the repository; when available
from the approved pack source, inspect:

- continuous filled interval ribbons;
- neutral center treatment;
- ribbons meeting the changed line backgrounds at pane edges;
- widening/narrowing around unequal changed blocks;
- absence of synthetic blank alignment rows;
- visually monotonic noncrossing ordinary correspondence.

### Alignment-space scrolling

The validated replacement pack likewise supplies
`p4merge-alignment-space-scrolling-reference.mp4` outside the repository. When
available from that approved source, inspect:

- input initiated over one pane;
- a one-sided block advancing while the opposite pane pins at its correspondence boundary;
- matching content resuming together after the block;
- reverse movement retracing the same structure;
- input initiated over the other pane without a leader/follower handoff artifact.

The recording is not a pixel-perfect oracle. The semantic motion is the requirement.

## Required BetterPullReview evidence session

Run a synthetic redistributable comparison through the actual composed application or closest deterministic rendered integration harness.

Record or inspect:

1. application/version/build identity;
2. platform, display scale, font, zoom, and viewport size;
3. left/right exact fixture identity and options;
4. shared scrollbar visible;
5. wheel or trackpad input over the left pane;
6. right-only insertion pinning the left pane;
7. continuation into matching content;
8. reversal through the insertion;
9. continuing input over the right pane with no jump;
10. unequal replacement proportional motion;
11. ribbon click selecting the same group shown in top panes and lower detail;
12. divider resize;
13. font/zoom or scale change with paths still attached;
14. beginning/end reachability.

## Evidence annotations

Alongside video or deterministic frame captures, preserve a machine-readable trace containing samples of:

```text
comparison_position q
active measured segment ID
left projected document y
right projected document y
left applied origin
right applied origin
left/right clamp state
selected group/segment ID
visible ribbon count
geometry generation
semantic diff generation
```

The trace must show that the pinned side's projected coordinate remains constant through a one-sided segment and that semantic diff generation does not change during ordinary scrolling.

## Falsification questions

- Could the demonstration pass if two independent scrollbars were merely close most of the time?
- Could it pass if the opposite pane jumped only when the insertion left the viewport?
- Could fake blank rows make the motion look aligned?
- Could the link map be a prerecorded overlay unrelated to source line geometry?
- Could the lower detail use a different line-pair heuristic?
- Could stale diff data be visible after a file switch?

Add instrumentation or a stronger interaction whenever the answer is yes.
