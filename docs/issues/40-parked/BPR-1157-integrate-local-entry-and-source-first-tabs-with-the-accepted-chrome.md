---
schema: "implement-rfc.card.v1"
id: "BPR-1157"
title: "Integrate local entry and source-first tabs with the accepted chrome"
status: "Parked"
rfcs: ["RFC-BPR-0061"]
depends_on: ["BPR-1146", "BPR-1151"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC61-01", "RC61-02", "RC61-03", "RC61-09"], "pack_alias": "PACK-CYCLE-15", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC61-01","outcome":"pending","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"application_review_cycle_desktop_public_path and review_cycle_workbench on macOS arm64: signed-out Open Project to R0 full source, title row '<name> · Local cycle', first note, suggestion and related location, Home recents and resume, restart resumes by root, folder unchanged. Owner judgment pending: BPR-1157-local-identity-v001 and BPR-1157-home-section-v002 (v001 rejected at event 720 on a launcher fault, fixed at 028882c3a; see the 2026-10-01 note)."}},{"criterion":"RC61-02","outcome":"pending","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"The page replaces the review surface in place with the review bar and tabs above the panes, Updates folded, PR-only title-row controls and status line hidden; a pull request arriving takes the surface back and Home returns to the cycle. Owner judgment pending: BPR-1157-surface-rows-v001."}},{"criterion":"RC61-03","outcome":"pass","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Full source is one pane from R0; unchanged, whole-file and older-revision notes accepted; comparison gestures on cycle tabs refused so no all-added diff or empty baseline appears (core_review_cycle_tabs, qt_review_cycle_view, review_cycle_workbench). Comparison views are BPR-1163."}},{"criterion":"RC61-09","outcome":"pending","revision":"c2a0df54a7b2ca0b48f76777afc6975728392e51","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Automated share: tabs restored after restart, 900-pixel compact width, named regions, exact CRLF/UTF-8 byte anchors from native selection. Owner Mac observation pending (BPR-1157-native-keyboard-v001); Windows and Linux runs owed with the ledger command."}}]
---

Pack alias: **PACK-CYCLE-15** (live `BPR-1157`, allocated at import).

# BPR-1157: Integrate local entry and source-first tabs with the accepted chrome

## Outcome and bounded work

Add Open Project, cycle recents/title badge and R0 full-source view in the RFC0052 scaffold. Preserve title/review bar, fold, per-tab exact selection and tray. Prepare a small native design course for genuinely unresolved layout choices without replacing accepted art.

## Acceptance and direct evidence

No-account entry to first note, native width/height/focus/key checks, tab/source restoration, no extra permanent row, unchanged-file comment action and subject-switch regression to a real PR.

Criteria: RC61-01, RC61-02, RC61-03, RC61-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), Qt 6.11.2, `build/dev` freshly configured at `2631bcf31`, and the fix `384525fb9`. The Qt rows run as registered: `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`.

Evidence only; the card stays parked for the owner. At `2631bcf31`, `review_cycle_workbench` aborted on Linux: `std::optional<review_cycle_projection>::operator->` asserted on an empty optional. GCC 15's libstdc++ checks this in the unoptimized dev build; libc++ on macOS does not by default. The course held `resumed` as a reference to the host's live projection, called `close()`, which empties `Cycle`, and then read `resumed.Cycle->Subject`. That is undefined behaviour in the test, not the product. `384525fb9` copies the subject before closing. After it, `review_cycle_workbench` (tabs, revision, path and reading line restored after restart; resume from recents), `core_review_cycle_tabs`, `qt_review_cycle_view_1_0` and `_2_0` (900-pixel compact width and named regions, offscreen), and `application_review_cycle_desktop_public_path` (six 1280x800 renders produced; not inspected) pass. For RC61-09 this is the Linux automated share of keyboard and focus. It ran offscreen, not in a native desktop session; the owner's Mac observation and the Windows run remain.

The build is clean under GCC with no warnings. The logs are station records in `build/logs/bpr-station-authoring/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `2631bcf3`, with `f42d4e3a` and the Linux station's `384525fb`,
`build/windows-dev` (MSVC 19.51) builds with no warning. `f42d4e3a` renamed
two locals in `suggestion_editor.cpp` for C4458. `384525fb` fixed the
course's read of an emptied optional in `review_cycle_workbench`, the same
fix this station had made. The authoring rows pass offscreen: the six core rows,
`sqlite_review_cycle_storage`, `review_cycle_{feedback,suggestion,workbench}`,
`qt_suggestion_editor_*`, `qt_review_cycle_view_*` and
`application_review_cycle_desktop_public_path`. The four new uncontained pins
are measured and confirmed. The details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, authoring".

No native Windows attended run was made. This entry adds evidence only, and the card stays Parked.

## Home section rejection: a launcher fault, and the window it exposed — macOS, 2026-10-01

The owner rejected `BPR-1157-home-section-v001` at session event 720: "The
window I got looks not correct." The attached screenshot shows an empty
"Open Project · Local cycle" page with "Open a project to review it locally."
and no Home.

Diagnosis. The v001 launcher set only `HOME`, which macOS ignores for
Application Support. The frozen `c2a0df54a` app therefore opened the owner's
own profile, not the fresh one its item promised; its `home/` stayed empty.
Five minutes earlier the `f3327b74b` candidate had migrated that profile's
`review-cycles.sqlite3` to schema 7, and the schema-4 build refused it. Listing
recents failed at start, and the refusal took Home over with a page whose only
text was the generic notice. The reason was in the folded Updates line.

Fix at `028882c3a`. A refusal before any cycle opens takes the page only after
the reviewer asks for a cycle (Open Project or a recent row); at start Home
stays. The page states the refusal in the source pane. A schema mismatch reads
"This profile's Review Cycles were saved by a different version of
BetterPullReview; they were left as they are." The desktop course marks a
profile's store newer (`PRAGMA user_version=99`) and checks that Home stays at
start and that the refusal is shown after Open Project. Dropping the guard
fails "Home stays in place at start". The course's render of that page
matches the owner's screenshot.

Launchers. `CFFIXED_USER_HOME` moves the macOS profile; an offscreen probe of
both frozen apps wrote the profile under the given home and left
`~/Library/Application Support/BetterPullReview` untouched. The new candidate's
launcher sets it. The shared v001 launcher
`/Users/jaeger/bpr-cycles-authoring-review.xpXmIl/launch.py` now sets it as
well, so the four pending v001 items open their own profile. Their app is
unchanged, and each item has an observation recording this (events 730-733).
The owner's own profile keeps the schema-7 store.

Asked again as `BPR-1157-home-section-v002` on
`/Users/jaeger/bpr-cycles-syntax-review.rMJBTD` (app sha256
`85492e75…ebef307`). The item says what the owner should see and carries
native renders of Home, of Home with a recent, and of the refusal page, with
the v001 screenshot for comparison. The card stays Parked.
