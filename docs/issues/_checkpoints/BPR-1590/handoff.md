# BPR-1590 handoff: v003 under owner review

- Branch `bpr-1590-compose-sheet`; product head `f8d68d949`. Not on `main`.
- v001 (`frozen-2f2dab458-v001`) was rejected, decision `41b07e24` (dead
  space in the expanded page, not the chosen direction, placeholder data).
- v002 (`frozen-67d16fc4f-v002`, session `88280030…`) was rejected: "the
  comments in the mockup are contained to one line; the comments here are
  much larger ... We should be able to go from those comments in the finish
  review to the actual comments in the review/discuss ... the transition from
  the end of the last comment to the start of the review message is kind of
  subtle". Host PID 27578 stopped; root removed.
- v003 (`f8d68d949`):
  - Pending rows are one line again: mark, file, lines in amber, the comment
    elided with its label bold; the whole comment is the tooltip. A very long
    comment never grows its row.
  - Rows take Threads' gestures (BPR-1582) through the shared
    `thread_pick_gestures` (also behind the thread finder now): click
    selects only; double click / Return opens in Review; Shift opens in
    Discuss; the menu adds Show in Threads. Opening routes through
    `core::route_workbench_thread`, closes the sheet or leaves the expanded
    page, chooses the line, and once the workflow brings that draft into the
    drawer (`resume_pending_comment_open`) opens it there focused, or in its
    Discuss composer, or its Threads row.
  - The summary is its own section: "Review summary" heading, a gap, and a
    bordered field with the accent edge on focus. A line under it says how
    rows open, on the sheet and the page.
- Verification (focused): qt_review_submission_editor_scale_{100,125,200}
  (negative control, double click, Shift, Return, menu, one-line cap with a
  very long comment); the whole workbench window binary offscreen plus
  qt_workbench_window_scale_{100,200} (new
  `test_pending_rows_open_their_comments_like_threads`: single click opens
  nothing; double click and Return close the sheet and open the draft in the
  drawer, focused; the expanded page is left; Shift+Return opens Discuss's
  composer); application_start_review_contract (the real app opens the
  row's draft writing in the drawer); review submission composition public
  path and scales; setup destination; review navigation entry; source PR
  find; header source navigation; discuss top; agent finding composition;
  conversation finder scales; feedback label threads; every application
  composition contract (round-trip resealed); architecture_boundaries;
  coding_format_check.
- Evidence: app showcase renders (sheet and page, Light and Dark) and
  side-by-side sheets of mockups C/A, v002 and v003, embedded in the session.
- Owner look: frozen root `/Users/jaeger/bpr1590-compose-sheet-v003.oa8xziun`
  from `f8d68d949`, executable SHA-256
  `cf6279587421f07beb839c79726fcefce3c9945f491963dc82172de4d5f152a3`, check
  reached `ATTENDED_FINISH_REVIEW_SHOWN`. Session
  `95b80a7a4bd9bf3e575d2338a8986edc`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1590-compose-sheet-v003`,
  candidate `BPR-1590-compose-sheet-v003` (fingerprint `8b1c229f…`), ready at
  sequence 3; host PID 42536.
- Known, not changed here: the heading's blocking chip counts blocking
  feedback already on GitHub (0 in the fixture), as the accepted preflight
  design does; a pending blocking comment is not added to it.
- Next: read `status --after 3`. On Accept, merge `main`, run the focused set
  plus the composition contracts, reseal, docs gate, finish BPR-1590, push,
  stop PID 42536 and remove the frozen root; then BPR-1591.
