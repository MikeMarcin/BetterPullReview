---
schema: "implement-rfc.card.v1"
id: "BPR-1852"
title: "Say a draft is a draft wherever the pull request is named, and let its author publish it"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","found_by":"Owner, 2026-11-03: \"I can't figure out how to publish my draft PR from the app. I can only barely tell that my PR is a draft from the overview tab\"","reservation":"docs/issues/_reservations/BPR-1852--BPR-1852--22d051ec4732.json","related_cards":["BPR-1774","BPR-1804","BPR-1841","BPR-1849"],"frontend_design":"frontend-design:frontend-design loaded before the Qt surfaces. GitHub's own vernacular: a filled grey Draft pill with a dashed (not yet closed) ring; a faint banner of the same grey under the summary; one default button","product_contract":"Amended: the author's draft state joins the description body and review requests as the metadata the app may change (docs/architecture/product-contract.md#publishing-a-draft)"}
parked_on: "human-review"
acceptance: [{"criterion": "AC1 Core decides who is offered which change (the author, on an open pull request), refuses a gesture that is not the author's, on a closed pull request or for the state it already has, decides the write from GitHub's answer, and keeps the viewer's change over reads until one agrees", "outcome": "pass", "revision": "348f95e13", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_draft_state$'", "detail": "core::draft_state_offered, decide_draft_state_change, decide_draft_state_write (already, then not open, then not permitted), and draft_state_overlay: pending at once, survives an earlier read, GitHub's answer believed until a read agrees or a second read disagrees, a failure falls back to GitHub's last answer rather than a stale read. No Qt."}}, {"criterion": "AC2 Fixed GraphQL documents on both servers and both connections, held to the designated pull request; written once, never for the state it has, a closed pull request or a viewer GitHub refuses", "outcome": "pass", "revision": "20d3fe6d3", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(github_mutation_contract|gh_cli_transport|github_read_contract|live_review_mutation_provider|live_review_submission_provider|application_review_mutation_application_host)$'", "detail": "ObserveDraftState, MarkReadyForReview (markPullRequestReadyForReview), ConvertToDraft (convertPullRequestToDraft) compile to exact bodies naming the accepted read's node, revalidate canonically, refuse a read passed off as a write, the other change or another pull request; GHES 3.18 at /api/graphql. Answers bound to the pull request; an unknown state is malformed; a null payload is refused_by_server, not read back; a lost answer is read back once and never sent twice. gh api graphql launches each with the exact body on stdin. Both mutation names reserved against the read transport. Host: a one-shot grant for the exact change; refused before any effect without the capability, with another session's grant or one issued for a publication; a closed pull request's binding takes none."}}, {"criterion": "AC3 The draft is said wherever the pull request is named: the title row, Overview's title and a banner under the summary; Home rows already show a Draft chip", "outcome": "pass", "revision": "348f95e13", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_pull_request_overview_page|qt_workbench_title_row_scale_(100|125|200)|qt_workbench_window_scale_(100|125|200)|qt_profile_management_window)$'", "detail": "draft_state_chip after the name in the title row (its own toolbar item, measured so the title elides around it) and before Overview's title; the banner says what a draft means; a closed draft is said closed; the summary no longer ends in ' Draft.'. Home's row chip (home_row_delegate) was already there and is unchanged. Offscreen fixture render: the title row and Overview both show the chip."}}, {"criterion": "AC4 The author gets an obvious Ready for review that publishes the draft, and Convert to draft once it is ready", "outcome": "pass", "revision": "348f95e13", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_pull_request_overview_page|application_draft_state_contract)$'", "detail": "Ready for review is the banner's default button and Pull Request > Mark Ready for Review; Convert to draft is in People and Pull Request > Convert to Draft. Offered only to the author on an open pull request in a session whose connection can change it. Pressing either only dispatches."}}, {"criterion": "AC5 After the mutation the window moves in place through the core's overlay, with no re-read and no whole Overview projection; a refusal is withdrawn with words", "outcome": "pass", "revision": "348f95e13", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_draft_state_contract$' --repeat until-fail:5", "detail": "--threaded-reply-probe --draft-state-probe on github-com and ghes-3.18 fixtures: DRAFT_STATE_EVIDENCE:1111 and DRAFT_STATE_COST:reads=0 overviewBuilds=0 inPlace=2 refreshInFlight=0, about 16 to 26 ms from the press to GitHub's answer. The chips go before the event loop turns, the banner says GitHub is being asked, GitHub's answer offers Convert to draft (one write); a refused Convert to draft from the menu shows the chips at once, then withdraws them with 'GitHub did not let you change whether this pull request is a draft. Nothing changed.' beside the button. Five consecutive passes."}}, {"criterion": "AC6 Live on MikeMarcin/BPR-Test", "outcome": "pass", "revision": "af81bc075", "evidence": {"kind": "command", "command": "python3 tests/scripts/draft_live_course.py --tool BUILD/tests/github_contract/reaction_live_course --work DIR", "detail": "2026-11-03, draft pull request #146 opened by the course, through the shipped review_mutation_session over gh api: Ready for review writes=1 draft=0; again writes=0; Convert to draft writes=1 draft=1; Ready for review writes=1 draft=0. MikeMarcinTest (not a collaborator, GH_TOKEN in the child only) asking Convert to draft: refused_by_server writes=0. gh showed #146 ready; closed, branch bpr-draft-20261103-202546 deleted. REST X-RateLimit-Remaining 4999 to 4993. The composed application was not driven live; the native HTTPS connection was not run live."}}, {"criterion": "AC7 Owner look: the draft indicator", "outcome": "pass", "revision": "6f550ad6a", "evidence": {"kind": "observation", "detail": "Owner accepted, attended-review session bf1f6055cfb4c72cac150423c3e722c8 (state /Users/jaeger/.codex/attended-review/bpr1852-v001-6JWPkv/state), candidate BPR-1852-draft-indicator, decision 711ea4a2-aa2e-475c-943b-a4ac08245ba8, 2026-11-04, frozen build bpr-1852-draft-v001 at 6f550ad6a."}}, {"criterion": "AC8 Owner look: Ready for review placement", "outcome": "pending", "revision": "69020e1a4", "evidence": {"kind": "observation", "detail": "v001 (banner with a default button, Convert to draft as a push button in People) rejected by the owner: \"Ugly UX\" (session bf1f6055, decision 9b8fc7b4-ccdf-4573-b9e1-2ef434a88be3). v002 at 69020e1a4: Ready for review ends Overview's title line, no banner, Convert to draft a quiet text button; asked again in a new session with the two directions not taken."}}, {"criterion": "AC9 Owner accepts the product-contract amendment (Publishing a draft, and the narrowed non-goal)", "outcome": "pass", "revision": "44e17fdd9", "evidence": {"kind": "observation", "detail": "Owner accepted, session bf1f6055cfb4c72cac150423c3e722c8, candidate BPR-1852-product-contract-amendment, decision cf1babbb-7c73-4ddd-b262-7a1d35bdcd03. 69020e1a4 changes only the section's placement words (the banner went with the v001 rejection); who may change what is as accepted."}}]
---

# BPR-1852: Say a draft is a draft wherever the pull request is named, and let its author publish it

## Request

The owner, 2026-11-03, verbatim: "I can't figure out how to publish my draft
PR from the app. I can only barely tell that my PR is a draft from the
overview tab"

## What was there

- **No Ready for review anywhere.** No source called
  `markPullRequestReadyForReview` or `convertPullRequestToDraft`, and the
  product contract listed changing pull request metadata other than the
  description body and review requests as a non-goal. The owner could not
  find the action because it did not exist.
- **The draft state was a word at the end of a sentence.** Overview's
  summary ended "… from fixture/x. Draft."; Merge said "Draft" in the Status
  drawer. The title row said nothing. Home's rows already had an outlined
  Draft chip (`home_row_delegate`), as does the review landing model.

## Outcome

- A grey **Draft** chip (`qt::draft_state_chip`: filled pill, dashed ring,
  GitHub's colour vernacular) after the pull request's name in the title row,
  and before its title in Overview, while it is an open draft. The title row
  shows it as soon as GitHub's first answer names the pull request.
- For the author, **Ready for review** ends Overview's title line:
  `[Draft] Title ........ [Ready for review]` (v002; v001's banner under the
  summary was rejected as "Ugly UX"). Directions rendered and not taken: a
  row under the role in the Status drawer, and a lead-edged button in the
  review bar before Approve.
- Pull Request > **Mark Ready for Review** does the same from any modality;
  once ready, it reads **Convert to Draft**, and People has **Convert to
  draft** (where GitHub's own page keeps it).
- Pressing either takes the chips away (or puts them up) at once; the button
  stays, disabled, reading "Marking ready…" until GitHub answers. A refusal
  or failure puts the read's state back and says why in the defect ink.

## Design

The core decides, the host performs, Qt presents.

- **Core** (`bpr/core/draft_state.hpp`): the offer, the gesture's refusal,
  the pre-write decision, and `draft_state_overlay` (pending change,
  GitHub's last answer, disagreement count) with `project_draft_state`.
  `pull_request_change::draft_state` keeps it off closed pull requests.
- **GitHub adapter**: `ObserveDraftState`, `MarkReadyForReview`,
  `ConvertToDraft` (both servers' recorded schemas have the fields and
  mutations); `review_mutation_session::change_draft_state` observes,
  decides, writes once, settles an ambiguous outcome by one read.
- **Application**: port `change_draft_state` and capability
  `ChangesDraftState`; coordinator confirms the target; host admits it under a
  one-shot `issue_draft_state` grant. `overview_context::ProjectedDraft` and
  `apply_draft_state` decide the draft state and the merge assessment in place
  (a draft just made ready is "Merge state unknown" until GitHub reassesses).
- **Composition**: `change_draft_state` and `publish_draft_state`, as
  BPR-1849's reviewer requests: the overlay moves the title row, Overview's
  chip and banner, and Merge in place; nothing re-reads the pull request.
- **Qt**: `workbench_window::publish_draft_state`, `present_strip_draft`,
  `set_draft_state_offer`; `pull_request_overview_page::present_draft_state`.

## Verification

| Check | Result |
|---|---|
| core_draft_state | pass |
| github_mutation_contract, gh_cli_transport, github_read_contract, live_review_mutation_provider, live_review_submission_provider | pass |
| application_review_mutation_application_host, application_pull_request_overview | pass |
| 178 tests matching core_, github_, live_review, gh_cli, application_pull_request_overview, application_review_mutation, architecture | 178/178 pass |
| qt_ (210 tests) | 203 pass before the title-row and menu expectations were updated for the new chip and command; all 7 pass after |
| application_draft_state_contract (github.com, GHES 3.18) | pass, five in a row |
| application_threaded_reply_contract, application_description_edit_contract, application_reviewer_request_contract | pass |
| 125 application contract and public-path tests | 122 pass; application_uiforetw_selected_diff_contract's title-row order then updated for the chip (bbe13b2ba) and passes; application_review_round_trip_composition_contract and application_composition_source_contract fail only on the stale github_application.cpp digest pin below |
| clang-tidy on every touched translation unit | no finding on a changed line after bbe13b2ba (pre-existing: workbench_window.cpp finish_choice, github_application.cpp go_on_to_reviewers, the comment-edit fixture null path) |
| Live BPR-Test #146 | pass |

## Not covered

- Owner look AC8 (Ready for review placement) is pending on v002; v001 was
  rejected ("Ugly UX"). AC7 (the indicator) and AC9 (the contract amendment)
  were accepted.
- Home's row for an authored pull request follows its own discovery read, as
  BPR-1849 left Home's Waiting on: a change made in a review shows on Home
  at its next discovery, not at once.
- The composed application was not driven against live GitHub; the native
  HTTPS connection did not run the mutations live (the gh connection did).
- macOS offscreen only; Windows and Linux were not run.
- `application_review_round_trip_composition_contract` still fails on its
  pinned digest of `github_application.cpp` (`4503eda2…`), which did not
  match main before this card either (BPR-1849 recorded the same); left for
  whoever owns that pin.
- With the viewer named the author (`--description-edit-probe` or
  `--draft-state-probe`) the threaded-reply course prints
  `THREAD_RESOLUTION_MINIMAL_FAILED` (`overviewCounted=0`); the resolution
  contract runs without either probe and passes. Not investigated here.
- `scripts/verify_documentation.py` reports `NEXT_ID: BPR-1853 rewinds
  historical high water BPR-1854`: another branch in the shared repository
  has allocated BPR-1853; main's counter is behind it, not this card's.
