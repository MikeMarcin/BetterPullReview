# Owner decision, 2026-09-25: BPR-0653's Windows capture at 150% (v001)

Status: **1 candidate ready and protected at sequence 4, pending. No verdict is
inferred from preparation.**

The owner could not reach the Windows machine, so the station captured
[BPR-0653](../../../issues/40-parked/BPR-0653-windows-caption-in-the-strip.md)'s
caption and strip visuals itself. The owner reviews them here. This is a
**new session with its own state**, holding only this one question: nothing
from an earlier session is on the page. That follows the owner's rule that a
session is scoped to the current questions only.

The earlier sessions are untouched. They are the checkout session
`cbd48b91000b00f1e72e1ad62c7c384c` (packet
[owner-decisions-20260924-v001](../owner-decisions-20260924-v001/operator-packet.md))
and the scoped session `7083474240820608cdf9ff551ddd298f` (packet
[owner-decisions-20260924-scoped-v001](../owner-decisions-20260924-scoped-v001/operator-packet.md)).
The unprepared candidate `BPR-0653-windows-capture-v001` in session `cbd48b91`
was never asked. This session's candidate asks its question for 150%.

## Durable session

- Session: `df5859c784ba447497026d6cd81af810`
- Sequence after preparation: `4`
- State: `/Users/jaeger/.mask/attended-review/owner-decisions-20260925-bpr0653-v001/session.json`
  (external `--state`, not committed)
- Manifest: [candidate-manifest.json](candidate-manifest.json). The images are
  named by repository path and SHA-256. The manifest given to `init` carried
  each one inline as `data:image/png;base64,` plus the file's bytes, so the
  fingerprint binds those bytes.
- Source binding: [source-manifest.txt](source-manifest.txt) names the capture
  folder's 39 PNGs, `capture.json` and `index.md` with their SHA-256. While
  this packet is pending, the documentation gate fails if any of them changes.
- Identities and fingerprint: [session-identity.json](session-identity.json)
- Offline page: [offline-package/index.html](offline-package/index.html)

## The question

| Candidate | Card | Question | Answer |
|---|---|---|---|
| `BPR-0653-windows-capture-150-v001` | [[BPR-0653]] criteria 2 and 4 | Accept the Windows caption and strip visuals as the station captured them at 150%? | Accept, Reject (note), Other (note) |

Fingerprint `b086dd44326bf32cef5d0703988972e3cd5839786c5cc2434ae66c009e39e775`,
revision `727ad9d7 (capture of exe 6aa27c8d…98ac4, source 85e522d8)`.

The item states the capture's limits plainly:

- **150% only. 100% was not captured.** The display ran at the scale Windows
  is set to, DPI 144. Capturing 100% needs Windows' display scale changed, a
  system setting on the owner's machine, and that is deferred. It stays owed
  on the card whatever the answer.
- **Station-driven, not owner-driven.**
  [`windows_caption_capture.ps1`](../../../../tests/scripts/windows_caption_capture.ps1)
  moved the pointer with `SendInput`, and no person did. Every move and press
  was guarded.
- **Also not pictured:** edge snap, a pull request open (the window is on Home,
  signed out), the 1920x1080 maximized size, and the maximizing double-click.
  The `application_windows_title_row` course asserts the maximizing
  double-click. The drag appears in `capture.json`'s frame coordinates, not in
  the window-only pictures.
- **Four window images are redacted.** In `hover-maximize`, `snap-flyout`,
  `pressed-maximize` and `maximized`, other applications' content was filled
  with the surrounding colour. `capture.json` keeps each pre-redaction
  SHA-256 (`captured_sha256`).

## The images

All 39 are embedded on the page: 13 states, and for each the window, the strip
and the caption-button crop. Each one is clickable to full size. They live in
[BPR-0653-2026-09-25](../../BPR-0653-2026-09-25/index.md), exactly as the
station committed them in `727ad9d7`, and this packet copies none of them.
[docs/AGENTS.md](../../../AGENTS.md) says screenshots are not to be checked in
as substitutes for runtime proof. These are human-review artifacts for this
question, so they stay in `docs/verification/` beside their `capture.json`.
The runtime proof beside them stays the `application_windows_title_row` course.

The preparer checked that all 39 files hash to `capture.json` (0 mismatches)
and recorded that as an observation, not a verdict. The page also names one
measurement for the owner to judge: the edges of the Maximize, Restore and
Close glyphs carry sub-pixel colour fringes.

## Live review

The host is running. Its complete URL, with its token, went to the
coordinator only. To start it again, or to attach to it while it runs:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260925-bpr0653-v001 serve
```

Each decision is saved as soon as it is made. **Finish and hand back** tells
the driver you have stopped. It approves nothing.

## Offline fallback

Open the offline page, decide the item, then use **Copy JSON** or **Download
JSON**. The driver imports the file with:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260925-bpr0653-v001 import --file /absolute/path/to/attended-decisions.json
```

## Driver watch

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260925-bpr0653-v001 wait --after 4
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260925-bpr0653-v001 status --after 4
```

The verdict applies only to this candidate's recorded revision and
fingerprint. The integrator records it on the card. Nothing here moves a card.
