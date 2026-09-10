# RFC-BPR-0028 and RFC-BPR-0018 attended session, v002

Status: **prepared; ten decisions pending**.

Nine parked cards wait on one run of the tests-disabled bundle built at the
pack head. Nothing here changes anything on GitHub: the pull request is
opened signed out, which reads and never writes.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `484b2072fef7f67cf6119e70478281ca02a40077cea4f64896e67e4438bba9c0`
  as recorded in [session-identity.json](session-identity.json), tests
  disabled, ad-hoc signed.
- Review page: [offline-package/index.html](offline-package/index.html), or
  ask for the live URL (`attended_review.py serve`). Each candidate offers
  the command that opens the build and the pull request link, both with
  Copy buttons. The page keeps your note, takes a pasted screenshot, lets
  you change or clear an answer, and has "Finish and hand back".
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`,
  milestone `attended-preparation-pack-head`.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build.
2. Stay signed out. On Home press "Read without signing in".
3. Copy the pull request link from the page (microsoft/terminal#20607),
   paste it into the bar on Home and press Open or Return.
4. Wait for the pull request, then let the review history settle.
5. Look at the window at its default size and again at about 1280x820.
6. If the history fails, read
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`.

## What each candidate asks

- **BPR-0646, workbench chrome.** The strip is the title row beside the
  traffic lights with the epoch line beneath and nothing else above the
  comparison; a handle on the epoch line changes the comparison; a rail
  carries a vertical label and an unread count; signed out, sign-in is
  reachable without scrolling. (Rejected on the 2026-09-07 build; the
  drawer toggles, Overview presets, epoch wording and comparison status have
  changed since.)
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
- **BPR-0675, drawer collapse.** Each drawer's own Hide and the strip toggle
  both collapse it, and the gesture reads as such without instruction.
- **BPR-0676, epoch line.** Nodes, span and amber tick are legible, and the
  three-word state is visible with its tooltip.
- **BPR-0683, Settings on macOS.** VoiceOver order, the four appearances at
  the real window, and the seven-category inventory of this tests-disabled
  bundle. Windows and Linux inventory stay pending on their stations.

## What changed since v001

The v001 session (bundle 884ad09b at 8ba0cb52) recorded one rejection on
BPR-0646 and was released for continued development. Since then: the
drawer toggles work without a resolved comparison and each drawer carries
Hide (BPR-0675); Overview collapses all three drawers (BPR-0674); the epoch
line names its state (BPR-0676); a comparison in preparation is not called a
failure (BPR-0677); the moved-base acquire is fixed (BPR-0667); Home takes a
pasted link through the setup destination (BPR-0669, BPR-0689); Settings
gained its remaining pages, scoped resets, export and migration preflight
(BPR-0678 to BPR-0688); and RFC-BPR-0024 profiles and windows landed
(BPR-0690 to BPR-0714). The bundle is built from the head that adopted the
pack plan.

## After the session

Import the exported decisions with `attended_review.py import --file FILE`
(the live page saves them itself). The integrator then finishes each card
from its own acceptance; a rejection becomes a bounded follow-up card.
