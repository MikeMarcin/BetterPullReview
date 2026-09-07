---
schema: "implement-rfc.card.v1"
id: "BPR-0647"
title: "Pull request overview page"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"RFC-BPR-0028 AC-04 with the mockup's columns and groups","outcome":"pass","revision":"3d83daae + this finish","evidence":{"kind":"observation","detail":"application_pull_request_overview pins the host-decided projection: header, folder-ordered rows with operation, added/removed, changed-since-review (changed, new, unknown), read state from receipts, unresolved/total threads, my note; participants with sign-off; the since-my-last-review summary (uploads by update row, replies to me, my unresolved threads, checks by outcome); progress; Start/Continue entry. qt_pull_request_overview_page pins the page: folder groups, every column, participant rows, the side values (not available / not retained / not yet resolved where unanswerable), the entry button and its dispatches."}},{"criterion":"Continue review lands on the first file marked changed since my last review, at its first hunk; Start review lands on the first file","outcome":"pass","revision":"3d83daae + this finish","evidence":{"kind":"observation","detail":"application_pull_request_overview: a first visit starts at the first file; with a resolved change set Continue lands on the first changed file even when read; with the set unknown on the first file not read; a head already reviewed has no changes since. qt_workbench_window_scale_100: Continue review and an activated row dispatch select_workbench_file_intent for the named file and enter Read, where the comparison opens the freshly selected file. The change set itself is BPR-0654; until then the column is unknown, never unchanged."}},{"criterion":"Focused Qt tests for the table model and the summary; a composed course through the fixture that opens a pull request on Overview","outcome":"pass","revision":"3d83daae + this finish","evidence":{"kind":"observation","detail":"qt_pull_request_overview_page and test_overview_shows_the_decided_page_and_continue_enters_read (qt_workbench_window_scale_{100,125,200}); the plain public-path course in src/app/main.cpp chooses Overview after the fixture pull request opens, requires the decided page with the drawer's file count and its action enabled, presses Continue review and requires Read with the comparison back, reported as overview=1 in PUBLIC_PATH_VERDICT; application_public_path, application_github_public_path_{github_com,ghes_3_18,uiforetw_74} and application_diff_workbench_public_path pass."}},{"criterion":"Gates: focused selectors, composition contracts with repinned digests, boundaries, tidy, format, docs","outcome":"pass","revision":"3d83daae + this finish","evidence":{"kind":"observation","detail":"See the card's verification note: the focused set (window, composed, epoch, page, overview, public paths, workbench/queue/ledger/appearance composition contracts, application_composition_contract, uiforetw selected-diff contract) green; application_review_round_trip_composition_contract repinned in two passes; check_boundaries; strict tidy on every touched translation unit; clang-format --Werror; verify_documentation.py. application_defect_composition_contract fails at 3d83daae with the slice stashed and is carded as BPR-0655."}}]
---

# Pull request overview page

RFC-BPR-0028 mockup state 1. The Overview modality's content: title, author
and branches, description, the files table grouped by folder (operation,
added and removed counts, changed since my last review, my read state, unread
and total comment badges, my note), participants with sign-off state, the
since-my-last-review summary, my review progress, and Continue review (Start
review on first visit) entering Read at the first unseen change. Data comes
from the accepted snapshot, the epoch host and the private ledger projections
the host already publishes; any missing projection is added in the host, not
computed in Qt.

## Acceptance

- RFC-BPR-0028 AC-04 with the mockup's columns and groups.
- Continue review lands on the first file marked changed since my last review,
  at its first hunk; Start review lands on the first file.
- Focused Qt tests for the table model and the summary; a composed course
  through the fixture that opens a pull request on Overview.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.

## Notes

- Projection first. `application::project_pull_request_overview` in
  `src/application/src/pull_request_overview.cpp` decides the whole page from
  the accepted `github_pull_request_snapshot` and an `overview_context`: the
  ledger's receipts for the pull request's files (keyed by
  `core::file_identity_of`), the private observation (notes, last-reviewed
  head), the retained update rows, the accepted head, an optional resolved
  change set and an optional draft count. It yields the header, the file rows
  sorted by folder then name with operation, counts, changed-since-review,
  read state, thread counts (unresolved and total) and my note; the
  participants (the author first, reviewers by their latest review, everyone
  else who posted as an observer, the viewer flagged); the since-my-last-review
  summary; the checks by outcome; my progress; and the one action with the
  file it lands on. Where the inputs cannot answer, the field is absent or
  `unknown` rather than zero: unread post counts (no post read state until
  [BPR-0648](BPR-0648-aligned-comment-cards-in-read.md)), new
  comments since (the read model carries no post times), uploads since when
  the reviewed update is not among the retained rows, and the changed set
  ([BPR-0654](BPR-0654-resolve-the-files-changed-since-my-last-review.md)).
  `epoch_row_ledger` mirrors the review-history model's row reconciliation so
  the composition holds the same update rows the epoch line shows. Pinned by
  `application_pull_request_overview`.
- The join is the composition's (`publish_overview` in
  `github_application.cpp`): no single host holds the snapshot, the ledger and
  the epochs, so it reads the GitHub host's accepted snapshot, loads each
  file's receipt through the ledger host (`load_receipt`, a read the
  review-ledger contract insists goes through the host, never the store),
  takes the ledger host's private
  observation and the epoch host's accepted head, and publishes on every
  pull-request presentation update, every accepted receipt and every catalog
  change. The appearance profile reaches the page beside the comparison
  surface, so the state colours are the theme's.
- The page (`pull_request_overview_page`, `src/adapters/qt`): the main column
  is the title, the author-and-branches line, the description and the files
  table (`overview_file_model`, a two-level tree of folders and files with
  the accepted columns; `overview_row_delegate` paints the operation box, the
  changed and new chips, a hollow, half or filled read dot so the state
  survives without colour, and the unresolved-over-total comment bubbles);
  the side column is the participants as rows with initial, login, role and
  standing, "Since my last review on <date>" with its values, "My review",
  and Continue review (Start review on a first visit). Every value the host
  could not answer reads "not available", "not retained" or "not yet
  resolved" and a changed cell reads "?", never as none. Pinned by
  `qt_pull_request_overview_page`.
- The window: beneath the epoch line the centre is a stack
  (`workbenchCentreStack`) of the comparison page and the overview page. The
  overview shows for the Overview modality once a projection has been
  published; every other modality, and Overview before one, shows the
  comparison, so the probes' comparison geometry is unchanged. The Overview
  preset's drawers are as BPR-0646 left them (files open, comments
  collapsed): the mockup's strip shows the files toggle on. Continue review
  and an activated file row select the file by the same
  `select_workbench_file_intent` the files drawer sends and enter Read;
  landing "at its first hunk" is the comparison surface's own opening
  position on a freshly selected file. Pinned by
  `test_overview_shows_the_decided_page_and_continue_enters_read` in the
  window tests; the centre pin now names the comparison page. The page
  reports no minimum size: a stacked widget's minimum is the largest of its
  pages' whether shown or not, and the first run of the UIforETW and defect
  contracts drifted because the hidden overview had widened the centre.
- The public path over the fixture (`src/app/main.cpp`, the plain course):
  after the pull request opens, Overview is chosen and must show the decided
  page with as many files as the files drawer and its action enabled;
  Continue review must return the centre to the comparison in Read; the
  modality the course had is restored before the render evidence. The
  verdict line carries `overview=1`.
- Verification at this head: `application_pull_request_overview`,
  `qt_pull_request_overview_page`, `qt_workbench_window_scale_{100,125,200}`,
  `qt_composed_github_scale_100`, the composition contracts with both digests
  repinned (twice: once for the join, once for formatting), strict tidy on the
  four touched translation units (two pre-existing naming errors in
  `github_application.cpp` fixed in passing, the Glaze option names exempted
  by name), `clang-format --Werror`, the docs gate.
- Found at this head, belonging to no card: `application_uiforetw_selected_diff_contract`
  and `application_defect_composition_contract` fail at 3d83daae with the
  slice stashed. The first is the strip: with the Home button in the row, the
  fixture's long identity at 1280 pushed the right region into an overflow
  menu, so the identity now yields below 1400 (it was 1180), as the strip's
  own yield order says it should. The contract's own row check still
  described the pre-RFC-0028 strip (file pair before change pair, no
  platform inset, the comments toggle last, one spacer before it), so it now
  pins the accepted row: inset and Home before Files, change pair then file
  pair, the centre region between the file pair and review details at
  least 100 wide, refresh, divider, account and settings after the comments
  toggle, and a collapsed drawer as its 18-pixel rail at the splitter's
  edge rather than nothing. The probe itself was already BPR-0646's; the
  contract passes again. The second is
  carded as [BPR-0655](BPR-0655-defect-composition-seed-stalls-resolving-the-comparison.md)
  with its reproduction.

