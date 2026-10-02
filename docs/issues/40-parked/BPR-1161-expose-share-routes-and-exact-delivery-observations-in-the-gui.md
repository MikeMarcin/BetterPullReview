---
schema: "implement-rfc.card.v1"
id: "BPR-1161"
title: "Expose share routes and exact delivery observations in the GUI"
status: "Parked"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0061"]
depends_on: ["BPR-1156", "BPR-1157", "BPR-1159"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-07", "RC56-10", "RC61-05", "RC61-06"], "pack_alias": "PACK-CYCLE-19", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC56-07","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 core_review_cycle_share, sqlite_review_cycle_storage, review_cycle_workbench, qt_review_cycle_view_* and the desktop course: copied/saved/retrieved/accepted/acknowledged admitted only from channels that can show them, for this round and digest; copied only when the platform clipboard returns the same text (else uncertain); saved under the file name only; each listed in its own words with 'none means received, read, acted on or accepted'; no note status changes; persisted across restart. Negative controls fail."}},{"criterion":"RC56-10","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Every route equals authorize's reviewer export over 225 classification/channel/setting rows, and a granted harness's retrieval gets the same answer; a clipboard denied for an unclassified plain folder copies, records and freezes nothing, says so, writes no file instead, and Save keeps its own decision (core_review_cycle_share, review_cycle_workbench, qt_review_cycle_view)."}},{"criterion":"RC61-05","outcome":"pending","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes (host and desktop courses: first draft of every open note, exact preview, one-click Copy freezes and copies, Save same bytes under the round's name, bundle, conflict asks, amendment after share, oversized copy refused with measure). Owner judgment asked as BPR-1161-compose-share-v001 in session cbd48b91 (sequence 826) on the frozen app dc64a2c2...0f2128."}},{"criterion":"RC61-06","outcome":"pending","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share passes (each observation kind worded apart; external route only when reachable, shows the exact invocation, records and starts nothing). Owner judgment asked as BPR-1161-delivery-words-v001 in session cbd48b91 (sequence 826)."}},{"criterion":"BPR-1161:boundary","outcome":"pass","revision":"7a385389cdf0f544cec5724d28104de8d106dfe9","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"49 of 49 changed rows incl. architecture_boundaries and test_network_isolation_manifest on macOS; core-only 22 of 22; clang-format and clang-tidy clean on changed units. Linux and Windows runs owed (commands on the card)."}}]
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
