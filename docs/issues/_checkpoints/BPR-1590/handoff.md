# BPR-1590 handoff: v001 under owner review

- Branch `bpr-1590-compose-sheet` (from `7836cd9fb`, BPR-1589 landed);
  claimed at `37f4ee890`; product head `2f2dab458`. Not on `main` yet.
- Changed paths: `src/adapters/qt/{include/bpr/qt,src}/compose_review_sheet*`
  (new), `workbench_window*` (sheet open, close, expand, restore, the focused
  page's single column with no thread list), `drawer_parts*` (a close glyph),
  `src/app/src/github_application.cpp` (the start-review course reads the
  sheet), the round-trip contract resealed, and the tests named below.
- Verification (focused, 25 rows): qt_workbench_window_scale_{100,125,200}
  (new course: sheet over Review at the right edge, principles, Expand and
  Restore keep caret and scroll, a pending row reveals its line with the
  sheet open, narrow window), qt_review_submission_editor_scale_*,
  qt_drawer_parts_scale_100, application_start_review_contract,
  application_review_submission_composition_public_path and _scale_125,
  application_setup_destination_public_path, application_discuss_top_public_path,
  application_header_source_navigation, application_review_navigation_entry,
  application_source_pr_find, every application composition contract,
  architecture_boundaries, coding_format_check.
- Owner look: frozen root `/Users/jaeger/bpr1590-compose-sheet-v001.wlxga4z2`
  from `2f2dab458`, executable SHA-256
  `5c9726b1c0204b664e58e0ae6ce24147164661d1bedc719f28b489608026f069`, check
  reached `ATTENDED_PULL_REQUEST_OPENED` (an `--open-file` freeze did not show
  its file within 60 seconds and was removed). Session
  `e3b17ba04ef5b4a318a72f9b274497ca`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1590-compose-sheet-v001`,
  candidate `BPR-1590-compose-sheet-v001` (fingerprint `824ca893…`), ready at
  sequence 3; host PID 68958.
- Next: read `status --after 3`. On Accept, merge `main`, run the focused set
  plus the composition contracts, reseal, docs gate, finish BPR-1590, push,
  stop PID 68958 and remove the frozen root; then BPR-1591. On Reject,
  revise from the note.
