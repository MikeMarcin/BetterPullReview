---
name: update-promo-assets
description: Regenerate BetterPullReview promotional screenshots and the demo recording from the real application window. Use when the owner asks to update marketing, README, website, hero, demo, screenshot or screen-recording assets for BetterPullReview, including the copies on betterpullreview.com.
---

# Update Promo Assets

Promotional media comes from the real application, not mockups. The test-enabled
build serves the constructed `promo-v1` pull request
(`bpr-demo/lantern#512`, [[BPR-1676]]) under its own temporary profile, and
`scripts/capture_promo_assets.py` drives that window and captures it.

## Before you start

- Run this only when the owner has asked for promotional assets in this
  session. It is the one exception to AGENTS.md's rule against driving the
  owner's desktop: for about a minute it brings the window to the front and
  types into it. Tell the owner before you start, and ask them to turn on Do
  Not Disturb and leave the keyboard alone.
- The run needs Screen Recording and Accessibility permission for the terminal
  or agent host it runs from. A missing permission fails the run; it does not
  capture anything else.
- Every capture is of the application's own window (`screencapture -l` for
  stills, ScreenCaptureKit for the recording). Never switch to a screen-region
  capture: a region records whatever sits above the window, including this
  conversation.
- The content is synthetic. If a capture shows anything else (a real account,
  a path from this machine, another application), discard it and recapture.

## Workflow

1. Check the tree and build the test-enabled application:

   ```sh
   git status --short
   cmake --build --preset dev --target app_better_pull_review
   ```

   Run the build without a pipe, or check its exit status: a failed CMake
   regeneration still ends with ninja's own output.

2. If the promo fixture's content should change, edit
   `scripts/build_promo_fixture.py` and regenerate every committed tree it
   owns, in order:

   ```sh
   python3 scripts/build_promo_fixture.py tests/fixtures/github/pull-requests/promo-v1/construction.json
   python3 scripts/build_recorded_pull_request_pack.py tests/fixtures/github/pull-requests/promo-v1/construction.json tests/fixtures/github/pull-requests/promo-v1/objects.pack
   python3 scripts/build_recorded_pull_request_contract.py tests/fixtures/github/pull-requests/promo-v1/construction.json tests/fixtures/github/contracts/v1/promo-v1
   python3 scripts/build_promo_fixture.py --threads tests/fixtures/github/contracts/v1/promo-v1
   python3 tests/scripts/recorded_pull_request_fixture_tests.py
   python3 tests/scripts/github_contract_fixture_security_tests.py
   ```

   A content change moves the commit identifiers. `PACK_OK` prints them; put
   them in the builder's `BASE_OID` and `HEAD_OID` and in the `promo_v1`
   descriptor in `src/app/src/github_public_path_fixture.cpp`, then rebuild.

3. Check the fixture offscreen before touching the desktop:

   ```sh
   QT_QPA_PLATFORM=offscreen build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review \
     --github-fixture promo-v1 --render-evidence build/dev/promo-v1.png
   ```

   It must print `PUBLIC_PATH_OK`.

4. Capture. Point `BETTERPULLREVIEW_SITE_ROOT` at the website checkout to copy
   the assets into its `static/images` as well:

   ```sh
   BETTERPULLREVIEW_SITE_ROOT=/path/to/betterpullreview.com \
     python3 scripts/capture_promo_assets.py --confirm-desktop
   ```

   The script hides the files drawer so the comparison has room, then writes
   to `docs/images/promo/`:

   | File | Shows |
   |---|---|
   | `betterpullreview-review.{png,webp}` | Review: the rewritten `begin_frame`, the link map, and the blocking thread with two unread replies |
   | `betterpullreview-overview.{png,webp}` | Overview: description, files, read state and Status |
   | `betterpullreview-discuss.{png,webp}` | Discuss: every thread's latest post with its label |
   | `betterpullreview-threads.{png,webp}` | Threads: the finder across files |
   | `betterpullreview-demo.{mp4,webp}` | 15 s: Threads, search, the thread opened beside its code, two changes, Discuss |

   The run fails rather than export two identical stills, which is what a key
   reaching another application looks like.

5. Look at every still and step through the recording before committing.
   Commit the assets in this repository and, separately, in the website
   checkout.

## Verification

```sh
magick identify docs/images/promo/*.png
ffprobe -v error -show_entries format=duration -of csv=p=0 docs/images/promo/betterpullreview-demo.mp4
webpmux -info docs/images/promo/betterpullreview-demo.webp
```

Report the files written, their dimensions, the recording's length and frame
count, and confirm the fixture application exited.
