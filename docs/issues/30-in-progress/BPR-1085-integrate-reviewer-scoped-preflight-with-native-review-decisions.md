---
schema: "implement-rfc.card.v1"
id: "BPR-1085"
title: "Integrate reviewer-scoped preflight with native review decisions"
status: "In Progress"
rfcs: ["RFC-BPR-0048"]
depends_on: ["BPR-1082", "BPR-1083"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-11","milestone":"authoring","acceptance":["R14","R15","R42"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1085: Integrate reviewer-scoped preflight with native review decisions

Pack alias: **PACK-CC-11** (live `BPR-1085`, allocated at import).

## Outcome and work

Add the selected preflight projection to the current review submission flow. Distinguish own published blockers, selected pending comments, advisory label guidance and unavailable coverage. Retain explicit native decisions and the existing provider/submission journal.

## Acceptance

- Comment, Approve and Request changes use their normal exact native event paths.
- Changes to body/selection/head/account invalidate only applicable prepared actions.
- No label or last-resolution event triggers a review verdict; other reviewers are not attributed to the submitter.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation (2026-10-06)

Built on `bpr-main-linux` in `/home/mmarcin/BetterPullReview` on `main`.
Engineering owner `rfc-implementer`, model `claude-opus-5-5`, effort `high`.
Product commits `902d64132` (core) and `281e552ed` (application, Qt,
composition, course and pin reseal). The run ledger's BPR-1085 section has
the full evidence.

- **Core.** `bpr/core/review_preflight.hpp` defines two functions.
  - `project_review_preflight` reads the shared feedback population, its
    annotation set and the selected unsent drafts for the submitter's exact
    provider identity. It returns four separate groups: `Blockers`, `Pending`,
    `Guidance` and `Uncertain`. Other reviewers are counted in `Others` and
    never added to `Blockers`.
  - `prepare_review_decision` and `review_decision_changes` bind one Submit
    gesture to the draft, target, account, disposition, summary and selected
    drafts, and say which of these changed.
- **Application.** `application::observe_review_preflight` reads each
  accepted snapshot once. Conversation identities are the ones Discuss uses.
- **Composition.** The existing `review_submission_workflow` receives the
  observation and the window's unsent comment draft. It presents the
  preflight only for the draft's own account and head. Submit carries the
  decision the editor showed, and the workflow refuses it if anything bound
  has changed. The request, idempotency key, grant, journal, host and
  provider are unchanged.
- **Qt.** Comment, Approve and Request changes are three exclusive buttons,
  as the owner chose in preflight B. The "Your blocking feedback" chip shows
  marks and counts, and opens a list whose rows reveal the item they name.
  Approve carries an amber dot, and its tooltip says approving does not
  resolve my blocking comments. Exact strings are in the
  [delivery contract](../../ux/conventional-comments-feedback.md#review-submission-preflight-bpr-1085-rfc-bpr-0048).

### Acceptance

| Criterion | Status | Evidence |
|---|---|---|
| Comment, Approve and Request changes use their normal exact native event paths. | Passing (fixture) | `application_review_submission_composition_public_path` submits each decision once through the existing host and journal (3 + 3 submissions). Approve with my blocking feedback open sends `approve`. Request changes after inspecting it sends `request_changes` once. |
| Changes to body/selection/head/account invalidate only applicable prepared actions. | Passing | `core_review_preflight` covers the summary, disposition, selected draft (label edit or deselection), head, account, pull request and draft changes. An unselected draft, a resolution or another reviewer's label changes nothing. The course refuses a Submit from a stale selection and from a stale summary, then submits the current view once. |
| No label or last-resolution event triggers a review verdict; other reviewers are not attributed to the submitter. | Passing (fixture) | In the course, the server's label edits, a label changed to blocking, and the resolution of my last blocker submit nothing and keep the chosen Approve. Another reviewer's blocker shows only under "Others". |
| Owner look at the preflight surface | **Open** | It follows existing patterns; the frontend-design skill is not installed on this station. R33's native matrix has not been run. |

Each new check has a failing-before control, listed in the ledger. No live
GitHub was used, and Linux is the only platform run.
