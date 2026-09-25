# BPR-0905 Mac full-suite contention — 2026-09-25

Candidate `f6de40b6`, macOS arm64 dev graph. The complete dev graph built, then
`ctest --preset dev -j10 --output-on-failure` ran 452 rows in 1,973.96 seconds.
Result: 445 passed, seven failed. The uncommitted run log is retained at
`/Users/jaeger/.codex/tmp/bpr0905-full-mac-20260925.log`, SHA-256
`fcbda836a17681396f6de6e91fa677826a38a301e1eaa6083aa1b970a1f906ea`.

All four rows whose bounds BPR-0905 corrected **passed under this contention**:

| Row | Result | Duration |
| --- | --- | ---: |
| `application_saved_server_contract` | Pass | 100.35 s |
| `application_defect_composition_contract` | Pass | 60.06 s |
| `application_review_ledger_composition_contract` | Pass | 38.86 s |
| `sqlite_epoch_catalog_behavior_contract` | Pass | 45.09 s |

The seven failed rows do not establish a BPR-0905 bound repair failure:

- `application_review_behaviour_contract` expired 76 ms after its existing
  120 s epoch budget, but its terminal shows a focused four-root/file-anchor
  state where the course expects six unfocused roots. No larger bound is
  justified. Follow-up BPR-1198 owns identity reconciliation.
- `application_selected_file_safety_public_path` and
  `application_unavailable_diff_public_path` failed the now hidden/full-message
  status presentation and copy contract; both failed again serially. Follow-up
  BPR-1197 owns the visible complete explanation.
- `application_large_review_cache_bytes` waited 553,706 ms for the first
  selected large file after 32 epoch refreshes while the diff was ready and
  jobs idle. The retained tab comparison is a changed-surface hypothesis,
  not an established resource-budget cause. Follow-up BPR-1196 owns exact
  comparison observation and repair.
- `application_agent_finding_dif_context_contract` reproduces the known Dif
  Compose placement gap, retained under BPR-0966 for owner disposition.
- `release_update_native_https` returned timeout instead of redirect in a
  four-hop TLS fixture with a shared 500 ms deadline. It passed one serial
  rerun (7.85 s); this is load-sensitive fixture evidence, not a release
  transport product verdict.
- `application_profile_empty_desktop_native_menu_bar` failed again serially:
  the resident menu lacks the new tab/window action placeholders. Follow-up
  BPR-1195 owns exact menu parity.

The focused serial rerun of the four short failed rows passed only
`release_update_native_https` (1/4). Its log is retained at
`/Users/jaeger/.codex/tmp/bpr0905-short-rerun-20260925.log`.

This run supplies Mac contention evidence for BPR-0905's four changed rows.
The overall suite is red and the follow-up failures remain open. Windows and
Linux runs are owner-deferred; native-only survey margins remain unknown.
