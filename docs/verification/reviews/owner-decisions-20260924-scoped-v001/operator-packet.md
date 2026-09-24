# Owner decisions, 2026-09-24, scoped session (v001)

Status: **10 candidates ready and protected at sequence 21, all pending. No
verdict is inferred from preparation.**

The owner asked: "if there are questions to answer or reviews to do then
prepare an attended review session". After the previous session the owner
said: "this attended review contained a bunch of stale questions that were
already answered, the attended review should be scoped to what this session
cares about". So this is a **new session with its own state**, holding only
the ten candidates below. Nothing already answered is on the page, and
nothing from the earlier queue is carried into it.

The earlier checkout session `cbd48b91000b00f1e72e1ad62c7c384c`
(`.mask/attended-review/session.json`) and its packet
[owner-decisions-20260924-v001](../owner-decisions-20260924-v001/operator-packet.md)
are untouched. This packet is a separate family rather than that family's
v002. A v002 would supersede v001, and the documentation gate would then
require an edit to v001's Status line and a new pointer on every open card
that names v001.

Every multiple-choice candidate ends with **Other: my answer is in the note**,
so a write-in answer never needs another box ticked (the owner's note on
BPR-1023 in session `cbd48b91`). Every candidate has the free-text note.
Answer any subset. A skipped item stays pending, and a hand-back approves
nothing.

## Durable session

- Session: `7083474240820608cdf9ff551ddd298f`
- Sequence after preparation: `21`
- State: `/Users/jaeger/.mask/attended-review/owner-decisions-20260924-scoped-v001/session.json`
  (external `--state`, not committed)
- Manifest: [candidate-manifest.json](candidate-manifest.json). Media are named
  by repository path and SHA-256. The manifest given to `init` carried each
  image inline as `data:image/png;base64,` plus the file's bytes.
- Identities and fingerprints: [session-identity.json](session-identity.json)
- Offline page: [offline-package/index.html](offline-package/index.html)

## The queue

One question per candidate.

| Candidate | Card | Question | Answer |
|---|---|---|---|
| `BPR-0975-wslg-choice-v001` | [[BPR-0975]] | A (WSLg-only workaround), B (wait for WSLg), or both | A, B, Both, Other |
| `BPR-0975-upstream-report-v001` | [[BPR-0975]] | Prepare an upstream report for microsoft/wslg#1022 from the prepared text | Yes, No, Other |
| `BPR-0974-defender-disposition-v002` | [[BPR-0974]] | The Defender disposition, informed by [[BPR-1110]]. BPR-0653's capture build waits on it | Multi-select of five, Other |
| `BPR-1109-hook-owner-v001` | [[BPR-1109]] | Optional: the Alt-chord hook's owner is known, find it, or ignore it | Known, Procedure, Ignore, Other |
| `BPR-0986-current-line-band-v001` | [[BPR-0986]] | The reader's line a shade apart under the selection wash, asked alone | Accept or reject |
| `BPR-0644-account-mismatch-erasure-v001` | [[BPR-0644]] | Material that signs in as another account: keep erasing it, or keep it until Sign out | (a), (b), Other |
| `BPR-0803-code-beside-cli-choice-v001` | [[BPR-0803]] | The GitHub CLI choice beside the device code, as rendered | Accept or reject |
| `BPR-1107-white-space-drawing-v001` | [[BPR-1107]] | Dots for spaces and lines to the stop for tabs | Accept or reject |
| `BPR-1107-white-space-choices-v001` | [[BPR-1107]] | The six choices and the default (Under the selection) | Proposed, Trailing default, Other |
| `BPR-1106-placement-v001` | [[BPR-1106]] | Where the composition entries go, A to E; C proposed | A, B, C, D, E, Other |

Not in this session: BPR-0653's Windows capture, which is not ready and waits
on the BPR-0974 answer, and everything the owner already answered in session
`cbd48b91`.

## BPR-0986's captures

[BPR-0986-current-line-band/](BPR-0986-current-line-band/) holds the four
captures the band's candidate embeds: light and dark, before and after, with
white space set to None ([[BPR-1107]]'s setting). Source was `9ee4ee50`,
exported with `git archive` into a scratch directory outside the checkout. Also
applied: the scratch harness and switch in
[scratch-capture-9ee4ee50.diff](BPR-0986-current-line-band/scratch-capture-9ee4ee50.diff),
which is not committed to the product. The build was `dev` Debug, macOS 26.6.2
arm64 with Qt 6.11.2, run with `QT_QPA_PLATFORM=offscreen`. The test binary's
SHA-256 is `e2f6e61ba7fb0d2202bc05f3e5bc415fa2a322bd3c7f341ea355c14526ff3168`.
"Before" is the same binary with `BPR_SCRATCH_CURRENT_LINE_BAND_OFF` set.

| Theme | Right line 4 (reading, washed) | Left line 4 | Right line 3 |
|---|---|---|---|
| Light, before | (219, 233, 249) | (219, 233, 249) | (255, 255, 255) |
| Light, after | (210, 227, 247) | (219, 233, 249) | (255, 255, 255) |
| Dark, before | (20, 35, 57) | (20, 35, 57) | (13, 17, 23) |
| Dark, after | (27, 43, 66) | (20, 35, 57) | (13, 17, 23) |

- The readings equal the BPR-0986 rendering note's.
- Before and after differ only in x 552 to 978, y 71 to 84: right line 4,
  5,945 pixels per theme.
- White space painted: 0 in every run.
- The "after" captures are byte-identical whether taken from the binary built
  without the switch or the one built with it.
- The two "before" captures are byte-identical to the rendering note's
  `panes-light-before.png` and `panes-dark-before.png` from `55e9405f`. So the
  fixture is the note's own, and with white space None and the band off, the
  panes are exactly what they were before BPR-0986.

## Live review

The host is running. Its complete URL, with its token, went to the
coordinator only. To start it again, or attach to it if it is running, use:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260924-scoped-v001 serve
```

Each decision is saved as it is made. **Finish and hand back** tells the driver
you have stopped. It approves nothing.

## Offline fallback

Open the offline page, decide any items, then use **Copy JSON** or **Download
JSON**. The driver imports the file with:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260924-scoped-v001 import --file /absolute/path/to/attended-decisions.json
```

## Driver watch

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260924-scoped-v001 wait --after 21
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-decisions-20260924-scoped-v001 status --after 21
```

Each verdict applies only to its candidate's recorded revision and
fingerprint. The integrator records it on the card. Nothing here moves a card.
