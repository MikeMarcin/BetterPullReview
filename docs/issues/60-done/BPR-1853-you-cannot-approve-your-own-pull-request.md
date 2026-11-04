---
schema: "implement-rfc.card.v1"
id: "BPR-1853"
title: "On your own pull request the review page offers Comment only"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-03: \"I don't think you should be able to approve your own PR. Doesn't seem like you can on the website anyways. If the approve or comment options still.\" (last sentence cut off)","related_cards":["BPR-1345","BPR-1700","BPR-1715","BPR-1774","BPR-1852"],"id_allocation":"Reserved on main (docs/issues/_reservations/BPR-1853--BPR-1853--c739b677320a.json).","frontend_design":"frontend-design loaded. No new layout: the app's existing idiom for an unavailable verdict (a disabled button whose tooltip and accessible description say why) is reused; Compose review's words read \"Comment…\" on your own pull request"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"99be53a4b","evidence":{"kind":"command","command":"ctest --preset dev -R '^core_review_submission$'","detail":"core::own_pull_request compares logins without ASCII case (MikeMarcin and mikemarcin are one), and an empty author or viewer is nobody's. verdict_open: the author may only Comment. controls(draft, author, viewer): on another's pull request equal to controls(draft); with an unknown author unchanged; on my own SubmitAs is Comment only, Authored is set, Submit is off while the draft holds Approve or Request changes, and a submission in flight can still be checked."}},{"criterion":"AC2","outcome":"pass","revision":"99be53a4b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(live_review_submission_provider|live_review_submission_restart_recovery|github_mutation_contract)$'","detail":"refusal_names_own_pull_request reads GitHub's words (\"Review Can not approve your own pull request\", \"Review Can not request changes on your own pull request\", as a string or an error object's message) and nothing else; they are not an unplaceable line. The production review_submission_session settles GitHub's 422 for the author's own approval as definitely not published, refused_by_server, Refusal own_pull_request, after one POST."}},{"criterion":"AC3","outcome":"pass","revision":"99be53a4b","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_review_submission_editor_scale_(100|125|200)$'","detail":"Given the core's controls for my own pull request, Comment is enabled and Approve and Request changes are shown and off; their tooltips and accessible descriptions say \"You can't approve your own pull request. GitHub lets its author leave only a comment.\" and the Request changes equivalent; a draft holding Approve disables Submit with the same reason."}},{"criterion":"AC4","outcome":"pass","revision":"99be53a4b","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_review_submission_composition_(public_path|scale_125|scale_200)$'","detail":"Composed application over the public-path fixture. With the fixture naming the viewer the author (ViewerAuthors): the page offers Comment only, Approve and Request changes visible and off with their reasons, Compose review reads \"Comment…\" with the reason in its tooltip; clicking Approve or Request changes, or choosing Approve and triggering Submit, sends nothing; Comment submits once as a comment. On another's pull request with GitHub refusing as it refuses an author's approval (RefuseNextReviewAsOwn, through the production session): not published, the page says \"Not published: GitHub doesn't let you approve your own pull request. Your summary is kept; submit it as a comment instead.\" and keeps the summary. Render: review-submission-composition.png-own-pull-request.png in the build tree."}},{"criterion":"AC5","outcome":"pass","revision":"99be53a4b","evidence":{"kind":"command","command":"python3 build/live_course.py (outside the tracked tree) and gh api against MikeMarcin/BPR-Test","detail":"Live, 2026-11-03. As MikeMarcin on its own #43: APPROVE and REQUEST_CHANGES were refused 422 with \"Review Can not approve your own pull request\" and \"Review Can not request changes on your own pull request\" (no review created), and COMMENT was accepted (review 5762773003). As MikeMarcinTest (GH_TOKEN per command) on MikeMarcin's #43: APPROVE was accepted (review 5762781502, dismissed afterwards by MikeMarcin), so a non-author approves. /user gave MikeMarcin and MikeMarcinTest, #43's user.login MikeMarcin: the logins the app compares. X-RateLimit-Remaining stayed at 4994 or above."}},{"criterion":"AC6","outcome":"pass","revision":"99be53a4b","evidence":{"kind":"command","command":"_coding-standard/scripts/format.py --dry-run <changed files>; _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed translation units>; python3 scripts/verify_documentation.py --root .; ctest -R '^(architecture_boundaries|qt_workbench_window_scale_100|qt_compose_review_entry_scale_(100|200)|application_review_submission_(contract|application_host|cli_ordinary))$'","detail":"clang-format 22.1.8 clean; clang-tidy 22 clean on the six changed translation units; documentation gate passes; the listed tests pass."}}]
---

# BPR-1853: On your own pull request the review page offers Comment only

## Goal

GitHub lets a pull request's author submit only a Comment review: Approve
and Request changes are refused ("Can not approve your own pull request").
The review page offered all three verdicts to everyone, so an author could
press Approve and only learn from GitHub's refusal. On your own pull request
the page should offer Comment only, and say why the other two are off. If
GitHub refuses anyway, for example because the author was not known when the
review was sent, the page should say so clearly.

## Design

The core decides, the host performs, Qt presents.

- **Core** (`review_submission.hpp`): `own_pull_request(author, viewer)`
  compares logins with `same_login`, ignoring ASCII case as GitHub does on
  github.com and GHES; an empty login is nobody's. `verdict_open` lets the
  author Comment only. `controls(draft, author, viewer)` turns off Approve and
  Request changes, sets `Authored` (the reason), and turns off Submit while
  the draft holds either verdict. An unknown author changes nothing.
- **Composition**: `review_submission_workflow::observe_author` takes the
  accepted read's `PullRequest.Author`. `decided_controls` compares it with
  the draft binding's `ActingIdentity.Login`, which is the signed-in
  account's viewer login from the same read, and is also the account the
  review is submitted as.
- **GitHub adapter**: `refusal_names_own_pull_request` reads GitHub's 422
  words. The session reports `review_submission_refusal::own_pull_request`,
  and the workflow says "Not published: GitHub doesn't let you approve your
  own pull request" (or request changes on it), keeping the summary and
  comments for a Comment.
- **Qt**: no new layout. The existing idiom for an unavailable verdict
  (disabled, tooltip and accessible description saying why) carries the
  reason. Compose review in the review bar reads "Comment…" and its tooltip
  and accessible text say why.

## Not covered

- In-app live course: a tests-on build composes no live publication ("This
  window is read-only"), so the live evidence is GitHub's own rule through
  `gh api` on BPR-Test, plus the composed fixture course. A MikeMarcinTest
  pull request could not be reopened: #49's head branch on the test
  account's fork is deleted. Creating a new one means writing to
  `MikeMarcinTest/BPR-Test`, a repository other than BPR-Test, so it was not
  done. The non-author approval was shown the other way round instead:
  MikeMarcinTest approved MikeMarcin's #43, and that approval was then
  dismissed.
- The composed fixture course keeps one login per node, so the author and
  viewer are spelled the same there. The case-insensitive comparison is
  covered by the core's tests.
- macOS offscreen only. Windows and Linux were not run.
- No owner look was requested. The brief chose "absent, or disabled with a
  short reason, whichever matches the app's existing idiom", and disabled
  with a tooltip is that idiom, as on GitHub's own page.
- `application_review_round_trip_composition_contract` fails on its pinned
  digest of `github_application.cpp` (`4503eda2…`). That pin did not match
  main's file before this card either, so it was left for its owner.
- BPR-1852 (draft indicator and Ready for review) is in the pull request
  header and Overview; this card touched only the review page, the review
  bar's Compose review words and the review workflow.
