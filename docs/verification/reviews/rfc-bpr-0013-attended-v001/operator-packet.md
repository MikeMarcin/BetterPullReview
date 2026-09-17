# RFC-BPR-0013 and RFC-BPR-0016 attended session, v001: the selected-diff read and the device relaunch

Status: **three candidates ready and protected in the consumer checkout's attended-review session `cbd48b91000b00f1e72e1ad62c7c384c` (sequence 389); owner verdicts pending; no host or watch running**.

## The bundle

The tests-disabled bundle was built from `94f6f8859993be4fea63c52855bab4f0144b185e` and is at
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-dsp3/src/app/BetterPullReview.app`.
Its executable,
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-dsp3/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`,
has SHA-256
`96fa16fea8983347303c6ce18095f147871b7d45ef51537d48097ac6004e1bfa`.
The build cache has `BUILD_TESTING=OFF`. The bundle is signed with the same
stable Developer ID identity as the earlier `build/app-release` attended
bundles, not ad hoc, and `codesign --verify --deep --strict` passed. One
`--self-test` run printed `AUTOMATED_PROFILE_PRIVATE_OK` and exited 0 without
touching the Default profile. The digests are in
[session-identity.json](session-identity.json). No earlier packet directory
was changed.

## Before any step

**Quit your own running BetterPullReview first.** It holds the Default
profile. While it runs, opening this bundle only hands that app an activation,
and you would be judging the wrong build. In Terminal, this must print nothing:

```sh
/usr/bin/pgrep -il 'betterpullreview|better_pull_review'
```

Then launch the exact bundle:

```sh
open /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-dsp3/src/app/BetterPullReview.app
```

No step asks you to paste a token, a device code or anything private into
this review. You enter a device code only on GitHub's own page. Verdict notes
should name a step, a state or a refusal, never file paths, code, commit IDs
or thread text.

## The three questions

Do them in this order. BPR-0795 watches the same read as BPR-0794, and BPR-0644's
device sign-in must be the last account change before the app quits.

1. `BPR-0794-cli-selected-diff`, fingerprint
   `2daa504175f67d96a261f73ace0a9e0fc9f438276d71d4ac4b4a2d6c08cba213`
   ([[BPR-0794]], acceptance bullet 4). Through the already opted-in GitHub.com
   GitHub CLI profile (Settings, Accounts & hosts, GitHub CLI on the GitHub.com
   card), open `https://github.com/EpicGames/UnrealEngine/pull/1773` from Home.
   Does the app show the correct file list and a readable selected diff whose
   source and review-thread anchors belong to the accepted epoch pair, with a
   Right OID equal to the pull request's current head commit? A remaining
   explicit refusal is a reject and leaves the card open.
2. `BPR-0795-loading-visible`, fingerprint
   `2be69bec3e74f237599460eb9a73f2024800c776dd171846b6941f0703c4cf62`
   ([[BPR-0795]]). On that same read, do pressing Return on the link and then
   selecting a file each show a visible loading state promptly
   (Loading pull request…, then Loading selected file comparison…) while the
   window stays responsive? The card's four acceptance bullets are automated
   and need no person. This is only the person-observable part of milestone
   exit 5. Large-review laziness itself (which Git objects are fetched, and that
   an older update waits until it is selected) cannot be seen by eye. It is
   not asked; the focused host, Qt and portable course checks carry it.
3. `BPR-0644-device-relaunch-8h`, fingerprint
   `e62486ae2e5ccc18176c364c94dca4716c10fd014e638e5d5099909b6652f3b2`
   ([[BPR-0644]], attended criterion). Sign in to GitHub.com with a device code
   in this bundle, note the time and quit it. For more than eight hours, open
   no BetterPullReview build, including
   `/Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/src/app/better_pull_review.app`.
   Every build shares the Default profile, and a differently signed build may be
   refused by the Keychain and erase the grant. Then confirm that
   `/usr/bin/shasum -a 256 /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-dsp3/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`
   still prints the digest above and relaunch the same bundle. Does it resume
   the session with no credential prompt? Launching this bundle may also forget
   a device grant made by a differently signed build, under today's
   fail-closed rule; the question's sign-in then starts from this bundle.

Each candidate on the page carries its full step list, the launch command and
the values to copy.

## Recording verdicts

[Open the offline review page](offline-package/index.html)
(`/Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/reviews/rfc-bpr-0013-attended-v001/offline-package/index.html`
once integrated). It is self-contained, keeps partial decisions and exports
them as JSON. One or two answers are useful on their own; BPR-0644's
answer comes more than eight hours later.

The page and its export are bound to the consumer checkout's session
`cbd48b91000b00f1e72e1ad62c7c384c`. The packet was first prepared in an isolated worktree, as session
`a3c10cc9039eae4acc894cd6464e3f06`. After integration, the integrator merged
the same manifest into this checkout's session, prepared the three candidates
there and regenerated this page. The candidate fingerprints bind the manifest,
not the session, so they did not change. The same commands regenerate the page
if the session is ever rebuilt:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview init --manifest /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/reviews/rfc-bpr-0013-attended-v001/candidate-manifest.json
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview prepare
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview html --output /Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/reviews/rfc-bpr-0013-attended-v001/offline-package/index.html
```

Import an exported JSON file with the same script's
`import --file /absolute/path/to/export.json`. For a live session, run `serve`
with the same `--repo-root` and arm the driver watch with
`wait --after <sequence>`. No host was started while preparing this packet.
No verdict is inferred from the build, the self-test or any automated check,
and no step in this packet writes to GitHub.
