---
schema: "implement-rfc.card.v1"
id: "BPR-1715"
title: "A review GitHub refused gives its drafts back instead of waiting for ever"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-1345", "BPR-1540", "BPR-1541"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1715--BPR-1716--d95c8c662f30.json","kind":"bug","origin":"owner report 2026-10-21 (relayed by the coordinator): a review with 9 pending comments stuck on 'We couldn't confirm this review' after a mid-review push and refresh, on 0.3.1","governing_rfc":"RFC-BPR-0008","related_cards":["BPR-1345","BPR-1538","BPR-1540","BPR-1541","BPR-1572","BPR-1520","BPR-1706"],"related_rfcs":["RFC-BPR-0022"],"follow_up":"an off-hunk comment carried in a review: mark it before Submit and offer RFC-BPR-0022's file comment with a permalink, which needs an admitted unsupported-response contract for the host","policy_decision":"A POST with no definite answer settles as not published ten minutes after its answer was recorded when no review GitHub lists can be it","frontend_design":"frontend-design:frontend-design loaded before the frozen review's status sentence"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7a00adb52f346629f7dfc5073bac4ec479f93984","evidence":{"kind":"command","command":"ctest --preset dev -R '^(live_review_submission_provider|core_review_submission)$'","detail":"live_review_submission_provider row refused_and_lost_reviews_settle: GitHub's 422 in the GitHub CLI transport's shape (an answer with status 422) is definitely not published, refused_by_server, one POST, and the attempt is withdrawn; a single comment's new review of mine with the same empty summary but one comment of two does not hold it back. core_review_submission test_post_answers_settle covers the rule. Failing-before control: with the session reading a non-2xx answer as no answer (as 0.3.1 did) the row fails at 'the GitHub CLI's 422, with no new review of mine after it, is a definite refusal by the server'."}},{"criterion":"AC2","outcome":"pass","revision":"7a00adb52f346629f7dfc5073bac4ec479f93984","evidence":{"kind":"command","command":"ctest --preset dev -R '^(sqlite_review_submission_recovery_storage|live_review_submission_provider|live_review_submission_restart_recovery|application_review_submission_composition_public_path)$'","detail":"Schema 7 keeps the POST's answer with the attempt; a version 6 journal migrates keeping the attempt exactly, with 'no answer since time zero'. The composition course writes nine pending comments, has the fixture drop the review with no answer, rolls the journal back to version 6 (DROP TABLE submission_post, user_version 6), and restarts: the restored attempt is checked, settled as not published with no POST, and all nine comments come back pending with their exact text; Submit sends one review and GitHub holds each comment once. Failing-before control: with core::definitely_unpublished settling only a 4xx answer, the course fails at 'after the upgrade the restored attempt is settled and all nine comments come back unlocked, with their text'. The contract row also covers a record with no answer, a new review of mine with a null body (stays unknown), a POST with no answer before and after the ten-minute delay, and a success answer a day later (stays unknown)."}},{"criterion":"AC3","outcome":"pass","revision":"7a00adb52f346629f7dfc5073bac4ec479f93984","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_submission_composition_public_path|qt_review_submission_editor_scale_(100|125|200))$'","detail":"The owner's course on the fixture: a comment and a summary, a push and Refresh, another comment, publish. The push freezes the review; it carries no comment of the new revision, its Discard is the summary's alone ('Discard your review summary?'), and the status says it deletes the summary, never a comment. Saved comments keeps both comments with their text. The new review carries the comment written after the push; the GitHub CLI's 422 releases it ('not published', no Check status); publishing again sends it once and nothing else. While a submission may hold its comments no pending row offers Delete, and a Delete asked for opens no question. After the upgrade course's nine comments come back, GitHub's refusal of the review in its own words (a line that must be part of the diff) keeps all nine and the page says a comment is on a line GitHub can't place and to delete or edit it; Delete on the off-hunk comment's row (asked first) leaves eight, and Submit publishes one review holding each of the eight once and not the deleted one. live_review_submission_provider reads that refusal as unplaceable_line in GitHub's review and single-comment forms, the native shape as the review refused, and other 422 bodies as neither. Failing-before control: with the workflow and editor as on main, the course fails at 'the frozen review takes no comment of the new revision' and the editor at 'the rows of a review awaiting GitHub's answer offer no Delete'."}},{"criterion":"AC4","outcome":"pass","revision":"7a00adb52f346629f7dfc5073bac4ec479f93984","evidence":{"kind":"command","command":"ctest --preset dev -R '<focused suite>'; python3 _coding-standard/scripts/format.py --dry-run <changed files>; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"26 of 26 focused tests pass on 7a00adb52f346629f7dfc5073bac4ec479f93984 rebased onto main ad6908848: core_review_submission, application_review_submission_contract, application_review_submission_application_host, sqlite_review_submission_recovery_storage, qt_review_submission_editor_scale_100/125/200, live_review_submission_provider, live_review_submission_restart_recovery, application_label_publication_recovery, application_review_submission_composition_public_path and _scale_125/_200, the nine application_review_tour_publication_* rows, application_review_submission_cli_ordinary, architecture_boundaries, command_correctness_registry_contract and application_review_round_trip_composition_contract. clang-format 22 dry run passes on the changed files; clang-tidy (LLVM 22) with warnings as errors passes on every changed unit. tests/command_correctness_registry.json reseals review_submission_composition_public_path.cpp, review_submission_fixture.hpp and github_application.cpp; the round-trip contract reseals full_application (-> 80d792ab9ed8) and production_app_manifest (-> 0b9e10d9308c) with a note at its top, and mutation 16d135419 stays exact. Both documentation gates pass with the card under a stand-in allocated ID; with the provisional ID the gate refuses only the card ID format. macOS, the dev preset built -O0 -g0."}}]
---

# BPR-1715: A review GitHub refused gives its drafts back instead of waiting for ever

## Owner report

The owner, 2026-10-21, verbatim:

> There's a big problem. I have a PR I'm commenting on I have a review with 9
> substantial draft comments ready to go and I can't publish it. It's stuck on
> we couldn't confirm this review check status. The pr did get updated in the
> middle of my review and I had to refresh it in order to add new comments but
> I had already left comments. Not sure if that is related.

The page read "We couldn't confirm this review", then "GitHub hasn't listed
this review yet. Check status again in a moment.", with Check status and nine
pending comments, each with a Delete icon. The build was 0.3.1. Its review
submission code is identical to 0.3.0's.

## Cause

A whole review ([[BPR-1345]]) is one POST. [[BPR-1345]] treats GitHub's 422,
with no new exact review listed after it, as a definite refusal: the attempt
is withdrawn and its drafts are released. The session recognised a 422 only in
the native transport's shape, an error carrying the status. The GitHub CLI
transport reports an HTTP error status as an answer with that status, so
through `gh` the refusal fell through to the reconciliation. That found no
review and left the attempt unknown. Check status only looks for a review to
confirm, so it could never end the attempt. The drafts stayed held and the
page had no way out.

Two further defects sat on the same path after a mid-review push:

- A review written before the push is frozen on its revision. It still took
  the new revision's pending comments, so its Discard (then "Discard review")
  deleted every pending draft of the pull request, including ones written on
  an earlier revision.
- The pending rows of a review awaiting confirmation offered Delete. It
  deleted nothing, because a held draft is not pending at the row's revision,
  but it asked "This cannot be undone." first.

## Change

- **One shape for GitHub's answer.** The session reads the POST's status from
  either transport alike: an answer's status, or an error's.
- **The answer is kept with the attempt.** The recovery journal moves to
  schema 7. The new `submission_post` table holds the answer (success, an
  error status's class, or none) and when it was recorded. The transmission
  witness writes "no answer" before the POST may leave and refuses the POST if
  it cannot, so an attempt with no answer kept never left. Migration from
  schema 6 gives every possibly sent unresolved attempt "no answer" at time
  zero.
- **A settle rule in the core.** `core::definitely_unpublished` decides, from
  the kept answer and an explicit clock, whether an attempt that no listed
  review can be was definitely not published:
  - a 4xx answer: at once (GitHub creates a review whole or not at all);
  - no answer kept: at once (it never left);
  - no definite answer (none, a redirect, a server error): once ten minutes
    have passed since it was recorded;
  - a success answer: never.

  The session applies it on Submit, on Check status and on restart.
  "No listed review can be it" means GitHub's complete review list holds no
  new exact review of mine that could be it. An exact review that holds
  another number of comments than the attempt carried is not it. A new
  review of mine with no body, commit or author, or one dismissed, could be
  it. A settled attempt is withdrawn, and the workflow releases its drafts
  unchanged.
- **After a push.** A review frozen on an earlier revision takes no comment of
  the current one. A frozen review that keeps nothing (no summary, the default
  verdict) is replaced by a new review on the current revision. Discard review
  deletes only the drafts the review carries, at the revisions and head it
  carries them. A refused review carries what is pending now, and its released
  drafts return as the answer releases them. The frozen status now reads "The
  pull request revision changed after you wrote this summary. Discard starts a
  new review on the current revision; it deletes the summary, never a comment."
- **Held rows offer no Delete** while a submission may hold them.

## The off-hunk comment and GitHub Enterprise Server 3.20

One of the owner's nine comments is on a line outside every diff hunk, on
GitHub Enterprise Server 3.20.8. That is very likely the 422. The public REST
contract defines a review comment's `line` as a line in the pull request's
diff. GitHub's newer comment-anywhere interface is a web feature with limited
API support. GitHub Enterprise Server 3.21 withdrew its release-note claim for
it as premature, and 3.20 is older. The one recorded observation
(the RFC-BPR-0022 ledger) is GitHub.com refusing a single off-hunk line comment
through this application's REST write with 422, `pull_request_review_thread.line`
"could not be resolved". Native off-hunk success has never been observed.
GitHub Enterprise Server itself was not observed. Both the review endpoint and
a single comment almost certainly refuse such a line there. As the
workbench specification says, production fallback to a file comment stays
unavailable until an exact unsupported-response contract is admitted, and the
application infers nothing from a host's name or version.

So this card changes what the owner sees, not how such a line is published:

- When GitHub's refusal of a review says a line must be part of the diff,
  could not be resolved, or has a blank diff hunk, the page says one of the
  review's comments is on a line GitHub can't place, such as a line outside
  the changes. GitHub doesn't say which, so the page doesn't either. All
  comments are kept; the reviewer deletes or edits that one and submits
  again. Any other refusal says the review was refused.
- An attempt settled on Check status or restart with no refusal words kept
  says it was not published, with the summary and comments kept.

Marking a carried off-hunk comment before Submit, and offering
[[RFC-BPR-0022]]'s file comment with a permalink for it (as [[BPR-1706]] does for
a commit-filtered line), needs an admitted unsupported-response contract for
the host. It is recorded as a follow-up, not guessed here.

## Policy decision: the ten-minute "no definite answer" rule

Before this card, an attempt whose POST had no definite answer stayed unknown
until a review appeared, possibly for ever. This card decides that a POST with
no definite answer is settled as not published when **ten minutes** have
passed since its answer was recorded and GitHub's complete review list holds
no review that could be it. The reviewer's drafts then come back to be
submitted again.

It cannot cause a duplicate publish:

- **Nothing is sent by settling.** Settling withdraws the record and releases
  the drafts. Only a new Submit from the reviewer sends again, as a new
  attempt with its own key and baseline. The core never retries.
- **A review GitHub created is found first.** Settling is considered only
  after the reconciliation has read the complete review list (one page,
  refused when paged or too large) and found no new review that could be this
  attempt. A review GitHub created for this POST has the attempt's summary,
  verdict, commit and author, and holds exactly its comments, so it is
  confirmed as published, not settled.
- **Ambiguous reviews hold it.** A new review of mine with no body, commit or
  author, a dismissed one, or an exact one holding as many comments as the
  attempt carried, keeps it unknown.
- **The delay covers the server finishing late.** A POST cut off in flight
  could still be completed by GitHub. Ten minutes is far longer than GitHub
  takes to finish and list a review it accepted. A clock earlier than the
  record waits.
- **A success answer never settles.** If GitHub said it created the review,
  only the reviewer looking at GitHub can say which review it is.

## Acceptance

- **AC1.** GitHub's 422 through the GitHub CLI is a definite refusal: the
  attempt is withdrawn and its drafts are released. The case fails before the
  change.
- **AC2.** An attempt an earlier version left unknown with nine held comments
  is settled on the upgraded application's first check, and all nine come back
  pending with their text. Publishing then sends one review. The rule's other
  cases are covered by contract tests. The course fails before the change.
- **AC3.** The owner's course (comment, push, refresh, comment, publish)
  publishes the current revision's comment once. No Discard deletes a comment
  written on another revision, and held rows offer no Delete. When GitHub
  refuses a review because a line cannot be placed in the diff, the page says
  so and keeps every comment, and deleting that comment and submitting again
  publishes the rest once. The cases fail before the change.
- **AC4.** The focused review submission, tour publication, recovery and
  composition suites pass, along with format, tidy, the documentation gates
  and the resealed pins.

## Follow-up — 2026-10-21: the source-line journal's storage test

`sqlite_review_mutation_recovery_storage` failed on main after this card. The
recovery database holds both journals, and its schema lives in
`review_mutation_recovery_store.cpp`, so the move to version 7 is that store's
change too. The test still expected version 6, its version 3 and version 2
fixtures kept the new `submission_post` table, and its "future" version was 7.
The migration was right; only the test was stale. It now expects version 7,
drops `submission_post` when it rolls a journal back, refuses version 8 as the
future, and covers 6 to 7: a possibly sent comment and a logical publication
read back exactly, and no source-line attempt gains a POST answer.

Why the verification missed it: AC2 and AC4 chose the suites by the feature's
name and ran `sqlite_review_submission_recovery_storage` only. [[BPR-1540]]'s
move to version 6 had updated both storage tests. A change to this store's
schema needs both.

## Follow-up — 2026-10-21: the start-review course's refusal sentence

`application_start_review_contract` failed on main after this card. Its
`--start-review-course` probe in `github_application.cpp` still expected a
refused review to say "Review was not published.", the sentence from before
this card. The fixture's refusal is a 422 for the whole review with no words
about a line, so the workflow now says "Not published: GitHub refused this
review. Your summary and comments are kept; check them and submit again.",
as the Change section above intends ("Any other refusal says the review was
refused"). The product was right; only the probe was stale. It now expects
that sentence, and still requires the refusal to keep the page open with the
comment's text, line and no submission. The full-application and manifest
pins and the command-correctness pins were resealed; mutation 16d135419
stays exact. Offscreen, `application_start_review_contract`, the thirteen
`review_submission` suites, `application_review_round_trip_composition_contract`
and `command_correctness_registry_contract` pass; so do `coding-format-check`
(clang-format 22.1.8), clang-tidy on `github_application.cpp` and both
documentation gates.

Why the verification missed it: AC4 ran the review submission and
composition suites, but not the start-review course, which drives the same
refusal through the composed application.
