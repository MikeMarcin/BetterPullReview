---
schema: "implement-rfc.card.v1"
id: "BPR-1194"
title: "Verify and close RFC-BPR-0061"
status: "Backlog"
rfcs: ["RFC-BPR-0061"]
depends_on: ["BPR-1193"]
closeout_for: ["RFC-BPR-0061"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "closeout", "acceptance": ["RC61-01", "RC61-02", "RC61-03", "RC61-04", "RC61-05", "RC61-06", "RC61-07", "RC61-08", "RC61-09", "RC61-10"], "pack_alias": "PACK-CYCLE-52", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-52** (live `BPR-1194`, allocated at import).

# BPR-1194: Verify and close RFC-BPR-0061

## Outcome and bounded work

Reconcile every criterion of RFC-BPR-0061 against its producing revision and direct evidence. Verify canonical predecessor closure and the imported ownership/remap. Leave unmet native/live/user gates open; do not relax criteria, recycle IDs or alter historical predecessor acceptance to close this RFC.

## Acceptance and direct evidence

Run the live documentation/board gates and record criterion/check/result/revision/environment. Close only this RFC through closeout_for; a complete pack validator or another RFC result is not substitute evidence.

Criteria: RC61-01, RC61-02, RC61-03, RC61-04, RC61-05, RC61-06, RC61-07, RC61-08, RC61-09, RC61-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

RFC base: `8466534f96f254d88fed412a587188b7b45f2580`.

All owned acceptance criteria remain **Not run** at intake.
## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| **RC61-01:** Open Project/Review Cycles offers a simple no-account/no-agent local entry with a clear Local cycle identity distinct from PRs and Review Tours. | Focused production-path checks and applicable native or service course at closeout | BPR-1157 share passes on macOS, Linux and Windows; the owner accepted `BPR-1157-home-section-v002` (event 791) and `BPR-1157-local-identity-v001` (event 792) in session `cbd48b91…`; BPR-1157 Done. Closeout reconciliation not run | `028882c3a` (product) |
| **RC61-02:** Cycle controls integrate the 0052 title/review bar, folding Updates line, tabs and tray without a new unapproved persistent row or PR regression. | Focused production-path checks and applicable native or service course at closeout | BPR-1157 share passes: the page replaces the review surface in place under the review bar and tabs, Updates folded, no extra permanent row, and a pull request takes the surface back (`application_review_cycle_desktop_public_path`, `qt_review_cycle_view_*`) on macOS, Linux and Windows; the owner accepted `BPR-1157-surface-rows-v001` (event 807); BPR-1157 Done. Closeout reconciliation not run | `c2a0df54a` (product) |
| **RC61-03:** Initial/full-source browsing is single-pane and remains available after comparisons; unchanged/historical local source stays commentable. | Focused production-path checks and applicable native or service course at closeout | BPR-1157 share passes: full source is one pane from R0; unchanged, whole-file and older-revision notes; comparison gestures on cycle tabs refused (`core_review_cycle_tabs`, `qt_review_cycle_view_*`, `review_cycle_workbench`) on macOS, Linux and Windows; BPR-1157 Done. Comparison views are BPR-1163's share. Closeout reconciliation not run | `c2a0df54a` (product) |
| **RC61-04:** Native note/suggestion workflows support primary/related anchors, accurate before/after, keyboard editing and explicit stale/ambiguous states; Suggest change starts from the caret, including the source pane's context menu, edits the whole file in the main pane, and previews removed and added lines in the diff panes' colours (owner amendment, 2026-10-01). | Focused production-path checks and applicable native or service course at closeout | BPR-1156 share passes: anchors, before/after in the diff fills, stale and ambiguous states, Suggest change from the caret and the context menu, the whole file in the main pane. The 15-row selection passes on macOS, Linux and Windows, and the owner accepted `BPR-1156-composer-layout-v006` (session `cbd48b91…`, event 789). Closeout reconciliation not run | `fd5ca4412` (product), `caae4cc9e` (tests) |
| **RC61-05:** Compose/Share prepares one complete assignment with first-class Copy and Save and no mandatory AI setup, second prompt or per-item ceremony. | Focused production-path checks and applicable native or service course at closeout | BPR-1161: the owner accepted `BPR-1161-compose-share-v004` (session `f18e0aba…`, event 7) on the Share notes sheet, one-gesture Copy round N and Save as File…, with an owner amendment renaming Show Code to Show Change, applied at `9cc3087d2`; macOS selection and `qt_` rows 132/132, Linux and Windows at `17691bc63`; BPR-1161 Done. Closeout reconciliation not run | `9cc3087d2` (product) |
| **RC61-06:** Whole-round feedback and exact delivery observations remain legible; optional direct/external routes add no hidden context or automatic dispatch. | Focused production-path checks and applicable native or service course at closeout | BPR-1161: the owner accepted `BPR-1161-delivery-words-v001` (session `cbd48b91…`, event 859); each delivery observation in its own words, external routes only when reachable and showing the exact invocation, starting nothing; the share sheet keeps the words verbatim; BPR-1161 Done. Closeout reconciliation not run | `7a385389c` (product) |
| **RC61-07:** Capture, response display, comparison, reviewer verification and next-round composition work without callback/protocol prerequisites. | Focused production-path checks and applicable native or service course at closeout | Not run | None |
| **RC61-08:** Publication preview differentiates bases and selected final content and links the actual PR while preserving local history/privacy. | Focused production-path checks and applicable native or service course at closeout | BPR-1177 share passes: the Publish as Pull Request sheet keeps P, T and R1–Rn apart, words each path, keeps a ledger of what goes to GitHub and what stays, and names the linked pull request (`review_cycle_publish_desk`, `application_review_cycle_publish_public_path`) on macOS, Linux and Windows; simulated GitHub; the owner accepted `BPR-1177-publish-sheet-v001` (session `aed7c9e8…`, event 8). BPR-1177 Done. Closeout reconciliation not run | `875192071` (product) |
| **RC61-09:** Native width/focus/shortcuts/accessibility/IME/tab-restoration courses preserve accepted workbench design and record actual platform/user evidence; every Tab stop shows a visible focus ring, and in the read-only code Return notes the selection or the caret's line and Shift+Return begins Suggest change there (owner decision, 2026-10-02). | Focused production-path checks and applicable native or service course at closeout | BPR-1157 share passes: a focus ring on every stop and Return/Shift+Return in the code (macOS offscreen and native; Linux at `862e31a93`), accepted as `BPR-1157-native-keyboard-v002` (event 806); the owner's request with it, Escape (and Control-C on macOS) cancels the open note or suggestion, passes on macOS offscreen and native. Windows run of both owed, not gating. BPR-1157 Done. Other native courses (IME, tab restoration on Windows/Linux) not run. Closeout reconciliation not run | `73b2160fc`, `fa9f42381` (product) |
| **RC61-10:** Named-corpus latency/memory and the eight-item/two-suggestion human-steering course measure benefit/limitations without unverified savings claims. | Focused production-path checks and applicable native or service course at closeout | BPR-1184: named-corpus latency and memory measured on macOS, Linux and Windows Release with mixed and negative findings recorded (the 514-item round's costs, BPR-1274; Windows cold open and a compare outlier); the owner accepted `BPR-1184-steering-benefit-v001` (session `db739d0e…`, event 5) with an empty note, recorded as a verdict without measured steering counts; no savings claimed. BPR-1184 Done. Closeout reconciliation not run | `397908dec` (product) |

## Owner answer (2026-10-06): amend 0050, do BPR-0653

RFC-BPR-0061 depended on RFC-BPR-0050, 0052 and 0037, all Proposed. The
coordinator put the question to the owner in chat: "RFC-0061 … depends on
RFC-0050, 0052 and 0037 …". The owner answered, verbatim:

> Amend 0050, do BPR-0653

The answer came in chat. No attended candidate was prepared for it, and
session `cbd48b91000b00f1e72e1ad62c7c384c` was not used.

The amendment is recorded in RFC-BPR-0061's
[owner amendment](../../rfcs/RFC-BPR-0061-native-review-cycles-workbench-and-usability.md#owner-amendment-2026-10-06-rfc-bpr-0050-is-not-a-dependency)
section and in the [RFC index](../../rfcs/README.md): RFC-BPR-0050 is
dropped, because the cycle page uses none of its label controls, marks or
settings. RFC-BPR-0052 and 0037 stay, and this card stays in Backlog until
they are Auto-Verified. RFC-BPR-0050 is not closed by the amendment; it stays
open on [[BPR-1100]].

What BPR-0653 needs from the owner, on Windows. The station prepared it on
2026-10-06 (BPR-0653's section "The 100% look, prepared for the owner on
JaegAW"), and the owner chose to clear it in person:

1. On JaegAW, set the primary display's scale to 100%. It is a system
   setting, and nothing changes it for the owner.
2. Run `C:\Users\mikem\bpr0653-100pct-v001\launch.cmd`. It checks the
   image's digests, refuses unless the display is at 96 DPI, starts the image
   the owner accepted at 150% (`6aa27c8d…`, bundle `ca3bc2b1…`) signed out on
   a private temporary profile, and reports `WINDOW_UP`, `EXITED`,
   `NO_WINDOW` or a refusal within 20 s into `launch.log`.
3. In the Windows desktop browser, open attended session
   `39ba8abc36558edf2edecab37b68b819` (a loopback page on JaegAW; not
   `cbd48b91…`) and answer its one item, `BPR-0653-windows-100pct-live-v001`,
   Accept or Reject with a note: at 100%, is the strip the title row (the
   caption buttons at rest, hovered and pressed; the snap layouts flyout;
   dragging empty strip space; double-click to maximize and to restore)?
4. Hand the session back, and set the display scale back if wanted.

That answers BPR-0653's criteria 2 and 4. Criterion 3 needs no owner: the
2026-09-25 station pass at `3da31f2a` passed 19 of 20 rows with the manifest
red only on BPR-1108's four rows, and the card says it is met once the
coordinator accepts that red as outside the criterion or those rows run
contained.

The chain after BPR-0653:

1. [[BPR-1021]] closes RFC-BPR-0028 (BPR-0653 is its last unmet card;
   BPR-0746 is Done by amendment).
2. RFC-BPR-0037: its closeout [[BPR-0927]] is Done and it waits only on
   RFC-BPR-0028 being Auto-Verified. No open card is named to record that
   status change.
3. [[BPR-1135]] closes RFC-BPR-0052: its seventeen card dependencies are Done,
   and its RFC dependencies are RFC-BPR-0025 and 0029 (Auto-Verified), 0028
   and 0037.
4. BPR-1194 (this card) closes RFC-BPR-0061 once [[BPR-1193]] is Done and
   RFC-BPR-0052 and 0037 are Auto-Verified.
