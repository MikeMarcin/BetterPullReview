---
schema: "implement-rfc.card.v1"
id: "BPR-1718"
title: "My review verdict on Home rows"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1718--BPR-1718--978436825a33.json","kind":"ux","origin":"owner request 2026-10-21","owner_note":"On the home page I would like to know if I have approved a PR. Right now they go to following and I don't really know that I took action.","related_cards":["BPR-1379","BPR-1407","BPR-1680","BPR-1683"],"related_rfcs":["RFC-BPR-0026","RFC-BPR-0066","RFC-BPR-0067"],"frontend_design":"frontend-design:frontend-design loaded before the chip's design, marks and wording"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f882beaf3d044a069013f583a23bcd1eb085117b","evidence":{"kind":"command","command":"ctest --preset dev -R '^core_review_queue_attention$'; ctest --preset core-only","detail":"core_review_queue_attention test_my_standing_review_verdict: no review is no verdict; an approval on the head is approved with its time and no new commits; an approval of an earlier head has new commits; an unknown commit says nothing; a later comment keeps the approval standing with the approval's own staleness; a change request replaces a comment; the latest verdict wins either way; comments alone are commented and can be overtaken; a dismissal after an approval is dismissed, never approved, with no staleness; a comment after a dismissal is commented; a comment before a dismissal does not outlast it; approving again after a dismissal is approved; every verdict has its own name. Failing-before control: with the behaviour sources (personal_discovery_host, review_queue_enrollment, review_queue_facts, github_paged_read, github_read_contract, github_public_path_fixture, review_landing_model, home_row_delegate) at the parent and core::my_review_for returning no verdict, plus bare home_verdict_text, home_verdict_sentence, verdict_rect and helpEvent so the tests link, every course below fails at its first verdict expectation: core_review_queue_attention 'an approval on the current head is approved, with its time', application_review_queue_enrollment 'my approval stands through my later comment...', github_paged_read 'a REST review's submitted_at is when it was submitted', qt_review_landing_model 'the chip's words: Approved', qt_home_list_scale_100 'the row's chip says Approved · new commits (720 wide)' and application_personal_discovery_public_path 'the approval and when it was submitted reach both windows' rows'."}},{"criterion":"AC2","outcome":"pass","revision":"f882beaf3d044a069013f583a23bcd1eb085117b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(github_paged_read|application_review_queue_enrollment|sqlite_queue_enrollment_storage)$'","detail":"github_paged_read: the batched query asks 'nodes{databaseId state submittedAt author'; the approval's submittedAt 2026-10-14T16:20:00Z reads as 1791994800 and a pending review's null as none; a review node without submittedAt or with '14 October 2026' is not the answer asked for and the batch is read the REST way (two GetPullRequest); the REST read's submitted_at reads the same and a review without one has none. application_review_queue_enrollment: a retained read with my comment, my approval, another reviewer's change request and my pending review gives approved at the approval's time, in Following; a moved head gives new commits and the row is Changed as before; an open (Queue.refresh) keeps the verdict; the record and the codec carry it as version 3; a dismissed approval is dismissed; a read without my reviews leaves none and the row's bytes are version 2; malformed version 3 (missing fields, no verdict, unknown verdict, a non-flag, version 2 with a verdict) is refused and a submission before the epoch is refused at encode. No request is added: the composed course's refresh still reads one ListHomePullRequests and one ListReviews. A read-only GraphQL probe of MikeMarcin/BPR-Test (as the active account, X-RateLimit-Remaining 4941) returned submittedAt as 'YYYY-MM-DDTHH:MM:SSZ', the shape the parser takes."}},{"criterion":"AC3","outcome":"pass","revision":"f882beaf3d044a069013f583a23bcd1eb085117b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_review_landing_model|qt_home_list_scale_(100|125|200)|application_personal_discovery_(cli_)?public_path)$'","detail":"qt_review_landing_model: chip words and sentences for each verdict, with and without a time and with new commits; the row's verdict roles; its accessible name 'Draft pull request; octo/app#7 Cache the parsed config. You approved this on <date>.'; a row without a verdict says none; new commits change only that row. qt_home_list_scale_100/125/200 at 720, 430, 360 and 300 wide: sections unchanged (approved in Following, approval with new commits Changed in Needs me); an unreviewed row has no chip; each chip is inside its row, complete (never narrower than its words), leads the second line, leaves the reason word and the facts column their place, and its words are not in the row's laid-out text; the accessible name ends with the tooltip sentence; a ToolTip event through the viewport shows the anchored tooltip with the sentence at anchored_tooltip_geometry of the chip, and one elsewhere on the row shows none; the approval's check has accepted-ink pixels and the stale approval's has none. application_personal_discovery_public_path (and _cli_): after Refresh both windows' #4003 rows carry MyReview approved at 1791994800, the chip 'Approved', the tooltip 'You approved this on 14 Oct 2026.' as the accessible name's end and a drawn chip; after the restart the row still says Approved. Offscreen 2x renders (light and dark, 720 and 430 wide) inspected before and after."}},{"criterion":"AC4","outcome":"pass","revision":"f882beaf3d044a069013f583a23bcd1eb085117b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_queue_refresh|qt_review_queue|qt_composed_github_scale_100|application_home_waiting_public_path|application_queue_composition_contract)$'","detail":"Buckets, home_section_for, queue_precedes and permits_notification are untouched; the courses above and these pass unchanged."}},{"criterion":"AC5","outcome":"pass","revision":"f882beaf3d044a069013f583a23bcd1eb085117b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(architecture_boundaries|command_correctness_registry_contract|application_review_round_trip_composition_contract|test_network_isolation_manifest)$'; cmake --preset core-only && cmake --build --preset core-only && ctest --preset core-only; cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <the 15 changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"Candidate rebased onto main ad6908848. architecture_boundaries passes with the chip's tooltip through show_anchored_tooltip (BPR-1354). The command-correctness and round-trip pins need no reseal (the fixture server is outside the manifest); the contract gains a note at its top. core-only builds and its tests pass. coding-format-check (clang-format 22.1.8) passes on every file; clang-tidy (LLVM 22) with warnings as errors passes on the 15 changed translation units. Both documentation gates pass with a stand-in allocated ID; with the provisional ID the gate refuses only the card's ID format. macOS, dev Debug -O0 -g0."}}]
---

# BPR-1718: My review verdict on Home rows

## Owner note

The owner, 2026-10-21, verbatim:

> On the home page I would like to know if I have approved a PR. Right now
> they go to following and I don't really know that I took action.

[RFC-BPR-0032](../../rfcs/RFC-BPR-0032-calm-home-and-accounts-surfaces.md#owner-amendment-my-review-on-home-rows--2026-10-21)
records the amendment, and the
[workbench specification](../../ux/workbench-specification.md#my-review-on-home-rows)
holds the wording.

## Acceptance

- AC1: The core decides my standing verdict from my submitted reviews:
  approved, changes requested, commented or dismissed, when it was submitted,
  and whether commits followed it. A comment-only review never replaces an
  approval or a change request, and a dismissed review is never approved.
- AC2: The verdict comes from the retained read Home already makes. The
  batched `ListHomePullRequests` query adds `submittedAt` to its review nodes,
  and the REST reviews' `submitted_at` is read. No request is added. The
  verdict is kept with the row's stored facts and survives a restart and an
  open.
- AC3: A Home row I have reviewed begins its second line with a complete
  verdict chip. It has a mark and a word, and " · new commits" when commits
  followed. Its tooltip and the row's accessible name say "You approved this
  on <date>." and the matching sentences. The chip never elides, and a narrow
  row keeps its width for the text.
- AC4: Sections, attention buckets, notifications, ordering and counts are
  unchanged.
- AC5: Focused build and tests, the core-only preset, format and tidy, and
  the documentation gate pass.

## Design

**Core** (`bpr/core/review_queue_attention.hpp`). `my_review_verdict` is
`none`, `commented`, `changes_requested`, `approved` or `dismissed`, and is
append-only. `my_review_for(span<my_submitted_review>)` reads my submitted
reviews oldest first, each with its state, its submission time and whether
its commit is the current head. Approved, changes requested and dismissed
replace one another, and the latest wins. A comment-only review replaces none
of them. A dismissal stands for nothing, so a comment after it is the verdict,
and a comment before it does not outlast it. `NewCommits` is the verdict
review's own commit against the head. An unknown commit says nothing, and a
dismissal has no staleness. The result, `queue_my_review`, is a new
`queue_item_facts::MyReview`.

**Host**. `personal_discovery_host::apply_metadata_row` already walks the
retained read's reviews for `ReviewedByMe`. It now also collects the
viewer's own submitted reviews, without pending ones, and asks the core for
the verdict. A metadata answer replaces the kept verdict, because a retained
read reads every review. An open rebuilds a row's facts without reading
reviews, so it keeps the verdict, as it keeps the change facts. The queue
facts codec writes version 3, which appends the verdict, its time and the
new-commits flag, only for a row with a verdict. Every other row keeps its
version 2 bytes, and versions 1 and 2 still read.

**Adapter**. `personal_discovery_review::SubmittedAt` is seconds since the
epoch. The batched fragment asks `submittedAt` on each review node. The node's
exact key set includes it, and its value must be null or a timestamp. A
malformed node sends the batch the REST way, as any malformed alias does.
The REST path reads `submitted_at`, which the review record already keeps.
The fixture server in `src/app/src/github_public_path_fixture.cpp` answers
both.

**Qt**. The model answers `home_row_role::verdict` (the chip's words),
`verdict_state`, `verdict_stale` and `verdict_tip`, and appends the sentence
to `Qt::AccessibleTextRole`. The delegate draws the chip at the start of the
second line. It has a thin neutral outline with the Draft chip's small
radius, its words are in the second line's font, and its mark is painted: a
check in the accepted ink, a plus over a minus in the defect ink, or the
comment bubble. A verdict with new commits after it has a quiet mark, and a
dismissed review has no mark and quiet words. The second line's first text
line starts after the chip and later lines return to the left edge
(`lay_out`'s lead), so narrow rows wrap as before. With room for fewer than
twelve characters beside the chip, the chip takes a line of its own. The
delegate's `helpEvent` shows the sentence over the chip only, and
`verdict_rect` reports its place. The `frontend-design` skill was loaded for
this work.

## Decisions to revisit

- The mark's colour is a second narrow exception to colour meaning state on
  Home. It uses the accepted and defect inks that already colour lines added
  and removed. The word carries the meaning without colour. If the owner
  wants Home's chips strictly neutral, only the mark's ink changes.
- Placement is unchanged. An approved pull request with nothing new stays in
  Following, and one with new commits stays Changed in Needs me. No approved
  group or sort was added, because the chip already answers "did I act".
- The tooltip and accessible sentence are in the second person ("You
  approved this on…"), as the request worded them. Home's row sentences are
  in the first person.
- An in-app submission shows on Home at the next retained read (Home's
  refresh or its freshness window), not at once. Opening the pull request
  keeps the last verdict.
- Following's collapsed summary does not count approvals.

## Execution

The candidate is f882beaf3d044a069013f583a23bcd1eb085117b on main ad6908848. Renders are offscreen
only, and no owner desktop was driven. The before and after renders, at 2x in
light and dark, wide and narrow, are in the coordinator's scratchpad
`homeverdict/` folder. No live GitHub effect was made. One read-only GraphQL
probe of BPR-Test confirmed the `submittedAt` shape. At landing, allocate the
ID on main and rename this file and every `BPR-1718` reference, including
the round-trip contract's note.
