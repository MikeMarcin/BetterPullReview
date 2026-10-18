# BPR-1589 handoff: landed

- Branch `bpr-1589-status-content`; product head `39afb7162`, not merged with
  `main` yet.
- v001 (`frozen-7ed3eec00-v001`, session `87347e18…`) was rejected, decision
  `7e7d36fc`: "Overall I like the direction but the first section feels
  verbose. Also this you may be a member thing kind of sucks, is there no way
  to tell?" Its root was removed and its session released.
- v002: the role is one short label with chips (team, Code owner, other
  roles) and the reasons in the tooltip; the GitHub adapter asks
  `organization.teams(query:, userLogins:)` per requested team (bounded to
  eight, cached per host, profile, team and login for the session, GHES at
  `/api/graphql`, `INSUFFICIENT_SCOPES` kept as needing read:org). A
  `members(query:)` query timed out (HTTP 502 and 504) on a large team
  live, so it was replaced.
- Verification (focused): core_review_role, application_pull_request_overview,
  qt_pull_request_overview_page, qt_identity_avatar,
  qt_workbench_window_scale_100, github_paged_read, github_provider,
  github_read_contract, gh_cli_transport, github_anonymous_read_contract,
  native_https_anonymous_read_contract and _fixture,
  application_status_attention_public_path,
  application_overview_labels_public_path, application_overview_summary_1,
  the workbench and round-trip composition contracts, architecture_boundaries,
  coding_format_check. Live: the query on one of the owner's organization
  teams answered member for the owner and not a member for the test account;
  BPR-Test has no teams.
- Owner look: frozen root `/Users/jaeger/bpr1589-status-content-v002.8opz7kp0`
  from `39afb7162`, executable SHA-256
  `10709d45ca473e370903fff34e65636d348aa64f1ff1b26fba3579a1cab530bd`, check
  reached `ATTENDED_PULL_REQUEST_OPENED`. Session
  `82f52ad4245bb64879f3bf2680814dea`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1589-status-v002`,
  candidate `BPR-1589-status-content-v002` (fingerprint `e36ac89e…`), ready at
  sequence 3; host PID 13128.
- [[BPR-1601]] (pull-request comments get read state) follows this card; the
  Status drawer's conversation line is built and hidden until then.
- Next: read `status --after 3`. On Accept, merge `main`, run the focused set
  plus the composition contracts, reseal, docs gate, finish BPR-1589, push,
  stop PID 13128 and remove the frozen root. On Reject, revise from the note.

## Landed (2026-10-18)

The owner accepted v002 (session `82f52ad4…`, decision `71bc675f`, no note).
Main merged at `57f335411` (`88d5f086c`), the round-trip contract resealed,
32 focused rows passed on the merged tree (every application composition
contract included), host PID 13128 stopped, the session released and the
frozen root removed. BPR-1601 goes to the Codex queue (item 7b).
