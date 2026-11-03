---
schema: "implement-rfc.card.v1"
id: "BPR-1849"
title: "Requesting a review shows the person in People at once and re-reads nothing"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-03: \"Request a review should immediately update people. Right now there is seemingly a slow whole review refresh.\"","related_cards":["BPR-1804","BPR-1818","BPR-1841"],"id_allocation":"Reserved on main (docs/issues/_reservations/BPR-1849--BPR-1849--ec24ebca4802.json).","frontend_design":"frontend-design loaded. No layout change: a Waiting on row just asked for reads \"asking…\" in faint ink, the picker's own vocabulary, until GitHub answers"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"01850ecca","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_reviewer_request_contract$'","detail":"Composed application over the public-path fixture, github.com and GHES 3.18. From the choice until both answers (a granted request and a refused one) settle: 0 pull request reads (9 before), 0 whole Overview projections (4 before), no refresh in flight (one before), People listed the person pending before the event loop turned again (about 1 ms after the key press; before, never in the fixture and only after the refresh on GitHub), GitHub's answer confirmed the row, and the refused request was withdrawn with the picker saying GitHub refused. The contract requires REVIEWER_REQUEST_MINIMAL_OK and 'reads=0 overviewBuilds=0'."}},{"criterion":"AC2","outcome":"pass","revision":"01850ecca","evidence":{"kind":"command","command":"ctest --preset dev -R '^core_reviewer_request$'","detail":"core::reviewer_request_overlay: a choice is pending at once, survives a read begun before it, is confirmed by an answer that lists the person and withdrawn by one that does not, by a failure or by an unanswered retirement; a confirmed change ends when a read lists the person and is given up at the second read that does not."}},{"criterion":"AC3","outcome":"pass","revision":"01850ecca","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_pull_request_overview|qt_pull_request_overview_page_scale_100)$'","detail":"application::apply_review_requests decides the roster, its note and the status (roles and Waiting on) exactly as project_pull_request_overview with overview_context::ProjectedReviewRequests does. pull_request_overview_page::present_review_requests redraws People, the role line and Needs attention in place: no file-model signal, the participant rows kept, another pull request refused for a whole presentation."}},{"criterion":"AC4","outcome":"pass","revision":"01850ecca","evidence":{"kind":"command","command":"cmake --build --preset dev --target coding-format-check; _coding-standard/scripts/tidy.py --warnings-as-errors --paths <touched translation units>; python3 scripts/verify_documentation.py --root .; ctest -R '^architecture_boundaries$'","detail":"clang-format 22.1.8 clean on every changed file (coding-format-check reports only gh_cli_transport.cpp, untouched here and unformatted on main). clang-tidy 22 reports no finding on a changed line; its four findings (workbench_window.cpp finish_choice, github_application.cpp go_on_to_reviewers, the comment-edit fixture null path, reviewer_request_tests.cpp long_text) predate this card. Documentation gate passes; architecture_boundaries passes. No ctest was added, so network-isolation registration is unchanged."}}]
---

# BPR-1849: Requesting a review shows the person in People at once and re-reads nothing

## Goal

The owner found Request review slow: People changed only after what looked
like a whole review refresh. Choosing a person should show them in People at
once, with GitHub's answer confirming or withdrawing them, and nothing else
should be read or redrawn. Re-requesting a reviewer takes the same path;
the product has no un-request.

## Root causes

Traced end to end: picker gesture (`reviewer_picker`, `pull_request_overview_page`),
composition (`reviewer_gesture`, `request_reviewer`), core decision
(`choose_reviewer`), mutation host and provider
(`review_mutation_session::reviewers`: a binding read, `decide_reviewer_write`,
the `requestReviews` mutation, one read-back only when the outcome is
ambiguous), terminal (`publish_reviewers`), and back to Qt.

1. **A full pull request refresh after every written request.**
   `publish_reviewers` triggered the window's Refresh action whenever GitHub
   answered with a write ("People shows GitHub's answer the way every other
   fact comes back: the next read"). That re-read the whole pull request (9
   paged reads in the fixture, more on a large pull request) and projected the
   Overview again (4 whole projections), with everything a refresh re-adopts.
2. **Nothing showed the request before that read.** The picker knew GitHub's
   answer (its notice said "Requested a review from …"), but People, the
   status's Waiting on and the role line counted only the read's roster.
   Removing the refresh alone would have left them stale.
3. **State that only a re-read refreshed.** The roster's note ("Review
   requests unavailable" or "Code owners and the complete list unavailable"),
   the status's Waiting on list and the viewer's roles all derive from the
   roster, so all three must be decided again with the requested person. The
   picker's own already-requested refusal already followed GitHub's answer
   (`finish_reviewer_request` stores the answer's `Requested`).

## Design

The core decides, the host performs, Qt presents.

- **Core** (`reviewer_request.hpp`): `reviewer_request_overlay` keeps each
  request the viewer asked for: pending from the choice
  (`begin_reviewer_request_change`), confirmed or withdrawn by GitHub's answer
  (`settle_reviewer_request_change`, `abandon_reviewer_request_change`), and
  reconciled by reads (`observe_reviewer_requests`): a read listing the person
  ends the change; the second read that does not is believed, as
  `conversation_overlay` does for a resolve. `project_reviewer_requests`
  gives the requests People adds to the read's.
- **Application** (`pull_request_overview`): `overview_context` takes
  `ProjectedReviewRequests` and a full projection includes them;
  `apply_review_requests` decides the roster, its note and the status again in
  place; `overview_review_request::Pending` marks a row GitHub has not
  answered.
- **Composition** (`github_application.cpp`): `request_reviewer` begins the
  overlay change before asking the host and moves People at once;
  `publish_reviewers` settles or abandons it and moves People again, and no
  longer refreshes. A retired host abandons a request still waiting. Each
  accepted read reconciles the overlay; another pull request drops it.
- **Qt**: `workbench_window::publish_review_requests` and
  `pull_request_overview_page::present_review_requests` redraw the Waiting on
  rows, People's faces and summary, the role line and Needs attention; no file
  row, tree, check or participant is visited. A pending row reads "asking…"
  and is named "asking GitHub for a review".

Safety is unchanged: the provider still binds the request to the designated
pull request, reads before writing, refuses the author, never writes twice,
and confirms an ambiguous outcome with one read.

## Measurement

`--threaded-reply-probe --reviewer-request-probe` over both fixtures, from the
choice of octo-maintainer through the refused request for
fixture-other-reviewer, until both answers and any work they began settled:

| | Before | After |
|---|---|---|
| Pull request reads | 9 | 0 |
| Whole Overview projections | 4 | 0 |
| People moved in place | 0 | 4 |
| Refresh in flight after the answer | yes | no |
| Person in People after the key press | never (fixture), after the refresh (GitHub) | about 1 ms, pending |
| GitHub's answer (fixture) | 11 to 14 ms | 17 to 18 ms, row confirmed |
| Refused request withdrawn with a reason | n/a | yes |

The course prints `REVIEWER_REQUEST_COST:` and `REVIEWER_REQUEST_MINIMAL_OK`.
The "before" row was measured with a temporary switch restoring the refresh
and skipping the overlay, removed before the commit. On the wire a request
still sends what it did: the binding read and the mutation.

## Not covered

- Home's "Waiting on N reviewers" for an authored pull request follows its
  own discovery read; BPR-1848 owns Home's refresh on return, so Home was not
  touched here.
- macOS offscreen only; Windows and Linux were not run. No live BPR-Test
  course: the change removes reads and adds no wire request.
- No owner look was requested: only the pending row's detail word is new.
- `application_review_round_trip_composition_contract` fails on its pinned
  digest of `github_application.cpp`. The pin (`4503eda2…`) already does not
  match main's file at c9d59105c (`8fafcb96…`), so it was failing before this
  card; it was left for whoever owns that pin to renew.
