# Owner review, 2026-09-25: a fresh session (v001)

Status: **Answered: handed back at sequence 54 with all 17 candidates
decided, and each verdict recorded on its card (2026-09-25). Protections were
released at sequence 55, the host is stopped and the runtime directory is
deleted. This packet describes `3759b0d8` and the frozen m2 app on purpose.**

The owner asked for a fresh attended review session. This is a **new session
with its own state**. It holds every explicit human question that is still
unanswered and can be judged on this Mac, and nothing that has already been
answered. Answer any subset. A skipped item stays pending, and **Finish and
hand back** approves nothing.

## Hand-back, 2026-09-25

The owner decided every candidate and handed back at sequence 54. Every
decision is `actor: human`, and its fingerprint equals the one in
[session-identity.json](session-identity.json). `BPR-1129-bar` and
`BPR-1132-plus` were each answered twice; the session keeps the later
decision current and lists the earlier one as superseded, and the later one
is recorded.

| Candidate | Decision, seq | Verdict | Recorded on |
|---|---|---|---|
| `BPR-0295-ac1-provenance-v001` | `d7af6bd8`, 35 | Amend AC1 for this exact v008 asset set | [[BPR-0295]] (Ready), RFC-BPR-0019 |
| `BPR-0641-private-pr-smoke-v001` | `cbba6fac`, 36 | accept, with two findings | [[BPR-0641]] (Done), [[BPR-1199]], [[BPR-1200]] |
| `BPR-0966-compose-dif-v001` | `dba44cfd`, 37 | A visible workshop slot on Compose | [[BPR-0966]] (Ready) |
| `BPR-1129-title` | `aee924e0`, 40 | Keep | [[BPR-1129]] |
| `BPR-1129-bar` | `fd158272`, 48 | Revise: file before change | [[BPR-1129]], [[BPR-1202]] |
| `BPR-1129-kept` | `011a2ebb`, 39 | reject | [[BPR-1129]], [[BPR-1203]] |
| `BPR-1134-tray` | `6c6d2c52`, 52 | reject | [[BPR-1134]], [[BPR-1204]] |
| `BPR-1134-voiceover` | `6ee64abe`, 53 | Works | [[BPR-1134]] |
| `BPR-1130-fold` | `74ee4e1c`, 41 | Keep | [[BPR-1130]] |
| `BPR-1130-history` | `2edcf8e2`, 42 | Clear enough | [[BPR-1130]] |
| `BPR-1130-pair-ready` | `ccba3a15`, 43 | Tooltip | [[BPR-1130]] |
| `BPR-1130-readiness` | `5b349ec3`, 44 | Track when open, summary when folded | [[BPR-1130]], [[BPR-1201]] |
| `BPR-1133-default-reply` | `1a095bfe`, 51 | Reply | [[BPR-1133]] |
| `BPR-1133-arrow` | `b3d1d32d`, 49 | Keep, with a finding | [[BPR-1133]], [[BPR-1207]] |
| `BPR-1133-card-reply` | `37c42e44`, 50 | Keep | [[BPR-1133]] |
| `BPR-1132-plus` | `c1a81e29`, 47 | Duplicate, with focus and sizing | [[BPR-1132]], [[BPR-1206]] |
| `BPR-1132-later` | `2c856168`, 45 | Hidden | [[BPR-1132]], [[BPR-1205]] |

All fourteen RFC-BPR-0052 verdicts are also summarised on [[BPR-1135]]. The
owner's screenshots show a private repository or the owner's other projects.
They stay with their decisions in `session.json` and are not committed. After
recording, `resume-development --all` released this session's 30 resource
claims. Session `60bd1ed5`'s 28 claims on the same frozen app and Qt were
untouched. `stop` ended host PID 86003, and the runtime directory above,
which held the BPR-0641 build and the probes, was deleted.

## Durable session

- Session: `06ad5734ed9f7c2fd6d0a62c8328fa0a`
- Sequence after preparation: `34`
- State: `/Users/jaeger/.mask/attended-review/owner-review-20260925-fresh-v001/session.json`
  (external `--state`, not committed)
- Runtime: `/Users/jaeger/.mask/attended-review/owner-review-20260925-fresh-v001-runtime/`
  holds the BPR-0641 build, the probes and their logs (not committed)
- Manifest: [candidate-manifest.json](candidate-manifest.json). Media are named
  by absolute path and SHA-256; the manifest given to `init` carried the same
  bytes inline.
- Source binding: [source-manifest.txt](source-manifest.txt) names the 59
  repository files the candidates declare, at `3759b0d8`. While this packet is
  pending, the documentation gate fails if any of them changes.
- Identities, fingerprints and readiness evidence: [session-identity.json](session-identity.json)
- Offline page: [offline-package/index.html](offline-package/index.html)

## Earlier sessions

None was changed, stopped or released.

- `cbd48b91000b00f1e72e1ad62c7c384c`, the checkout's default state, sequence
  610. Its verdicts stay durable there. Its one unanswered item,
  `BPR-0653-windows-capture-v001`, is Windows-only.
- `60bd1ed56e768d60a40b104920641436`, the RFC-BPR-0052 m2 session, cursor 42,
  14 unanswered. Its server on port 63317 and its hand-back waiter belong to
  the Codex coordinator and still run. Its 14 questions are carried here
  **verbatim, with identical fingerprints** and shared static claims on the
  same frozen app and Qt. A verdict given here can be imported there by
  changing only the `session` field of the exported JSON.
- `fc60f0ba…`, BPR-0768's d14 session: one waiting item for a card that is
  Done.

## The queue

One question per candidate.

| Candidate | Card | Question | Answer |
|---|---|---|---|
| `BPR-1130-readiness` | [[BPR-1130]], [[BPR-1135]] | Where loading and readiness words appear | Track and summary, Keep the row, Other |
| `BPR-1129-title` | [[BPR-1129]] | The title row at 1280×852 | Keep, Revise, Other |
| `BPR-1129-bar` | [[BPR-1129]] | The review bar at 1280×852 and the width floor | Keep, Revise, Other |
| `BPR-1129-kept` | [[BPR-1129]] | Compose review's amber kept-draft mark | Clear enough, Change, Other |
| `BPR-1134-tray` | [[BPR-1134]] | Comments, its count, then Findings as the tray header | Keep, Revise, Other |
| `BPR-1134-voiceover` | [[BPR-1134]] | VoiceOver announces the tray and Right Arrow reaches Findings | Works, Needs correction, Other |
| `BPR-1130-fold` | [[BPR-1130]] | The Updates disclosure and fold | Keep, Revise, Other |
| `BPR-1130-history` | [[BPR-1130]] | The incomplete-history mark | Clear enough, Change, Other |
| `BPR-1130-pair-ready` | [[BPR-1130]] | Pair-ready words in the comparison menu's tooltip | Tooltip, Back on the bar, Other |
| `BPR-1133-default-reply` | [[BPR-1133]] | Comment on a thread's line replies to the thread | Reply, Separate comment, Other |
| `BPR-1133-arrow` | [[BPR-1133]] | The interim Comment arrow menu | Keep, Revise, Other |
| `BPR-1133-card-reply` | [[BPR-1133]] | Reply on the thread card beside the code | Keep, Revise, Other |
| `BPR-1132-plus` | [[BPR-1132]] | The strip's plus duplicates the active view | Duplicate, File chooser, Other |
| `BPR-1132-later` | [[BPR-1132]] | Later threads on an older pair: listed or hidden | Listed, Hidden, Other |
| `BPR-0966-compose-dif-v001` | [[BPR-0966]] | Which surface owns Dif while a workshop edit is on Compose | Workshop slot, Corner (with [[BPR-0800]]), No Dif and amend B7, Other |
| `BPR-0295-ac1-provenance-v001` | [[BPR-0295]] | Accept Dif's recorded provenance unknowns for RFC-BPR-0019 AC1, or supply them | Amend AC1, Supply facts, Other |
| `BPR-0641-private-pr-smoke-v001` | [[BPR-0641]] | EpicGames/UnrealEngine#14366 opens read-only in the tests-disabled build | Accept, Reject (note) |

The five RFC-0052 cards are Done; their owner questions are pending exits of
the m2 milestone recorded on [[BPR-1135]].

## What makes each one judgeable

**The 14 RFC-0052 questions.** The frozen app is
`/Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe/BetterPullReview-m2.app`,
source `ba9d2723`, executable `a2e5128a…8a83`, both rechecked. The Qt 6.11.2
digest and all 55 fixture files match their recorded values, and the
per-question fixture roots are empty. Each question's launch command, run
with a scratch root and the network denied, opened the window on Home. The
same binary's fixture public path printed `PUBLIC_PATH_OK`,
`GITHUB_FIXTURE_OK:uiforetw-74`, `RENDER_OK` and `REVIEW_LEDGER_OK` in 13 s and
rendered pull request 74 in the workbench. A first attempt at the default
8 s run budget printed `PUBLIC_PATH_FAILED` with `RUN_BUDGET_EXPIRED` at
stage 2, after `Pull request ready.`; the rerun used
`--automated-run-budget-ms 90000`. Main has 27 product commits since
`ba9d2723`, touching 12 Qt adapter files, among them `workbench_window.cpp`,
`document_tab_strip.cpp` and `conversation_cards_view.cpp`. These verdicts
bind to the frozen build only.

**BPR-0966.** The Dif context course ran at the card's candidate `12ea8646`,
course `86de5254…85be`, canary `109315a6…cc`. It failed exactly as the card
records, in 19.09 s: on Compose the editor shows, the AI tools slot and Dif
are hidden, and ready and acknowledge never paint. `src/` and `tests/` at
`12ea8646` equal main's `faf53c8f`, and `3759b0d8` still opens Compose before
presenting Dif. It was not rerun at `3759b0d8`.

**BPR-0295.** `python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif`
on a `git archive` export of `3759b0d8` printed `DIF_ASSETS_VALID` in 45 s.
`provenance.json` (`f77354d6…6f1c`), the sprite manifest (`c9f3f38e…f40d`) and
the v008 verdict (`57a2a6d8…e116`) match the
[RFC-0019 ledger](../../RFC-BPR-0019.md#ordinary-closeout-audit--2026-09-25).

**BPR-0641.** `cmake --preset app-release` and `cmake --build --preset app-release`
on a `git archive` export of `3759b0d8`, which excludes the uncommitted
BPR-0972 work. The build used the checkout's dependency cache, offline, and
finished 389 of 389 steps. Its only two warnings are in third-party
libarchive. `codesign --verify --deep --strict` passes on the ad-hoc
signature, and the executable is `5b398420…e1ed`. Launched with a scratch home
and the network denied, it showed Home signed out and exited on SIGTERM. The
real profile directory was not touched. The preparer did not sign in or open
the pull request, and no live account was used. The owner launches it on
the normal profile, signs in, and opens #14366 read-only.

## Not asked

- [[BPR-0153]] is Mac-eligible but **unready**. Nobody has named the
  unfamiliar collaborator, the concrete defect or a disposable pull request,
  and no publication or contact with another person is authorized.
- [[BPR-0653]] owes the 100% capture on the Windows station. BPR-0974 and
  BPR-1033 are Windows. BPR-0960 is Windows; BPR-0961 and BPR-1122 are Linux.
- BPR-0958 needs a clean macOS account with a default Keychain. BPR-0959
  needs a native Intel Mac. BPR-0911 needs authorized live private-release
  evidence. BPR-0962 needs release-security inputs. None of these is an
  attended question.
- BPR-0735: the owner declined the editor observation, so it is not asked
  again by default.
- BPR-0972, BPR-1128 and BPR-1139 carry no human criterion.
  [[BPR-1121]]'s native menu-bar row needs an Aqua session to run, but it is
  an automated row, not a human judgment.
- BPR-0143, BPR-0144 and BPR-0169 wait on live GitHub. BPR-0720 and BPR-0738
  are accepted and parked only on an immutable review link.

## Live review

The host is running. Its complete URL, with its token, went to the
coordinator only. To start it again, or to attach to it while it runs:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-review-20260925-fresh-v001 serve
```

Each decision is saved as soon as it is made.

## Offline fallback

Open the offline page, decide any items, then use **Copy JSON** or
**Download JSON**. The driver imports the file with:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-review-20260925-fresh-v001 import --file /absolute/path/to/attended-decisions.json
```

## Driver watch

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-review-20260925-fresh-v001 wait --after 34 --kind handed-back
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/owner-review-20260925-fresh-v001 status --after 34
```

Each verdict applies only to its candidate's recorded revision and
fingerprint. The integrator records it on the card. Nothing here moves a card.
