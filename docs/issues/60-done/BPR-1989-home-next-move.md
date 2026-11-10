---
schema: "implement-rfc.card.v1"
id: "BPR-1989"
title: "Decide and show whose move an authored pull request is"
status: "Done"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0001-home-next-move.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0098 AC01","outcome":"pass","revision":"5018fe6aa336d553b52f075ee32a27e8ddb9057f","evidence":{"kind":"command","detail":"One next move per row of the table","command":"ctest --preset dev -R core_home_next_move"}},{"criterion":"RFC-BPR-0098 AC02","outcome":"pass","revision":"5018fe6aa336d553b52f075ee32a27e8ddb9057f","evidence":{"kind":"command","detail":"Placement and order","command":"ctest --preset dev -R 'core_home_next_move|application_home_next_move_public_path'"}},{"criterion":"RFC-BPR-0098 AC03","outcome":"pass","revision":"5018fe6aa336d553b52f075ee32a27e8ddb9057f","evidence":{"kind":"command","detail":"Word, sentence, tone, header","command":"ctest --preset dev -R 'application_home_next_move_public_path|qt_review_landing_model'"}},{"criterion":"RFC-BPR-0098 AC04","outcome":"pass","revision":"5018fe6aa336d553b52f075ee32a27e8ddb9057f","evidence":{"kind":"command","detail":"Absent facts claim nothing","command":"ctest --preset dev -R 'core_home_next_move|application_review_queue_enrollment|qt_review_landing_model'"}},{"criterion":"RFC-BPR-0098 AC05","outcome":"pass","revision":"5018fe6aa336d553b52f075ee32a27e8ddb9057f","evidence":{"kind":"command","detail":"Bounded reads, no extra request","command":"ctest --preset dev -R 'application_home_next_move_public_path|github_paged_read'"}},{"criterion":"RFC-BPR-0098 AC06","outcome":"pass","revision":"2f772b6f00b3be54d27e4b5462815332977e955b","evidence":{"kind":"observation","detail":"Owner accepted BPR-1989-home-moves (23a80fed), BPR-1989-own-pull-requests (cae206d9) and BPR-1989-word-place (0b371105), attended session bb8c24c5f3cb7ed97777f4e3f1a70a78, no notes"}}]
---

# Decide and show whose move an authored pull request is

## Scope and acceptance

Implements
[RFC-BPR-0098](../../rfcs/RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md).

Carry the review decision, merge state and reviewers' latest states from the
existing Home read into queue facts, add the bounded conversations-awaiting-
reply read for authored pull requests, decide the next move in the core by the
RFC's table, and present the reason word, sentence, tone, order and section
summary on Home.

Covers AC01 to AC05. AC02's placement is settled: authored rows stay in
My pull requests and Waiting on others, with no new section (owner,
2026-11-10). AC06 is the owner's
look on a frozen build. Load frontend-design before the Qt work.

This is part of `home-next-move` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Evidence

Product candidate `5018fe6aa`; record
[RFC-BPR-0098 verification](../../verification/RFC-BPR-0098.md).
Frontend-design was loaded before the Qt work.

- AC01: `core_home_next_move` — pass.
- AC02: `core_home_next_move`, `application_home_next_move_public_path` — pass.
- AC03: `application_home_next_move_public_path`, `qt_review_landing_model` — pass.
- AC04: `core_home_next_move`, `application_review_queue_enrollment`,
  `qt_review_landing_model` — pass.
- AC05: `application_home_next_move_public_path` (a refresh is its two
  searches and two batched queries), `github_paged_read` — pass.
- AC06: the owner's look — pending. Filed, not waited on (owner,
  2026-11-10: "Continue working unattended, you can file reviews but dont
  block on their results"): attended session
  `bb8c24c5f3cb7ed97777f4e3f1a70a78`, state
  `/Users/jaeger/.attended-review-state/home-next-move-v001-2026-11-10/state`,
  cursor 9, three candidates: `BPR-1989-home-moves` (the fixture rows, the
  frozen composed course of `2f772b6f0` in
  `/Users/jaeger/home-next-move-v003.ozia7rab`, launched with `--attended`),
  `BPR-1989-word-place` (the word after the sentence, as every Home row has
  it, or leading, as the design round drew it) and
  `BPR-1989-own-pull-requests` (a tests-off live build, product as
  `2f772b6f0`, against the owner's own open pull requests; refrozen from
  `abba53926` in `/Users/jaeger/home-next-move-live-v002.s9ny7rtj` after the
  first live build could not start).
