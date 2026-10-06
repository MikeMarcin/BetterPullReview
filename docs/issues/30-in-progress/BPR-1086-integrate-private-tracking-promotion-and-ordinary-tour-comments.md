---
schema: "implement-rfc.card.v1"
id: "BPR-1086"
title: "Integrate private tracking, promotion and ordinary tour comments"
status: "In Progress"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0051"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-12","milestone":"authoring","acceptance":["R16","R17","R18","R19"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1086: Integrate private tracking, promotion and ordinary tour comments

Pack alias: **PACK-CC-12** (live `BPR-1086`, allocated at import).

## Outcome and work

Reuse explicit defect classification/verification and available note/finding promotion into the shared draft. Keep severity/provenance private. Exercise ordinary and deliberately labelled tour comments against the inherited segment/order/publication contracts. Record absent deferred interfaces without duplicating them.

## Acceptance

- Issue recognition offers tracking but does not silently create or close defects.
- Website resolution, patch application and relabelling cannot verify or withdraw a private defect.
- Promotion preserves lineage and tours preserve order/links/hooks with no mandatory taxonomy.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation (2026-10-06)

Built on `bpr-main-linux` in `/home/mmarcin/BetterPullReview` on `main`,
claimed at `06a3c530b`. Engineering owner `rfc-implementer`, model
`claude-opus-5-5`, effort `high`. Product commits `62c44db39` (segment owner,
Qt reading, core proofs) and `8a9e280a4` (host proofs, composed course, pin
reseal). The run ledger's BPR-1086 section has the full evidence and the
failing-before controls.

What already held, and was only proved here:

- **Tracking is explicit.** The thread menu's "Classify as defect…"
  (BPR-1208) is offered on any review thread on a current line, an `issue`
  root included. `application::correlate` reads no comment text, so a label
  cannot reach a defect. Evidence (an author reply, a website resolution, a
  code change under the anchor) only ever moves a defect to
  `needs_verification`. `verify` stays the reviewer's explicit command.
- **Promotion.** Each live promotion path copies exactly the text the
  reviewer accepted: RFC-BPR-0023's Squash to Draft, BPR-1210's note copy and
  RFC-BPR-0012's finding conversion. None appends severity, category or
  runtime provenance. The squash path keeps its lineage privately: the
  draft's composition link, then the candidate's derivation, then the
  finding, then its run.
- **Tour hooks.** No shipped tour hook asks for a feedback label.

What was built:

- **A labelled published tour stop now shows its label.** Before, Qt read
  every body from its first byte, where a published stop has its pinned
  source and generated heading, so a stop whose author labelled the
  narrative showed no mark. `core::published_feedback_segment`
  (`feedback_conversation.hpp`) is now the one reading of a published body's
  user text. It is the review preflight's former private `segment_of`, moved
  unchanged. The Qt mark (`feedback_body_mark_of`), the cards' header length
  and the conversation model use it. A plain stop stays plain, and the
  heading is never set as a header. The only visible change is that mark on
  a labelled stop. No new text or control was added.

### Acceptance

| Criterion | Status | Evidence |
|---|---|---|
| Issue recognition offers tracking but does not silently create or close defects. | Passing (fixture) | In the `application_label_authoring_contract` course, the file shows three issue roots (the fixture's and two the course published), after several refreshes, and no defect exists. The issue root's thread menu then offers Classify as defect, enabled, and only that gesture creates one defect. It is outstanding (`needs_verification` from the fixture author's reply), and no review was submitted or posted. `application_defect_correlation` shows that an `issue` thread nobody classified never becomes a defect. |
| Website resolution, patch application and relabelling cannot verify or withdraw a private defect. | Passing (fixture; patch application at host level) | In the same course the fixture's website removes the root's `issue` label: the same thread now reads plain, and the defect is unchanged in state, severity and category. The website then resolves the thread: the defect stays `needs_verification`, Verify is still offered on the thread, and nothing was submitted. `application_defect_correlation` covers every starting state with resolution, an applied suggestion's code change under the anchor, and an author reply together: none verifies, withdraws, accepts as is or removes a defect, or changes its classification. `application_defect_composition_contract` (unchanged, re-run) still shows a resolved thread raising only `needs_verification`. |
| Promotion preserves lineage and tours preserve order/links/hooks with no mandatory taxonomy. | Passing | `core_feedback_private_promotion`: a critical security finding from a run that reported its model and effort is promoted through the exact-copy squash, the reviewer-authored squash, a squash of two labelled sources, the note copy and the finding conversion. The draft and its publication proposal carry exactly the accepted bytes. No private metadata appears and no `(blocking)` is derived. The label is the one written; two sources never stack headers. The squash lineage reaches the finding and its run. `core_feedback_tour_coexistence`: plain and labelled stops, the index and an ordinary thread count as five units; the index's links count none; order, ordinals and navigation survive; a label edited into a stop changes only its narrative. `qt_feedback_label_threads` and the course's last phase show the labelled stop's mark and the plain stop's absence of one. `application_writing_delivery` shows no tour hook asks for a label. |
| Owner look at a labelled tour stop's mark | **Open** | It is the existing BPR-1083 mark, now also on a stop's narrative label. The frontend-design skill is not installed on this station, and R33's native matrix has not been run. |

R16 to R19 map onto these rows; the lineage limit on the copy paths is
below.

### Absent or deferred interfaces (recorded, not built)

- **Defect authoring that seeds an `issue` draft** (RFC-BPR-0048, "Defects
  and private notes"). Absent. The live defect workflow classifies existing
  threads only, and nothing here duplicates it.
- **Lineage on the copy paths.** BPR-1210's note copy and RFC-BPR-0012's
  finding conversion copy text by design and record no source link; the
  note or finding stays in the private store unchanged. Squash to Draft
  records lineage.
- **RFC-BPR-0023 is no longer deferred.** It has been Auto-Verified since
  2026-09-27, so its Squash to Draft is a live path and is integrated. The
  intake table's "Notes tab promotion (BPR-1210), Deferred" is also out of
  date: BPR-1210 is Done.
- **Per-comment commit identity** (RFC-BPR-0021): Deferred.
  **Source-bearing owner forwarding** (BPR-1227): Backlog. **Headless**
  defect, promotion or tour-label commands: none (BPR-1092).
- **Native apply-suggestion.** None (BPR-1084). A suggestion applied on
  GitHub reaches BPR only as a code change after a refresh. That is
  `nearby_code_change` evidence, proved at host level; the course does not
  move the fixture's head.

### Open question for the coordinator

RFC-BPR-0048 says "Reading `issue:` offers existing Track as defect
functionality". This card keeps the existing "Classify as defect…" on every
review thread's menu and adds no label-specific offer. Should an `issue`
root also get its own offer?

- (a) Keep it as is (recommended): it is label-independent and already
  explicit, and the owner's selected design adds nothing here.
- (b) Put "Classify as defect…" first in an `issue` root's menu.
- (c) A small icon button with a tooltip on an `issue` root's card.

Linux only; macOS and Windows runs are owed. No live GitHub.
