---
schema: "implement-rfc.card.v1"
id: "BPR-2249"
title: "Seen in the review closes the pull request the way Approve does"
status: "Done"
rfcs: []
depends_on: []
acceptance: [{"criterion":"BPR-2249:same-setting","outcome":"pass","revision":"c392ed816","evidence":{"kind":"command","detail":"Seen follows Approve's finish choice: Home by default, the pull request when unticked","command":"ctest --preset dev -R '^(core_review_submission|application_review_seen_public_path|application_review_seen_stays_public_path)$'"}},{"criterion":"BPR-2249:after-recorded","outcome":"pass","revision":"c392ed816","evidence":{"kind":"command","detail":"the queue answers only after the store; refused and unstored Seen keep the review","command":"ctest --preset dev -R '^(application_review_queue_enrollment|application_review_seen_fails_public_path)$'"}},{"criterion":"BPR-2249:failure-stays","outcome":"pass","revision":"c392ed816","evidence":{"kind":"command","detail":"a store failure keeps the review open, says so, withdraws the seen head; the row stays in Needs me","command":"ctest --preset dev -R '^(application_review_queue_enrollment|application_review_seen_fails_public_path)$'"}},{"criterion":"BPR-2249:same-notice","outcome":"pass","revision":"c392ed816","evidence":{"kind":"command","detail":"one leave_review_page path for submitted reviews and Seen; status reads '... Closed the review of octo-org/review-repository#17.'","command":"ctest --preset dev -R '^(application_review_seen_public_path|application_review_submission_composition_public_path)$'"}}]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"change","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner request, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2249--BPR-2249--3017b2614c30.json","related":["BPR-1816","BPR-2049","BPR-2217"],"acceptance":["BPR-2249:same-setting","BPR-2249:after-recorded","BPR-2249:failure-stays","BPR-2249:same-notice"]}
---

# BPR-2249: Seen in the review closes the pull request the way Approve does

## Report

The owner, 2026-11-17: "Seen in the compose review menu should close the PR
same way approve does"

[[BPR-2217]] put Seen beside Approve in the review. Approve, once GitHub
confirms it, closes the review page and, by Settings › Review › Finishing a
review ([[BPR-1816]]), closes the review and shows Home. Seen recorded the
new commits as seen and left the review page open.

## Scope and acceptance

- **BPR-2249:same-setting.** Seen follows Approve's own finish choice: ticked
  (the default), the review closes and Home shows; unticked, the review page
  closes and the pull request stays open. The core decides the transition.
- **BPR-2249:after-recorded.** It closes only once the queue has recorded the
  seen head (the store answered); a refused Seen (head moved, no longer
  offered) or a store that fails keeps the review page open.
- **BPR-2249:failure-stays.** On failure the review stays open, the
  unstored seen head is withdrawn (the core offers Seen again) and the status
  line says the commits were not marked seen.
- **BPR-2249:same-notice.** The notice is Approve's shape: the status line
  and a polite accessible announcement, with "Closed the review of …"
  appended when the review closes. Approve has no undo, so neither does Seen.

The Pull Request menu's Mark New Commits Seen is not the review's Seen and is
unchanged.

## Intended verification

A core test of the Seen transition per setting; an application test of the
queue host's answer (recorded, refused, store failure withdrawing the seen
head); the composed `--seen` course closing to Home on the default setting,
and a new course where the store fails and the review stays open.

## Evidence (2026-11-17)

Lane commit `c392ed816`.

- Core: `core::decide_seen_finish(settings, recorded)` is
  `decide_review_finish` for a confirmed Approve, or `keep_review` when Seen
  was not recorded (`review_submission.hpp`).
- Application: `review_queue_application_host::apply(queue_seen_intent,
  queue_seen_answer)` answers `recorded` only after the store kept the seen
  head, `refused` at once (closing, row gone, head moved, Seen not offered),
  and `not_kept` when the store fails, withdrawing the unstored seen head.
- Composition: the window's Seen dispatch reads Approve's accepted finish
  choice when the queue answers, as the submission workflow does, and asks
  the core for the transition.
- Qt: the submitted-review observer and Seen share
  `workbench_window::leave_review_page` (close the page, status line,
  announcement, close the review and go Home on the next turn). Seen's
  answer closes only the review it was pressed in (identity line). A
  `keep_review` answer leaves the review open and says "The new commits were
  not marked seen. The review stays open; try Seen again." The Pull Request
  menu's Mark New Commits Seen is unchanged.

Verification, `ctest --preset dev -j4 -R …`, all pass:
`core_review_submission` (test_seen_finish_transition),
`application_review_queue_enrollment` (test_seen_answers_after_the_store),
`application_review_seen_public_path` (closes to Home, says both),
`application_review_seen_stays_public_path` (Approve unticked: page closes,
pull request stays), `application_review_seen_fails_public_path` (store
refuses: review stays open, says so, row stays in Needs me, nothing asked
of GitHub), `application_home_approve_again_public_path`,
`application_home_oversized_public_path`,
`application_review_submission_composition_public_path`,
`architecture_boundaries`. Full `cmake --build --preset dev` passes; format
check passes on the changed files; `verify_documentation.py` passes.

Limitation: a failed queue store marks the whole queue unavailable (existing
behaviour), and the window offers no queue actions, Seen included, until a
store succeeds again; the course therefore checks the withdrawn head through
the row staying in Needs me rather than through Seen reappearing.
