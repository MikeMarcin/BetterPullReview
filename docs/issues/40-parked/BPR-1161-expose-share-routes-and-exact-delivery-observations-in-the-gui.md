---
schema: "implement-rfc.card.v1"
id: "BPR-1161"
title: "Expose share routes and exact delivery observations in the GUI"
status: "Parked"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0061"]
depends_on: ["BPR-1156", "BPR-1157", "BPR-1159"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-07", "RC56-10", "RC61-05", "RC61-06"], "pack_alias": "PACK-CYCLE-19", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}, "related_issues": ["BPR-1212"]}
parked_on: "human-review"
acceptance: [{"criterion":"RC56-07","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 core_review_cycle_share, sqlite_review_cycle_storage, review_cycle_workbench, qt_review_cycle_view_* and the desktop course: copied/saved/retrieved/accepted/acknowledged admitted only from channels that can show them, for this round and digest; copied only when the platform clipboard returns the same text (else uncertain); saved under the file name only; each listed in its own words with 'none means received, read, acted on or accepted'; no note status changes; persisted across restart. Negative controls fail."}},{"criterion":"RC56-10","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Every route equals authorize's reviewer export over 225 classification/channel/setting rows, and a granted harness's retrieval gets the same answer; a clipboard denied for an unclassified plain folder copies, records and freezes nothing, says so, writes no file instead, and Save keeps its own decision (core_review_cycle_share, review_cycle_workbench, qt_review_cycle_view)."}},{"criterion":"RC61-05","outcome":"fail","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"observation","path":"docs/design/BPR-1161-share-round-v1.html","detail":"Owner Reject of BPR-1161-compose-share-v001 on the frozen app dc64a2c2...0f2128 (session cbd48b91, event 834, decision bf77d66c-1ec3-4cca-9ee7-6d6df2d036e4): 'There is a lot going on in this screen and it's not a very pretty experience. I have no idea how I got to this screen, why I would wnat to do any of these actions, and what is important.' The automated share still passes. Design round v1 (57293c334) audits the page and offers directions A, B and C; the choice is asked as BPR-1161-share-direction-v001 (sequence 837)."}},{"criterion":"RC61-06","outcome":"pending","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes (each observation kind worded apart; external route only when reachable, shows the exact invocation, records and starts nothing). Owner judgment asked as BPR-1161-delivery-words-v001 in session cbd48b91 (sequence 826)."}},{"criterion":"BPR-1161:boundary","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"49 of 49 changed rows incl. architecture_boundaries and test_network_isolation_manifest on macOS; core-only 22 of 22; clang-format and clang-tidy clean on changed units. Linux and Windows runs owed (commands on the card)."}}]
---

Pack alias: **PACK-CYCLE-19** (live `BPR-1161`, allocated at import).

# BPR-1161: Expose share routes and exact delivery observations in the GUI

## Outcome and bounded work

Compose feedback and Share feedback in the accepted native layout. Copy and Save are first-class, with selection/order/context preview; optional routes are capability-driven. Freeze+delivery may be one action without confusing copied/saved/read/acknowledged states.

## Acceptance and direct evidence

No-agent native share course, identical round Copy then Save, amendment after share, explicit destination/bytes, denied channel, oversized output and no per-item confirmation/second prompt. No automatic executable discovery or network on preview.

Criteria: RC56-07, RC56-10, RC61-05, RC61-06.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-02

At `7a385389c`. Compose feedback opens the cycle's latest round draft, or
starts the first with every open note, in the centre of the cycle page (Back
and Escape leave it, as on a pull request's Compose review). Copy to
Clipboard, Save to File… and Save as Bundle… are buttons on the page; each
freezes the previewed revision and shares it in one click. Use in external
agent and Send to Agent appear only when the host reaches them, which this
build does not. Every route is decided by `core::authorize`; a refused one is
shown refused, shares nothing and is never replaced by another. Delivery
observations are admitted by the core per channel, kept in the store's new
`deliveries` table (schema 8) and listed on the page in their own words.
Owned paths, commands, the 49-row pass, ten negative controls and the
compatibility decision are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"BPR-1161: Compose feedback, Share feedback and delivery observations".

RC56-07 and RC56-10 pass on the automated evidence. RC61-05 and RC61-06 have
their automated share and wait for the owner.

## Pending owner judgment

One Accept/Reject question per criterion in session
`cbd48b91000b00f1e72e1ad62c7c384c`, added at sequence 826:

- `BPR-1161-compose-share-v001` (RC61-05): can one complete round be shared
  in one gesture, with Copy and Save first-class?
- `BPR-1161-delivery-words-v001` (RC61-06): are the round and each delivery
  observation legible, said apart, with nothing claiming more?

Candidate: `/Users/jaeger/bpr-cycles-share-review.3aySQ6/BetterPullReview-cycles-share.app`,
executable SHA-256 `dc64a2c2956914f3774607908888680abd7959f7a58d31e153ab8d9820d2f128`,
built at `7a385389c`, launched with
`python3 /Users/jaeger/bpr-cycles-share-review.3aySQ6/launch.py` (own profile
in `home/`, `HOME` and `CFFIXED_USER_HOME` set; saves may go to `shared/`).
`7a385389c` landed as `8cbdc1477` with identical BPR-1161 source.

## Owner verdict at hand-back 835: the share page is rejected (2026-10-02)

`BPR-1161-compose-share-v001` (RC61-05) was **rejected** at event 834,
decision `bf77d66c-1ec3-4cca-9ee7-6d6df2d036e4`, fingerprint
`2056fe1c…efe3`, on the frozen app above. The owner's note, verbatim:

> There is a lot going on in this screen and it's not a very pretty experience. I have no idea how I got to this screen, why I would wnat to do any of these actions, and what is important.

The session was handed back at 835. RC61-05 now records `fail` at
`7a385389c`. The automated share evidence still stands; what failed is how
the page presents it. `BPR-1161-delivery-words-v001` (RC61-06) was left
undecided and is still open. The rejected candidate was released by name at
836 (`resume-development --candidate BPR-1161-compose-share-v001`). Its
frozen app stays protected for the delivery-words question.

### Design round v1 (2026-10-02)

The owner asked for this design iteration to use `frontend-design`. Product
source was not touched in this round, because BPR-1169 was in progress in
`src/`. [The overview](../../design/BPR-1161-share-round-v1.html) audits the
rejected page from the candidate's own render. It pins 11 findings under the
owner's three complaints. The entry is an unlabeled pencil, and the tray's
"Next round" reads like the way to share. The only context is the title
"F2". Three equal buttons sit above the choices that shape them. A facts
line, Move Up and Move Down, and an empty "Shared so far" box add clutter.
The largest element is a raw-Markdown preview full of hashes. The file list
and the Notes tray repeat the notes beside the page. The overview links
three directions, each drawn at 1280×800 in the app's light and dark
palettes, before and after Copy:

- [A: Share sheet](../../design/BPR-1161-share-round-A-sheet-v1.html): a
  labelled Share notes button opens a sheet over the code with Copy round 2
  as its default.
- [B: Share from Notes](../../design/BPR-1161-share-round-B-tray-v1.html):
  no page. The Notes tray holds the round, and its foot holds Copy round 2.
- [C: The round as a letter](../../design/BPR-1161-share-round-C-letter-v1.html):
  the page renders the round as its reader gets it, with one side panel to
  send it.

All three keep RC61-05's one-gesture freeze, first-class Copy and Save, no
AI setup, second prompt or per-note confirmation, and the exact text. The
design commit is `57293c334`, with the audit image under
`docs/verification/BPR-1161-share-round-v1/`.

**Why this work lives here and not on [[BPR-1212]].** BPR-1212 is the later
rethink of a pull request's Compose review (RFC-BPR-0037 and RFC-BPR-0052).
It is in Backlog and covers summary and disposition, inline comments,
replies, kept drafts and the AI workshop. This page is Review Cycles' share
step. It is bound to RC61-05 and RC61-06 on this card, which is parked on
exactly this judgment. Moving the work there would either pull an unrelated
backlog card into the Review Cycles pack, or make this card wait on it. The
two are linked instead. Whichever direction is chosen here gives BPR-1212 a
starting vocabulary, and BPR-1212 now says so.

**Asked:** `BPR-1161-share-direction-v001` in session `cbd48b91`, added at
sequence 837 and ready at 839, revision `57293c334`, fingerprint
`e177ae92…cbc6b`. It is one option-list question, because A, B and C are
named alternatives, and it allows a note. It asks which direction the share
page takes. It says the delivery-words question is still open. The next
round reads `status --after 839`, builds the chosen direction, and asks
RC61-05 again on a new frozen candidate.

## Owed platform runs

Not gating. Linux station (`build/dev`):
`cmake --build --preset dev && ctest --preset dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$|^architecture_boundaries$"`.
Windows station: the same selector with `--preset windows-dev` plus
`|^test_network_isolation_manifest$`.

### Linux at `dbe6966ea`: share page and delivery records (bpr-main-linux, 2026-10-02)

Includes `8cbdc1477`. WSL2 (i9-10900KF, 20 logical processors), GCC 15.2,
Qt 6.11.2, `build/dev` Debug, offscreen. `cmake --build --preset dev` exits 0
with 0 `warning:` lines. The requested `ctest --preset dev -j4` selection
passes **12 of 12** in 32.96 s:

- `core_review_cycle_share` and `core_review_cycle_policy`
- `sqlite_review_cycle_storage`
- `review_cycle_{round,export,bundle,verification}`
- `qt_review_cycle_view_1_0` and `_2_0`
- `application_review_cycle_desktop_public_path`, 2.82 s. The real clipboard
  works offscreen on Linux; there was no clipboard-owner failure.
- `architecture_boundaries`

**`review_cycle_workbench`, which renders the 16 MiB round, takes 32.94 s at
-j4, and 30.94 s and 31.00 s alone.** The row sets no `TIMEOUT` property, so
it runs under CTest's default. No remote mutation.

## Windows (bpr-main-win, 2026-10-02)

At `dbe6966ea`, which contains `8cbdc1477`. Windows 11 Home 10.0.26200, MSVC
14.51, Qt 6.11.2, `build/windows-dev` Debug. The 10 executables these rows
run were deleted first, and the build rebuilt every one. It printed no
`warning C` or `warning LNK` line. The sources changed since `ce2b02c0b`
declare no `near`, `far` or `small` identifier.

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^core_review_cycle_share$|^core_review_cycle_policy$|^sqlite_review_cycle_storage$|^review_cycle_(workbench|round|export|bundle|verification)$|^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$|^architecture_boundaries$|^test_network_isolation_manifest$"
```

12 of 13 pass, in 53.87 s:

- `review_cycle_workbench`, which renders the 16 MiB round, passes in
  38.13 s. It has no `TIMEOUT` of its own, so CTest's 1,500 s default bounds
  it.
- `application_review_cycle_desktop_public_path` 15.67 s.
- `qt_review_cycle_view_1_0` 8.82 s and `_2_0` 17.60 s, against 60 s.
- `review_cycle_round` 1.62 s, `_verification` 2.56 s, `_bundle` 1.37 s and
  `_export` 0.54 s.
- `sqlite_review_cycle_storage` 3.39 s.
- `core_review_cycle_share` and `core_review_cycle_policy`.
- `architecture_boundaries` 32.92 s.

`test_network_isolation_manifest` fails, and not on this card's rows. All 58
of its lines are "uses `bpr_windows_uncontained` but is not in the pinned
Windows set". No `review_cycle_*` row or other row of this card is among
them.

- Most of them are [[BPR-1237]]'s unmeasured pins: the private-review,
  tour-publication, logging, scale and other rows that card lists.
- The 11 `application_large_review_*` rows are not on [[BPR-1237]]: active
  work, cache bytes and entries, history and its session race, launch, loaded
  close, pathological, provider cancel, resources and responsiveness.
  `cef11b982` (BPR-0967) registered them `WINDOWS_UNCONTAINED` without adding
  them to `scripts/verify_test_network_isolation.py`'s Windows set. They are
  BPR-0967's to pin.

## `review_cycle_workbench` bound, and the manifest (bpr-main-win, 2026-10-02)

`review_cycle_workbench` now sets `TIMEOUT 120`. That is three times the
slowest isolated run, Windows Debug's 38.13 s, rounded up
(`docs/development/build-and-test.md`). Linux, about 31 s, and macOS, about
18 s, sit well inside it. On Windows it passes in 34.91 s under the new
bound. `test_network_isolation_manifest`, red above on other cards' rows, now
passes on Windows ([[BPR-1237]]).
