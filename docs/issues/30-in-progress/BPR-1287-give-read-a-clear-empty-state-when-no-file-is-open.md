---
schema: "implement-rfc.card.v1"
id: "BPR-1287"
title: "Give Read a clear empty state when no file is open"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner aside in attended session 8bf444584f9806487f06891db2f5af50 (BPR-1083 chip-menu-v001, decision 01876495, 2026-10-06T00:12Z)","reservation":"docs/issues/_reservations/BPR-1287--BPR-1289--621f5b91f3f1.json","related_cards":["BPR-1083","BPR-1214"],"owner_reference":"docs/design/owner-refs/BPR-1083-owner-ref-read-without-a-file.png","acceptance":["BPR-1287:no-blank-diff","BPR-1287:direction","BPR-1287:unchanged","BPR-1287:owner-look"]}
---

# BPR-1287: Give Read a clear empty state when no file is open

## Origin

Rejecting `BPR-1083-chip-menu-v001` for another reason, the owner added, quoted
exactly:

> aside: this is what happens when you go to Read without a file open. We need
> to not show a blank diff here and instead have some clear direction to open
> a file.

Their screenshot, [`BPR-1083-owner-ref-read-without-a-file.png`](../../design/owner-refs/BPR-1083-owner-ref-read-without-a-file.png),
shows Read with a pull request open and no file chosen: two empty code panes
numbered `1`, the small status text "Select a file comparison" above the left
pane, and the same words cut off in the tab strip.

## Outcome

With a pull request open and no file comparison chosen, Read shows no empty
diff. The comparison area says plainly that no file is open and how to open
one, with the gesture to do it in the same place.

## Acceptance

- **BPR-1287:no-blank-diff.** With a pull request open and no file chosen,
  Read shows neither empty code pane, nor a line number, nor the bare "Select a
  file comparison" status; a rendered check of the comparison area fails
  against the old surface.
- **BPR-1287:direction.** In their place, an empty state names the situation
  and the way out: choose a file in Files, or open the first file. Its
  controls do what they say through the existing gestures (the files drawer's
  own selection and Next file), can be reached by keyboard, and have
  accessible names. A pull request with no actionable file says that instead.
- **BPR-1287:unchanged.** Choosing a file shows its comparison as before;
  drafts, the chosen comparison, read state and the drawers are unchanged, and
  the empty state causes no GitHub, Git or storage effect.
- **BPR-1287:owner-look.** The owner accepts the empty state in an attended
  session. Required, not self-certified.

## Boundaries

Presentation only, in the Qt adapter; the core decides nothing new. Review's
own empty comparison may share the surface if it has the same gap. macOS first;
Linux and Windows runs follow the usual station requests.
