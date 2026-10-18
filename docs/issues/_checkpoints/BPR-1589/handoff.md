# BPR-1589 handoff: under owner review

- Branch `bpr-1589-status-content`; product head `7ed3eec00` (on `ee9830062`,
  "Claim BPR-1589"), not merged with `main` yet. The pause note `20113b42b`
  was dropped when the owner resumed the redesign.
- Changed paths: core `review_role` (+ `core_review_role`); the overview
  projection's `Status`; `drawer_parts` (`drawer_section_header`,
  `drawer_meter`, the audit ignores rows an ancestor hides);
  `pull_request_overview_page` (the Status content); `workbench_window`
  (thread count, Go to them); the `status-attention-v1` fixture, its builder
  and its registration in `src/app`; tests listed on the card; the round-trip
  contract resealed for `github_application.cpp` and the production digest.
- Verification (focused, no full suite): core_review_role,
  application_pull_request_overview, qt_pull_request_overview_page,
  qt_drawer_parts_scale_{100,125,200}, qt_disclosure_button_scale_{100,200},
  qt_identity_avatar, qt_workbench_window_scale_100, qt_composed_github_scale_100,
  application_overview_summary_{1,1_25,2}, application_accepted_file_in_overview,
  application_profile_multi_window_public_path,
  application_personal_discovery_public_path,
  application_overview_labels_public_path,
  application_status_attention_public_path, every application composition
  contract, architecture_boundaries, coding_format_check.
- Owner look: frozen root `/Users/jaeger/bpr1589-status-content-v001.xadh4h9g`
  from `7ed3eec00`, executable SHA-256
  `13d1333674555842fd5f6d24531582c7102dc6f4d1a37de595725a37b6e895bc`, its
  `--check` reached `ATTENDED_PULL_REQUEST_OPENED`. Session
  `87347e1859c7d3be5439b0adc9d29f93`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261017-bpr1589-status-v001`,
  candidate `BPR-1589-status-content-v001` (fingerprint `343280fb…`), ready
  at sequence 3; host PID 35881.
- The Discuss line's question was dropped: the owner decided pull-request
  comments get read state, allocated as [[BPR-1601]] (after this card). The
  line "N new comments in the conversation" with Go to them (Discuss) is built
  and hidden until BPR-1601 supplies a count.
- Next: read `status --after 3`. On Accept, merge `main`, run the focused set
  plus the composition contracts, reseal, docs gate, finish BPR-1589, push,
  stop PID 35881 and remove the frozen root. On Reject, revise from the note.
