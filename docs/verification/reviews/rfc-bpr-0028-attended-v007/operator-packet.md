# RFC-BPR-0028, RFC-BPR-0018, RFC-BPR-0026 and RFC-BPR-0006 attended session, v007

Status: **prepared; ten decisions pending; no host running**.

The four v006 rejections asked again, and the six cards landed since the
v006 hand-back, on the bundle built at the head where
[BPR-0733](../../../issues/60-done/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md)
was parked. Nothing here changes anything on GitHub: the pull request is
read signed out, and an anonymous read counts against GitHub's sixty
requests an hour from this machine.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827`
  as recorded in [session-identity.json](session-identity.json), tests
  disabled, signed with the local identity the preset carries, built from
  the source at 2cdc0e57 (`cmake --preset app-release && cmake --build
  build/app-release`, log `build/logs/app-release-2cdc0e57.log`,
  `codesign --verify --deep --strict` clean). `src/app/src/github_application.cpp`
  is unchanged since the v006 bundle head d1ba791f, so the two composition
  digests pinned there did not move.
- What changed in the bundle since v006: the epoch line carries the
  catalogue's updates on a restored or cached open (BPR-0725); C and C++
  calls, types and ALL_CAPS macros are coloured (BPR-0726); Overview refuses
  a hand-opened drawer and is chosen, never inferred (BPR-0727); the
  conversations drawer narrows below 375 in Read (BPR-0728); the signed-out
  Home is the account card and one paste field that opens a public link in
  one gesture, with no queue and no "Read without signing in" (BPR-0732);
  and the app no longer opens the last pull request at startup but lists
  it under Recently viewed (BPR-0733; restoration store `user_version` 3,
  migrated from 2 with the old row as the newest entry).
- Review page: [offline-package/index.html](offline-package/index.html), or
  the live URL once `attended_review.py serve` is started. Each candidate
  offers the command that opens the build and the pull request link, both
  with Copy buttons. The page keeps your note, takes a pasted screenshot,
  lets you change or clear an answer, and has "Finish and hand back". The
  page lists the fifteen decided v006 candidates as decided; their
  screenshots are not embedded (they are the PNG files in the `captures` directory beside
  [the v006 packet](../rfc-bpr-0028-attended-v006/operator-packet.md)).
- Session: `cbd48b91000b00f1e72e1ad62c7c384c`, candidate revision
  `app-release-64d9e46a-2026-09-11.3`, ten candidates ready and protected
  at sequence 311. The fifteen v006 verdicts stay recorded against
  `app-release-cc2afa7f-2026-09-11.2`; the four re-asked candidates carry
  the new revision and keep their v006 rejections in the session's history.
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`,
  milestone `attended-preparation-pack-head`.

## Starting the session

Not started. The host and the watch are armed only on the owner's say-so:

    python3 .claude/skills/attended-review/scripts/attended_review.py \
        --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve

then, from the coordinator, `wait --after 311` as a persistent background
watch. Or open the offline page above and import its export afterwards with
`attended_review.py import --file FILE`.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build. This
   build keeps the workbench settings store at schema version 6, the
   appearance store at schema version 4 and moves the restoration store to
   `user_version` 3; an older build opened afterwards refuses those stores
   rather than reading them, so go back to the older build only if you mean
   to.
2. Nothing opens by itself now. Home appears at once; if a review was open
   at the last quit it is the first entry under "Recently viewed", between
   the paste field and the queue. Open microsoft/terminal#20607 from there,
   or paste its link into the field and press Open or Return. Signed out
   there is no "Read without signing in" button any more and no queue; the
   field opens the public pull request directly.
3. Let the review history settle. The catalogue on this Mac already holds
   the head of #20607 from the v006 session, so this launch is the cache
   hit that emptied the epoch line in v006; the line should now carry the
   updates without an acquire.
4. Look at the window at its default size and again at about 1280x820.
5. For the dark palette and the identifier roles, use Dark appearance
   (Settings > Appearance, or System on a dark desktop) and open
   `ScreenBufferTests.cpp` in Read or Review.
6. For Overview's drawer rule, switch modality with the strip and try the
   drawer toggles and Alt+Shift+1/2/3 in each. For the conversations
   drawer, switch to Read and drag the handle between the code and the
   drawer to the right.
7. For Recently viewed, read a little, quit, relaunch: Home again, the
   entry first, "Resume reading" returns to the position.
8. If the history fails, read
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`.

## What each candidate asks

Asked again from v006, each on the fix its rejection named:

- **BPR-0646, epoch handles.** Narrowed to the one criterion left: a
  handle on the epoch line changes the comparison. The strip, the rails
  (BPR-0721) and the sign-in criterion were settled in v006.
- **BPR-0663, public preview history.** The line shows the updates rather
  than "No updates loaded."; a cache hit is now enough, and the
  `smart_acquire` log line only reappears when GitHub holds a newer head.
- **BPR-0676, epoch line legibility.** Nodes, span and amber tick legible
  at the default size and at about 1280x820; the three-word state visible.
  Say whether the line should also name whether the pair is the default or
  your own choice.
- **BPR-0720, dark palette.** Unchanged values and reset; asked again
  because the missing colours were BPR-0726's, which is its own candidate.

New in this round:

- **BPR-0725, cache hit.** Opening a pull request the catalogue already
  holds shows the nodes, span and handles.
- **BPR-0726, identifier roles.** In `ScreenBufferTests.cpp`, calls pale
  yellow, types teal, ALL_CAPS macros preprocessor grey, comments and
  strings untouched. Misfires count as much as misses.
- **BPR-0727, Overview drawers.** Overview refuses a hand-opened drawer and
  the status bar says why; the other modalities still open theirs. Its
  second criterion is a judgement made while landing the card, shown for
  your verdict: the window no longer infers Overview from the files-only
  drawer shape at restart, because that shape is also a Read with the
  conversations closed by hand; Overview is only ever chosen.
- **BPR-0728, conversations drawer width.** The handle takes the drawer
  well below 375 in Read, controls stay reachable, and the width survives
  Home and back. This is the second option you selected on BPR-0724.
- **BPR-0732, signed-out Home.** The account card, one paste field and
  Open, no queue or filters, no "Read without signing in"; a pasted public
  link opens in one gesture. BPR-0716 is finished with this one.
- **BPR-0733, Recently viewed.** Relaunch with a review saved shows Home at
  once with the entry; open it by hand; "Resume reading" returns to the
  position. Its third criterion is a change made while landing the card,
  shown for your verdict: a refused startup *authority* (no grant, or the
  capability not composed) is now silent because nothing is pending at
  startup, while a refused destination or store still speaks.

No open question rides this packet beyond the two judgements above.

## After the session

Import the exported decisions with `attended_review.py import --file FILE`
(the live page saves them itself). The integrator then finishes each card
from its own acceptance; a rejection becomes a bounded follow-up card.
After the verdicts: [BPR-0734](../../../issues/30-in-progress/BPR-0734-a-design-round-for-the-single-pane-read-layout.md),
then the Windows and Linux inventories on their stations.
