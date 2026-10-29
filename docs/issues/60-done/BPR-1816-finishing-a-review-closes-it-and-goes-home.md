---
schema: "implement-rfc.card.v1"
id: "BPR-1816"
title: "Finishing a review closes it and goes Home"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1816--BPR-1816--93095fe3c075.json","kind":"ux","found_by":"owner request, 2026-10-29","related_cards":["BPR-1345","BPR-1573","BPR-1597"],"frontend_design":"frontend-design:frontend-design loaded before the Settings surface"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "e4595da51357e9782dc2d6d4d17bb84bf42c0f97", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_review_submission$'", "detail": "test_finish_transition: every verdict defaults to Home; a published answer to the reviewer's own Submit closes the review and goes Home; a not-published or unknown answer keeps the review, from Submit or recovery; a published recovery (Check status, reopening) and a published answer while another submission awaits its answer return to the pull request; unticking one verdict changes only that verdict; an unknown stored choice or verdict never leaves the pull request."}}, {"criterion": "AC2", "outcome": "pass", "revision": "e4595da51357e9782dc2d6d4d17bb84bf42c0f97", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^sqlite_workbench_settings_storage$'", "detail": "test_finish_review_migration_and_round_trip: a version-23 store migrates to schema 24 with every verdict going Home and every other choice kept; each verdict's choice and combinations round-trip; an unknown choice cannot be written; a stored finish_review of 8 is refused. Every older legacy fixture drops the new column and still migrates."}}, {"criterion": "AC3", "outcome": "pass", "revision": "e4595da51357e9782dc2d6d4d17bb84bf42c0f97", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_review_submission_editor_scale_(100|125|200)|qt_center_ribbon_width_scale_(1.0|1.25|2.0))$'", "detail": "The editor's confirmation carries the core's transition to its observer. test_finish_review_settings_follow_the_projection: Settings › Review › Finishing a review has one row, 'Close and go Home after', with Comment, Approve and Request changes boxes, ticked by default, named 'Close the review and go Home after …', StrongFocus, an information glyph and its own Reset; unticking Approve dispatches the whole record with only Approve staying; the boxes follow the published record without dispatching; Reset ticks all three."}}, {"criterion": "AC4", "outcome": "pass", "revision": "e4595da51357e9782dc2d6d4d17bb84bf42c0f97", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_review_submission_composition_public_path'", "detail": "Through the composed application and the fixture server: with defaults, a review GitHub refuses stays on its sheet with its summary and verdict and nothing goes Home; resubmitted and confirmed, the sheet closes, the review closes and Home shows, saying 'Review submitted. Closed the review of octo-org/review-repository#17.'; reopened with Comment unticked, a confirmed Comment returns to the pull request, and a confirmed Request changes still goes Home; one submission per gesture. Earlier courses that submit several reviews on one pull request untick all three through Settings first; Check status and reopening confirmations stay on the pull request."}}, {"criterion": "AC5", "outcome": "pass", "revision": "e4595da51357e9782dc2d6d4d17bb84bf42c0f97", "evidence": {"kind": "artifact", "detail": "With BPR_FINISH_REVIEW_RENDER_DIR set, qt_workbench_window --center-ribbon-width writes finish-review-settings-light.png and finish-review-settings-dark.png of Settings › Review at its defaults; copies are outside the repository."}}]
---

# BPR-1816: Finishing a review closes it and goes Home

## Request

The owner, 2026-10-29:

> Finishing a review should take you to Home and close that review. Add
> Settings options for this transition, defaulted on. Keep a separate option
> for each outcome: finishing with Comment, and finishing with Approve. Decide
> whether Request changes gets its own option or shares one, and justify the
> choice. My suggestion: also give it its own option, defaulted on.

## Outcome

- Once GitHub confirms a review submitted with Comment, Approve or Request
  changes, the Finish your review sheet closes, the review closes as Close
  review closes it (drafts kept), Home shows with its Open bar focused, and
  the status bar says "Review submitted. Closed the review of owner/name#N."
- Settings › Review › Finishing a review: one line, "Close and go Home
  after", with a box per verdict, each ticked by default and each its own
  preference, an information glyph and a Reset. Unticked, a confirmed review
  returns to the pull request as [[BPR-1345]] v003 did.
- Nothing leaves the pull request on a refused or unknown answer (the page
  keeps its summary, verdict and comments), on a confirmation found by Check
  status or on reopening, or while another of the pull request's submissions
  still awaits its answer.

## Decision: Request changes has its own box

Request changes gets its own option, on by default, as the owner suggested.
The three finish buttons are three separate gestures, and each verdict leaves
a reviewer somewhere different: after Approve there is usually nothing more to
do on the pull request, while a reviewer asking for changes may want to stay
and watch for the author's answer or add a note. Sharing Comment's box would
make one choice silently change another button's behaviour, and the three
boxes cost one Settings line.

## Design

Loaded `frontend-design:frontend-design`. The group follows the Review page's
existing grammar (heading with its own Reset, 180-pixel name column, quiet
controls, information glyph) and sits between Reading and Layout, in a
review's own order: open, read, finish. The one line reads as a sentence, and
its boxes carry exactly the finish buttons' names, so the setting and the
gesture it governs share vocabulary.

## Built

- **Core.** `core::review_finish_settings` (`review_finish_return` per
  verdict, Home by default), `finish_return`, `valid`, and
  `decide_review_finish`: a typed `review_finish_transition`
  (`keep_review`, `return_to_pull_request`, `close_review_and_go_home`) from
  the verdict, GitHub's answer (`published`, `not_published`, `unknown`), its
  origin (`submit`, `recovery`) and whether another submission is unsettled.
- **Application settings.** `review_behaviour_settings::FinishReview`,
  validated with the record. Workbench settings schema 24 adds
  `finish_review` to the profile row (one bit per verdict that stays; zero,
  all Home, is the default and what a migrated row gets).
- **Host.** `review_submission_workflow` remembers the key its own Submit
  sent, asks the core on each answer with the profile's accepted choices
  (`set_finish_settings`, composed in `github_application`), and passes the
  transition with `confirm_submitted`.
- **Qt.** The window's submitted observer closes the sheet as before; for
  `close_review_and_go_home` it shows Home and releases the review through
  the existing Close review dispatch on the next event-loop turn (the
  confirming workflow is still on the stack), only if the same review is
  still open. `release_review` is factored out of `close_review` unchanged.
- Docs: `docs/ux/workbench-specification.md`,
  `docs/development/settings-authority-inventory.md`.

## Not verified

- macOS offscreen only; Windows and Linux were not run.
- No owner look at the Settings line was asked for; the renders are offered
  with the handoff.

## Owner review

On 2026-10-29 the owner accepted the look from offscreen screenshots (no frozen build was requested).
