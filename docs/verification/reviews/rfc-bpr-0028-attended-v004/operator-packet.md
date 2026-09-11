# RFC-BPR-0028 and RFC-BPR-0018 attended session, v004

Status: **prepared; thirteen decisions pending; no host running**.

The ten v003 candidates, prepared again on the bundle built at the head
that lands the three v003 follow-ups
([BPR-0719](../../../issues/60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md),
[BPR-0721](../../../issues/60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md),
[BPR-0722](../../../issues/60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md)),
plus one candidate for each of those three. Nothing here changes anything
on GitHub: the pull request is read signed out.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `9261509a91136ef9a564fa4f912e7f9441bdb6a68c81ecfd51d7d47cb7c19475`
  as recorded in [session-identity.json](session-identity.json), tests
  disabled, signed with the local identity the preset carries, built at
  3c9a3f1c (`cmake --preset app-release && cmake --build build/app-release`,
  `codesign --verify --deep --strict` clean).
- Review page: [offline-package/index.html](offline-package/index.html), or
  the live URL once `attended_review.py serve` is started. Each candidate
  offers the command that opens the build and the pull request link, both
  with Copy buttons. The page keeps your note, takes a pasted screenshot,
  lets you change or clear an answer, and has "Finish and hand back".
- Session: `cbd48b91000b00f1e72e1ad62c7c384c`, candidate revision
  `app-release-9261509a-2026-09-10.3`, prepared at sequence 182. The v003
  rejection of BPR-0646 (decision `65f3f881`) stays recorded against the
  v003 revision in the session's history; every v004 candidate is
  undecided and protected.
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`,
  milestone `attended-preparation-pack-head`.

## Starting the session

Not started. The host and the watch are armed only on the owner's say-so:

    python3 .claude/skills/attended-review/scripts/attended_review.py \
        --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve

then, from the coordinator, `wait --after 182` as a persistent background
watch. Or open the offline page above and import its export afterwards with
`attended_review.py import --file FILE`.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build. This
   build moves the workbench settings store to schema version 6; an older
   build opened afterwards refuses that store rather than reading it, so
   go back to the older build only if you mean to.
2. If a review opens on its own at launch, that is the restored review;
   otherwise stay signed out, press "Read without signing in" on Home, paste
   the pull request link (microsoft/terminal#20607) into the bar and press
   Open or Return.
3. Let the review history settle. A signed-out read counts against
   GitHub's anonymous limit of sixty requests an hour from this machine.
4. Look at the window at its default size and again at about 1280x820.
5. The drawers now show no rail and no Hide button by default. To judge the
   rails and the Hide buttons (BPR-0646's third criterion, BPR-0675, and
   the second half of BPR-0721) turn on Settings > Reviews & queue >
   Drawers > Drawer controls: Shown.
6. If the history fails, read
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`.

## What each candidate asks

The ten carried over are the v003 criteria, with two reworded for the
setting:

- **BPR-0646, workbench chrome.** The strip is the title row beside the
  traffic lights with the epoch line beneath and nothing else above the
  comparison; a handle on the epoch line changes the comparison; with Drawer
  controls Shown a rail carries a vertical label and an unread count, and
  with them Hidden a collapsed drawer leaves no column; signed out, sign-in
  is reachable without scrolling.
- **BPR-0662, drawer widths.** Usable widths after the clobbered layout, no
  drift on resize, a dragged width survives Home.
- **BPR-0663, public preview.** The epoch line shows the updates rather than
  "No updates loaded." and smart_acquire succeeds in the log.
- **BPR-0667, moved base branch.** The comparison resolves to an exact pair
  and protocol_acquire succeeds in the log.
- **BPR-0677, comparison readable.** While preparing, the status line says
  so and never advises a refresh; afterwards files open and read.
- **BPR-0674, Overview.** Two candidates: the headings are not elided with
  every drawer collapsed, and a choice of rule for a drawer opened by hand
  while Overview is active.
- **BPR-0675, drawer collapse.** With Drawer controls Shown, each drawer's
  own Hide and the strip toggle both collapse it, and the gesture reads as
  such without instruction.
- **BPR-0676, epoch line.** Nodes, span and amber tick are legible, and the
  three-word state is visible with its tooltip.
- **BPR-0683, Settings on macOS.** VoiceOver order, the four appearances at
  the real window, and the seven-category inventory of this tests-disabled
  bundle. Windows and Linux inventory stay pending on their stations.

The three new ones:

- **BPR-0719, large file.** `ScreenBufferTests.cpp` (+291) opens and shows
  its diff; the status line reads "Diff ready — N semantic changes", or the
  same with "(approximate alignment)". A comparison cached by an older
  build may still say "unavailable (cached)": Refresh once.
- **BPR-0721, drawer controls.** By default no rail and no Hide button, the
  strip toggles (Alt+Shift+1/2/3) open and collapse the drawers, and a
  collapsed drawer leaves no column; Shown brings the Hide buttons and the
  18-pixel rails back with nothing above a rail; both states acceptable
  with Hidden as the default.
- **BPR-0722, strip identity.** At the default size and at about 1280x820
  the strip names the open pull request (owner/repository and number at
  least) and nothing folds into an overflow menu.

## Open questions for the owner, not candidates

Carried from the v003 hand-back; answer in the note of whichever candidate
is nearest, or aside:

1. "Even in read mode viewing experience is not great": what specifically.
2. BPR-0720: does the whole dark syntax palette move to the Visual Assist
   scheme or only comments and includes, and is the saved custom palette
   reset or left.
3. Below about 960 pixels wide the strip has no room for the identity's
   number: which strip control should give way there, or should the
   window's floor move from 900 to 960.

## After the session

Import the exported decisions with `attended_review.py import --file FILE`
(the live page saves them itself). The integrator then finishes each card
from its own acceptance; a rejection becomes a bounded follow-up card.
