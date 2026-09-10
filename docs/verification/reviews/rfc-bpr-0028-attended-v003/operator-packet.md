# RFC-BPR-0028 and RFC-BPR-0018 attended session, v003

Status: **prepared; ten decisions pending**.

The same ten candidates as v002, prepared again on the bundle built at the
head that fixes [BPR-0715](../../../issues/40-parked/BPR-0715-opening-a-pull-request-signed-out-no-longer-reaches-a-comparison.md):
opening a pull request signed out reaches its files and an exact comparison
again. Nothing here changes anything on GitHub: the pull request is opened
signed out, which reads and never writes.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `9334b13ad4880eab414724ff8179a862f00d4a7406bfc4d1da3211ec15a50525`
  as recorded in [session-identity.json](session-identity.json), tests
  disabled, ad-hoc signed, built at HEAD_PLACEHOLDER.
- Review page: [offline-package/index.html](offline-package/index.html), or
  ask for the live URL (`attended_review.py serve`). Each candidate offers
  the command that opens the build and the pull request link, both with
  Copy buttons. The page keeps your note, takes a pasted screenshot, lets
  you change or clear an answer, and has "Finish and hand back".
- Session: `cbd48b91000b00f1e72e1ad62c7c384c`, candidate revision
  `app-release-9334b13a-2026-09-10.2`. The v002 rejection of BPR-0646
  (decision `bdd9244a`) stays recorded against the v002 revision in the
  session's history; every v003 candidate is undecided.
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`,
  milestone `attended-preparation-pack-head`.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build.
2. Stay signed out. On Home press "Read without signing in".
3. Copy the pull request link from the page (microsoft/terminal#20607),
   paste it into the bar on Home and press Open or Return.
4. Wait for the pull request, then let the review history settle. The files
   drawer lists the four changed files and the epoch line sits under the
   strip; both were missing on the v002 bundle.
5. Look at the window at its default size and again at about 1280x820.
6. If the history fails, read
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`.
   A signed-out read counts against GitHub's anonymous limit of sixty
   requests an hour from this machine; if the pull request will not load
   at all, wait for the hour to turn.

## What each candidate asks

The criteria are unchanged from [v002](../rfc-bpr-0028-attended-v002/operator-packet.md):

- **BPR-0646, workbench chrome.** The strip is the title row beside the
  traffic lights with the epoch line beneath and nothing else above the
  comparison; a handle on the epoch line changes the comparison; a rail
  carries a vertical label and an unread count; signed out, sign-in is
  reachable without scrolling. The strip was accepted on v002; the files
  and the epoch line are what to look at first.
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

## What changed since v002

The v002 session (bundle 484b2072 at 97c8f9f1) recorded one rejection on
BPR-0646: "Can't open any files. Saw no epoch line." Two defects were
behind it, both fixed by BPR-0715: the workbench refused the signed-out
authority (no account, no viewer) before listing any file, and the
application host stopped seeing the primary window's accepted read once the
window composition of RFC-BPR-0024 landed. The Home direction in the same
note is registered as BPR-0716 under RFC-BPR-0026 and is not in this
bundle. A stale reflow assertion (BPR-0717) and the red workbench
composition contract (BPR-0718) were carded on the way; neither changes the
product.

## After the session

Import the exported decisions with `attended_review.py import --file FILE`
(the live page saves them itself). The integrator then finishes each card
from its own acceptance; a rejection becomes a bounded follow-up card.
