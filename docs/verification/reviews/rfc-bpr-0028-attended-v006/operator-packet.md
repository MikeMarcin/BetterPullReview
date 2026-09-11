# RFC-BPR-0028, RFC-BPR-0018 and RFC-BPR-0006 attended session, v006

Status: **prepared; fifteen decisions pending; no host running**.

The fifteen v005 candidates, prepared again on the bundle built at the
head that fixes the two red contracts the owner asked for before the next
round of reviews
([BPR-0718](../../../issues/60-done/BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md),
[BPR-0723](../../../issues/60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)).
The asks are unchanged from v005. Nothing here changes anything on
GitHub: the pull request is read signed out.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c`
  as recorded in [session-identity.json](session-identity.json), tests
  disabled, signed with the local identity the preset carries, built from
  the source at d1ba791f (`cmake --preset app-release && cmake --build
  build/app-release`, `codesign --verify --deep --strict` clean).
- What changed in the bundle since v005: the desktop path now decides
  whether the workbench layout store is usable by opening it once at
  composition and falls back to session defaults when it cannot
  (BPR-0718), and the automated-run status is read before shutdown
  (BPR-0723, a testing-only path). Nothing the reviewer sees is meant to
  differ from the v005 bundle.
- Review page: [offline-package/index.html](offline-package/index.html), or
  the live URL once `attended_review.py serve` is started. Each candidate
  offers the command that opens the build and the pull request link, both
  with Copy buttons. The page keeps your note, takes a pasted screenshot,
  lets you change or clear an answer, and has "Finish and hand back".
- Session: `cbd48b91000b00f1e72e1ad62c7c384c`, candidate revision
  `app-release-cc2afa7f-2026-09-11.2`, prepared at sequence 259. The v003
  rejection of BPR-0646 (decision `65f3f881`) stays recorded against the
  v003 revision in the session's history; every v006 candidate is
  undecided and protected.
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`,
  milestone `attended-preparation-pack-head`.

## Starting the session

Not started. The host and the watch are armed only on the owner's say-so:

    python3 .claude/skills/attended-review/scripts/attended_review.py \
        --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve

then, from the coordinator, `wait --after 259` as a persistent background
watch. Or open the offline page above and import its export afterwards with
`attended_review.py import --file FILE`.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build. This
   build keeps the workbench settings store at schema version 6 and the
   appearance settings store at schema version 4 (the v005 bundle's
   one-time palette reset has already happened if that bundle was opened;
   otherwise it happens now); an older build opened afterwards refuses
   either store rather than reading it, so go back to the older build only
   if you mean to.
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

Unchanged from v005: the
[v005 packet](../rfc-bpr-0028-attended-v005/operator-packet.md) describes
the thirteen carried from v004 and the two added there (BPR-0720, dark
palette; BPR-0724, Read width, an options group). No open question rides
this packet.

## After the session

Import the exported decisions with `attended_review.py import --file FILE`
(the live page saves them itself). The integrator then finishes each card
from its own acceptance; a rejection becomes a bounded follow-up card.
