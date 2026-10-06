---
schema: "implement-rfc.card.v1"
id: "BPR-1287"
title: "Give Read a clear empty state when no file is open"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner aside in attended session 8bf444584f9806487f06891db2f5af50 (BPR-1083 chip-menu-v001, decision 01876495, 2026-10-06T00:12Z)","reservation":"docs/issues/_reservations/BPR-1287--BPR-1289--621f5b91f3f1.json","related_cards":["BPR-1083","BPR-1214"],"owner_reference":"docs/design/owner-refs/BPR-1083-owner-ref-read-without-a-file.png","acceptance":["BPR-1287:no-blank-diff","BPR-1287:direction","BPR-1287:unchanged","BPR-1287:owner-look"]}
parked_on: "human-review"
acceptance: [{"criterion":"BPR-1287:no-blank-diff","outcome":"pass","revision":"163cad3b1","evidence":{"kind":"command","path":"docs/issues/30-in-progress/BPR-1287-give-read-a-clear-empty-state-when-no-file-is-open.md","command":"ctest --test-dir build/dev -R 'qt_workbench_window_scale|application_label_authoring_contract'","detail":"With a pull request open and nothing chosen, Read shows the idle page and neither pane nor the bare status (qt_workbench_window at three scales); in the composed application over conventional-comments-v1 the label-authoring course's first evidence bit (ReadEmptyDirected) checks the same, offscreen and on the frozen executable 7110dd47. macOS arm64 Debug."}},{"criterion":"BPR-1287:direction","outcome":"pass","revision":"163cad3b1","evidence":{"kind":"command","path":"docs/issues/30-in-progress/BPR-1287-give-read-a-clear-empty-state-when-no-file-is-open.md","command":"ctest --test-dir build/dev -R qt_workbench_window_scale","detail":"'No file open' with the file count and ⌥↓; Open first file dispatches Next file's navigate intent (the core enters the order at the first actionable file) and follows its enablement; Show files expands Files and focuses the tree; both keyboard-focusable and named; no actionable file says 'No files to read' without Open first file; tab and status line agree."}},{"criterion":"BPR-1287:unchanged","outcome":"pass","revision":"163cad3b1","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","command":"ctest --test-dir build/dev -j8","detail":"set_loading brings the panes back, clear() the direction, clear(reason) shows the reason; review details still open with nothing chosen; the idle page dispatches nothing but Next file. Whole dev suite 603 of 606 before the last two edits, the two of this work fixed and green, the third BPR-1236's."}},{"criterion":"BPR-1287:owner-look","outcome":"pending","revision":"163cad3b1","evidence":{"kind":"freeform","detail":"Asked as BPR-1287-read-empty-v001 in session c2a2442c5ce16c3fbb77598fc87360bb (state /Users/jaeger/.mask/attended-review/owner-review-20261005-bpr1083-authoring-v002, cursor 20), frozen root /Users/jaeger/bpr1083-authoring-review-v002.nvkpPZ, launch.py --no-file."}}]
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

## Implementation, 2026-10-05

Built in the same worktree as BPR-1083 round v002 (`claude/bpr1083-v002`),
right after it, because the owner's aside came in that session.

- **`diff_workbench`.** The panes share their splitter slot with an idle page,
  one shown at a time, so the splitter keeps its two children and the review
  details still open below either. The page shows while no comparison is
  chosen: from construction, and after `clear()` with no reason. Loading, a
  failure, an invalidation or a host reason (`clear(reason)`) show the panes
  and status as before. The page uses the interface font, not the code
  typography the panes inherit. `set_idle_direction` takes its title, detail
  and the two gestures from the composition.
- **`workbench_window`.** "No file open" with "Choose one of the N files in
  Files, or open the first one. ⌥↓ opens the next file."; Open first file is
  Next file's action (the core enters the order at the first actionable file
  when none is chosen) and is enabled exactly when it is; Show files expands the
  Files drawer when collapsed and focuses the tree. With no actionable file:
  "No files to read", no Open first file. The comparison tab says the title,
  and the status line says "No file is open. Choose one of the N actionable
  file(s)." instead of "The selected file is outside the N actionable file(s)."
  (which it said with nothing chosen).
- **Courses.** The label-authoring course opens Read with no file before its
  file and checks the idle page (`ReadEmptyDirected`, the first bit of
  `LABEL_AUTHORING_EVIDENCE`). The UIforETW selected-diff course accepted only
  visible panes before its first selection; it now also accepts the idle page
  there. `--attended-open-pull-request` opens the fixture pull request signed
  in and stops on its opening page, for the owner's look.
