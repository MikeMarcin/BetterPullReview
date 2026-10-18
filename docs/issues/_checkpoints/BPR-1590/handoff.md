# BPR-1590 handoff: v002 under owner review

- Branch `bpr-1590-compose-sheet`; product head `67d16fc4f`. Not on `main`.
- v001 (`frozen-2f2dab458-v001`, session `e3b17ba0…`) was rejected, decision
  `41b07e24`: "Why is there so much dead space in this expanded one? I don't
  think this looks like the direction I approved does it? Also I can't tell
  if these are ugly just because there is placeholder fake data or if they
  are just ugly.. I think the summary lines look nicer right now then what I
  see here." Its host was stopped, the session released and the root removed.
- Deviations of v001 from the chosen mockups (compose.html C and A), each
  fixed in v002: the expanded column sat at the page's left with an empty
  right region (A: a 680-pixel column centred); the count, the pending rows
  and the summary were three separate boxes with "Saved comments…" above
  them (A and C: one box, header with the blocking chip, rows, summary); no
  pull-request line under the title (A); the summary field was a fixed
  160-pixel wall (A: about 110 pixels, grows with words); the sheet's header
  had no rule or Esc hint and no row-reveal hint (C); line numbers were in
  the highlight blue (A and C: amber, bold, code font).
- Rows: restyled toward the thread lines the owner preferred (the accepted
  note anatomy: dashed rail in the label's tint, mark, quiet file and lines
  line, two lines of words with the label bold) rather than the mockups'
  one-line columns, which v001 and today's page both use and the owner
  judged worse. Verdict buttons keep no icons: their icon slot carries the
  BPR-1085 approve-with-open-blocking dot.
- Evidence: `--attended-finish-review-look` writes five real-looking
  comments into a review through the drawer and a summary, then opens
  Finish your review; renders of today's page came from a scratch worktree
  at `7836cd9fb` with a throwaway render course (removed).
- Verification (focused): qt_review_submission_editor_scale_{100,125,200},
  qt_workbench_window_scale_{100,200}, application_start_review_contract,
  application_review_submission_composition_public_path and _scale_125,
  application_setup_destination_public_path, application_discuss_top_public_path,
  application_header_source_navigation, application_review_navigation_entry,
  application_source_pr_find, application_agent_finding_composition_contract,
  every application composition contract (round-trip resealed),
  architecture_boundaries, coding_format_check.
- Owner look: frozen root `/Users/jaeger/bpr1590-compose-sheet-v002.izb8fq7u`
  from `67d16fc4f`, executable SHA-256
  `6f3fa94c66be57e4c954176ccfdfde72227219534fa7e0653685c4a4760cfde6`, check
  reached `ATTENDED_FINISH_REVIEW_SHOWN`. Session
  `882800300815ab7eb32c3b57883c6079`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1590-compose-sheet-v002`,
  candidate `BPR-1590-compose-sheet-v002` (fingerprint `d45319b0…`), ready at
  sequence 3; host PID 27578.
- Next: read `status --after 3`. On Accept, merge `main`, run the focused set
  plus the composition contracts, reseal, docs gate, finish BPR-1590, push,
  stop PID 27578 and remove the frozen root; then BPR-1591.
