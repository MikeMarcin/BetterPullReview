# RFC-BPR-0028, RFC-BPR-0018 and RFC-BPR-0006 attended session, v005

Status: **prepared; fifteen decisions pending; no host running**.

The thirteen v004 candidates, prepared again on the bundle built at the
head that lands the owner's answers to the v003 questions
([BPR-0720](../../../issues/40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md),
[BPR-0724](../../../issues/60-done/BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md)),
plus one candidate for each of those two. Nothing here changes anything
on GitHub: the pull request is read signed out.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `d238842d2e7a78ee46f70c0268f9ff57f8b92a6a43758a4eba7d7fe680617519`
  as recorded in [session-identity.json](session-identity.json), tests
  disabled, signed with the local identity the preset carries, built from
  the source at 25e1525d (`cmake --preset app-release && cmake --build
  build/app-release`, `codesign --verify --deep --strict` clean, a
  rebuild at 25e1525d is a no-op with the same digest).
- Review page: [offline-package/index.html](offline-package/index.html), or
  the live URL once `attended_review.py serve` is started. Each candidate
  offers the command that opens the build and the pull request link, both
  with Copy buttons. The page keeps your note, takes a pasted screenshot,
  lets you change or clear an answer, and has "Finish and hand back".
- Session: `cbd48b91000b00f1e72e1ad62c7c384c`, candidate revision
  `app-release-d238842d-2026-09-11.1`, prepared at sequence 228. The v003
  rejection of BPR-0646 (decision `65f3f881`) stays recorded against the
  v003 revision in the session's history; every v005 candidate is
  undecided and protected.
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`,
  milestone `attended-preparation-pack-head`.

## Starting the session

Not started. The host and the watch are armed only on the owner's say-so:

    python3 .claude/skills/attended-review/scripts/attended_review.py \
        --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve

then, from the coordinator, `wait --after 228` as a persistent background
watch. Or open the offline page above and import its export afterwards with
`attended_review.py import --file FILE`.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build. This
   build keeps the workbench settings store at schema version 6 and moves
   the appearance settings store to schema version 4, which clears the
   saved custom palette once (the owner's "yes reset"); an older build
   opened afterwards refuses either store rather than reading it, so go
   back to the older build only if you mean to.
2. If a review opens on its own at launch, that is the restored review;
   otherwise stay signed out, press "Read without signing in" on Home, paste
   the pull request link (microsoft/terminal#20607) into the bar and press
   Open or Return.
3. Let the review history settle. A signed-out read counts against
   GitHub's anonymous limit of sixty requests an hour from this machine.
4. Look at the window at its default size and again at about 1280x820.
5. The drawers show no rail and no Hide button by default. To judge the
   rails and the Hide buttons (BPR-0646's third criterion, BPR-0675, and
   the second half of BPR-0721) turn on Settings > Reviews & queue >
   Drawers > Drawer controls: Shown.
6. For the dark palette, use Dark appearance (Settings > Appearance, or
   System on a dark desktop) and open a C++ file in Read or Review.
7. If the history fails, read
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`.

## What each candidate asks

The thirteen carried over are the v004 criteria, unchanged; the v004
packet describes them. In this build Read also collapses the repositories
drawer, so a Read screenshot has one drawer beside the comparison.

The two new ones:

- **BPR-0720, dark palette.** In Dark appearance comments are green,
  `#include` lines and their path grey, keywords blue, types teal,
  functions pale yellow, variables light blue, strings salmon, on the
  unchanged background and on added and removed rows; and the old saved
  customisation is gone. Three Visual Studio values were lightened
  slightly (keyword, comment, preprocessor) to stay readable on the diff
  row fills; the light and high-contrast palettes and the diff row
  colours are unchanged.
- **BPR-0724, Read width.** An options group: after this build's change,
  should Read also close the conversations drawer (amending the RFC's Read
  definition), keep it but let it go narrower than 375 pixels, get a
  single-pane layout as a new slice, or stay as it is.

## Answered since v004

The three v003 questions are answered and recorded: Read mode lacked
horizontal room for the code (BPR-0724), the whole dark palette moves to
the Visual Assist scheme with the saved palette reset (BPR-0720), and the
strip below about 960 pixels stays as it is (BPR-0722). No open question
rides this packet.

## After the session

Import the exported decisions with `attended_review.py import --file FILE`
(the live page saves them itself). The integrator then finishes each card
from its own acceptance; a rejection becomes a bounded follow-up card.
