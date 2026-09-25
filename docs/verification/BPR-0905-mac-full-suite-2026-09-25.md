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
The overall suite at `f6de40b6` remains a red historical run. Windows and
Linux runs are owner-deferred; native-only survey margins remain unknown.

## Follow-up disposition on the combined Mac checkout

- BPR-1195 mirrored the six disabled resident menu commands; the native,
  Qt and composed profile checks passed 3/3 after integration.
- BPR-1197 restored the visible selectable explanation for unavailable
  comparisons; seven affected checks passed, including both failed public
  paths.
- BPR-1196 proved stale first-tab comparison inheritance with a failing
  `1:1 → 2:2` control, repaired it, and passed the unchanged cache-byte
  course on combined main in 335.08 s. Eviction, revisit, supersession and
  cleanup remained checked.
- BPR-1198 reconciled the review-behavior course with a settled focused tab
  and the separate unselected Overview state. On combined main, review
  behavior passed in 95.51 s and the source-pinned round-trip check passed.

Those focused results repair the four changed-surface failures seen in the
historical suite; they are not a second full-suite pass. BPR-0966's Dif
Compose placement remains owner-blocked. The release-update fixture passed
its one serial rerun; its concurrent 500 ms deadline is a recorded flake.

## Release HTTPS fixture deadline follow-up

The `redirect-loop` course opens four sequential TLS connections to check the
three-redirect cap. Its former 500 ms shared deadline was the same as a
single-request course and could expire before the fourth response under Mac
full-suite contention, producing `timed_out` instead of the intended
`redirect` classification. That is the failure recorded above. The fixture
now allows 2,000 ms for this course, a stated fourfold multiple of the
unchanged 500 ms single-request allowance. The explicit four-request count
and redirect terminal remain the acceptance checks; the `timeout` course
retains 500 ms. The focused `release_update_native_https` row passed 1/1
on the combined Mac checkout in 8.31 s. This repairs the identified fixture
deadline; no second contended full-suite result is claimed.
