# Owner decisions, 2026-09-24 (v001)

Status: **22 new candidates at sequence 566: 21 questions ready and
protected, and one capture waiting on a Windows build. The earlier licensing
question is also ready and still pending. No verdict is inferred from
preparation.**

The owner asked: "if there are questions to answer or reviews to do then
prepare an attended review session". This packet puts every owner decision
found pending on the board at main `86cd8227` into the checkout's standing
session. Each item asks one question, binds its card and commit, and has
option buttons where the card offers options. Answer any subset; skipped
items stay pending and nothing is read into a skip or a hand-back.

## Durable session

- Session: `cbd48b91000b00f1e72e1ad62c7c384c` (the checkout session)
- Sequence after preparation: `566`
- State: `/Users/jaeger/Documents/GitHub/BetterPullReview/.mask/attended-review/session.json` (not committed)
- Manifest: [candidate-manifest.json](candidate-manifest.json)
- Identities and fingerprints: [session-identity.json](session-identity.json)
- Offline page: [offline-package/index.html](offline-package/index.html)

## The queue

| Candidate | Card | Question |
|---|---|---|
| `BPR-0653-entry-row-v001` | [[BPR-0653]] ask 3 | Amend RFC-0028 AC-01 to admit the entry rows, or move the entries. Also settles [[BPR-1036]]'s floor and [[BPR-1033]]'s AC3 |
| `BPR-0653-private-qt-api-v001` | [[BPR-0653]] ask 2 | Accept or refuse Qt's private `setCustomMargins` in the shipped app |
| `BPR-0653-windows-capture-v001` | [[BPR-0653]] ask 1 | **Waiting**: the capture needs a JaegAW tests-disabled build at or after `e08a0409` |
| `BPR-0986-hunk-header-roles-v001` | [[BPR-0986]] ask 1 | The two hunk-header roles: (a) drop, C5 47 to 45; (b) keep with a note; (c) RFC-0006 |
| `BPR-0986-rendered-panes-v001` | [[BPR-0986]] ask 2 | Accept the rendered tab markers and current-line band (captures embedded) |
| `BPR-0975-wslg-activation-v001` | [[BPR-0975]] | A real Linux desktop, or an amendment accepting WSLg's refused activation |
| `BPR-1023-refused-connect-v001` | [[BPR-1023]] | With activation refused, may the hidden Connect begin device sign-in |
| `BPR-0974-defender-disposition-v001` | [[BPR-0974]] | Fresh scanned image, false-positive submission, or deferral to RFC-0038 |
| `BPR-1020-uncontained-pins-v001` | [[BPR-1020]] | Keep or revert the four extra Windows pins |
| `BPR-1038-purge-policy-v001` | [[BPR-1038]] | Accept the inherited owner-only directory rule for Purge profile |
| `BPR-0884-independent-review-v001` | [[BPR-0884]] | Independent read-only review of `02e3d59d`: yes or no |
| `BPR-0906-independent-review-v001` | [[BPR-0906]] | Independent read-only review of `03117e63`: yes or no |
| `BPR-1048-independent-review-v001` | [[BPR-1048]] | Independent read-only review of `a670fd95`: yes or no |
| `BPR-1104-independent-review-v001` | [[BPR-1104]] | Independent read-only review of `51784082`: yes or no |
| `BPR-1024-upstream-qt-report-v001` | [[BPR-1024]] | Prepare an upstream Qt report from the WSLg capture |
| `BPR-1022-alt-chord-hook-v001` | [[BPR-1022]] | The Alt-chord keyboard hook on JaegAW: known, ignore, or identify read-only |
| `RFC-0039-0051-start-v001` | RFC index | Start the imported feature packs now, in canonical order |
| `BPR-0746-disposition-v001` | [[BPR-0746]] | Amend away the unobtainable AC1/AC2, or keep it Blocked |
| `BPR-0644-grant-policy-v001` | [[BPR-0644]] | Keep or erase a remembered device grant on a locked Keychain or offline launch |
| `BPR-0897-observed-mutation-intent-v001` | [[BPR-0897]] | Is publishing to any open pull request the intended release behaviour |
| `BPR-0825-descriptor-home-v001` | [[BPR-0825]] item 3 | Where the Windows owner-private descriptor definition lives |
| `BPR-0803-cli-during-device-flow-v001` | [[BPR-0803]] | Cancel a waiting device flow on Use GitHub CLI, or keep its code on screen |
| `rfc0014-m9-windows-licensing-v002` | [[BPR-0962]] AC1 | Carried forward unchanged from sequence 476 |

Not asked, with reasons, is in `session-identity.json`: [[BPR-0735]] and the
[[BPR-0746]] probe were declined; [[BPR-0153]] and [[BPR-0641]] are
owner-deferred; the M8/M9 native cards wait on environments, not answers.

## Live review

Start or attach to the loopback host from the checkout:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve
```

It prints the complete URL, with its token, once. Open that URL on this Mac.
Each decision is saved as it is made. **Finish and hand back** tells the
driver you have stopped; it approves nothing.

## Offline fallback

Open the offline page in a browser, decide any items, then use **Copy JSON**
or **Download JSON**. Give the file to the driver, who imports it with:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview import --file /absolute/path/to/attended-decisions.json
```

The page holds only this queue, so its export imports cleanly. Browser storage
is not the record until the export is imported.

## Driver watch

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview wait --after 566 --kind handed-back
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview status --after 566
```

Each verdict applies only to its candidate's recorded revision. The
integrator records it on the card; nothing here moves a card.
