# RFC-BPR-0028, RFC-BPR-0018, RFC-BPR-0026 and RFC-BPR-0006 attended session, v008

Status: **prepared; six decisions pending; no host running**.

The two v007 rejections asked again on the pull request the owner named
for them, and the four cards landed or drafted since the v007 hand-back,
on the bundle built at the head where
[BPR-0740](../../../issues/60-done/BPR-0740-the-layout-public-path-no-longer-opens-the-exact-public-review.md)
finished. Nothing here changes anything on GitHub: the pull request is
read signed out, and an anonymous read counts against GitHub's sixty
requests an hour from this machine.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `e4fdb0ea8e250f42da2bfa3a61c501fbdbb6ffd6d92d929912f6b25b44b2757f` as recorded in
  [session-identity.json](session-identity.json), tests disabled, signed
  with the local identity the preset carries, built from the source at
  35438244 (`cmake --preset app-release && cmake --build build/app-release`,
  log `build/logs/app-release-35438244.log`, `codesign --verify --deep
  --strict` clean). `src/app/src/github_application.cpp` is unchanged
  since the v007 bundle head 2cdc0e57, so the two composition digests
  pinned there did not move.
- What changed in the bundle since v007: the epoch line and the
  "Comparing" menu are one full-width row under the strip, above the
  drawers, in every modality (BPR-0737); the dark palette's identifier
  roles take the Visual Assist values from the owner's corrected reading,
  with a distinct macro role (BPR-0738; the appearance store moves to
  schema version 5); and the central splitter handles draw the drawer's
  edge rule and highlight under the pointer (BPR-0739). BPR-0734 is a
  design page, not a bundle change.
- Pull request: [NVIDIA/stdexec#1846](https://github.com/NVIDIA/stdexec/pull/1846),
  the owner's answer (mandate r9) for a public pull request with more than
  one update. The update count the epoch line shows on it is read on the
  bundle, not assumed.
- Review page: [offline-package/index.html](offline-package/index.html), or
  the live URL once `attended_review.py serve` is started. Each candidate
  offers the command that opens the build and the pull request link, both
  with Copy buttons. The page keeps your note, takes a pasted screenshot,
  lets you change or clear an answer, and has "Finish and hand back". The
  page lists the decided v006 and v007 candidates as decided; their
  screenshots are the PNG files in the `captures` directories beside
  [the v006 packet](../rfc-bpr-0028-attended-v006/operator-packet.md) and
  [the v007 packet](../rfc-bpr-0028-attended-v007/operator-packet.md).
- Session: `cbd48b91000b00f1e72e1ad62c7c384c`, candidate revision
  `app-release-e4fdb0ea-2026-09-11.4`, six candidates ready and protected
  at sequence 341. The v006 and v007 verdicts stay recorded against
  their own revisions; the two re-asked candidates carry the new revision
  and keep their v007 rejections in the session's history.
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`,
  milestone `attended-preparation-pack-head`.

## Starting the session

Not started. The host and the watch are armed only on the owner's say-so:

    python3 .claude/skills/attended-review/scripts/attended_review.py \
        --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve

then, from the coordinator, `wait --after 341` as a persistent background
watch. Or open the offline page above and import its export afterwards with
`attended_review.py import --file FILE`.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build. This
   build moves the appearance store to schema version 5 (the workbench
   settings store stays at 6 and the restoration store at `user_version`
   3); an older build opened afterwards refuses the appearance store
   rather than reading it, so go back to the older build only if you mean
   to.
2. Home appears at once. Paste the NVIDIA/stdexec#1846 link into the field
   and press Open or Return; signed out, the field opens the public pull
   request directly. This pull request is new to the catalogue on this
   Mac, so the first open acquires it.
3. Let the review history settle: the epoch line should carry the pull
   request's updates, more than one, with the two bracket handles.
4. For the handles (BPR-0646) drag either handle to another update and
   watch the comparison follow. For the row (BPR-0737) switch modality with
   the strip, toggle and drag the drawers, and watch the row hold its place.
5. For the seam (BPR-0739) switch to Read and rest the pointer on the seam
   just right of the code's scroll bar: the rule should light up and the
   resize cursor should stay; then drag the drawer.
6. For the dark palette and the identifier roles (BPR-0720, BPR-0738), use
   Dark appearance (Settings > Appearance, or System on a dark desktop) and
   open a header with macros and templates, one of stdexec's `include/`
   files, in Read or Review.
7. For the design round (BPR-0734) open
   `docs/design/RFC-BPR-0028-workbench-direction-v6.html#read-one-pane`
   in a browser and read the card's "Round output, 2026-09-11" section.
8. If the history fails, read
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`.

## What each candidate asks

Asked again from v007, each on the pull request the owner named:

- **BPR-0646, epoch handles.** The one criterion left: a handle on the
  epoch line changes the comparison. microsoft/terminal#20607 has one
  update, which is why v007 could not judge it.
- **BPR-0720, dark palette.** Asked again ("macros are still grey,
  functions arent orange"); the roles themselves are BPR-0738's candidate,
  judged there token by token.

New in this round:

- **BPR-0737, the epoch row.** The line and the Comparing menu are one row
  the full width of the window under the strip, holding their place
  through the four modalities and every drawer gesture; in v007 the line
  sat in the centre pane and moved with the drawers.
- **BPR-0738, identifier colours.** Functions `FF8000`, class and struct
  types `FFD700`, variables `BDB76B`, attributes `4EC9B0`, macros purple.
  The macro purple is `CE8AD4`, one step lighter than the reference's
  `BD63C5`: `BD63C5` reaches only 3.18 against the added fill under the
  wash and the dark profile holds a 4.5 contrast floor (`CE8AD4` clears it
  at 4.54); say whether the lighter purple is acceptable or the floor
  should yield. Two open questions ride the candidate: the include path
  (string-salmon in the reference, grey in the accepted BPR-0720
  direction) and whether the standard-library aliases' italic matters.
- **BPR-0739, the splitter seam.** The resize cursor over the handle
  between the code and the conversations drawer stays, and the seam is
  visible: a one-pixel rule on the drawer's edge of each handle, in the
  highlight colour while the pointer is over it. The highlight is a state
  the design asset does not draw; say if it should go.
- **BPR-0734, the one-pane Read design.** State 2b "Read, one pane" on
  workbench states v6 and the two amendment texts on the card, with three
  questions: block order (struck rows above, or p4merge's kept-first),
  where the choice lives (a property of Read per window, or of the window
  that Review inherits), and whether the Modalities amendment should name
  the one-pane gutter map as a new element.

## After the session

Import the exported decisions with `attended_review.py import --file FILE`
(the live page saves them itself). The integrator then finishes each card
from its own acceptance; a rejection becomes a bounded follow-up card.
After the verdicts: [BPR-0741](../../../issues/20-ready/BPR-0741-the-multi-window-public-path-course-still-waits-for-a-sign-in-stop.md)
(the multi-window course, red at HEAD on macOS), BPR-0729 rewritten from
the accepted state, then the Windows and Linux inventories on their
stations.
