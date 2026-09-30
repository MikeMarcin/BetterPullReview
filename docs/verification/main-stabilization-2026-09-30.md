# Main stabilization milestone, macOS arm64, 2026-09-30

Coordinator-implementer milestone "main stabilization" (recipe revision 2):
make the known-red Mac rows green while the Review Cycles pack waits on parked
reviews and a live target. Base: local `main` at `e9e65eab2`. Station: macOS
arm64 (Darwin 25.6), dev preset (Debug, `/usr/bin/c++`), Qt 6.11.2 offscreen.

## Cards

| Card | Outcome | Candidate |
|---|---|---|
| [BPR-1228](../issues/60-done/BPR-1228-restore-the-application-composition-contracts-at-main.md) | Done: reviewed reseal of seven pinned-scope commits, typed testing-grant probe, release graph checked for build edges | `46e0d07b2` |
| [BPR-1238](../issues/60-done/BPR-1238-the-defect-composition-contract-fails-to-seed-its-epoch-fixture.md) | Done: probe picks a right-side line (BPR-0587's rule); a window's own private reload correlates again | `6e1bf4ddc` |
| [BPR-1240](../issues/60-done/BPR-1240-the-scoped-writing-agent-fixture-compiles-as-cxx98.md) | Done: `cxx_std_23` for `test_scoped_writing_agent` | `790bc57cf` |
| [BPR-1241](../issues/40-parked/BPR-1241-compose-review-and-the-tour-retarget-both-claim-alt-r.md) | Parked, human-review: tour retarget moved to Option-Shift-U; attended item `BPR-1241-retarget-key-v001` | `ddecba284` |
| [BPR-1242](../issues/60-done/BPR-1242-the-notes-layout-rows-predate-the-private-history-page.md) | Done: Qt rows follow RFC-BPR-0023's Notes pages and BPR-1200's first read | `577fd7fa5` |
| [BPR-1243](../issues/60-done/BPR-1243-three-repository-contract-rows-are-red-at-main.md) | Done: format, dependency modernization and contained temporaries | `94250ab9c` |
| [BPR-1232](../issues/60-done/BPR-1232-windows-markdown-body-qtemporarydir-denies-the-appcontainer.md) | Done: macOS run added to the Windows and Linux runs | `dfef04a68` |

Triaged and filed, not fixed here:
[BPR-1245](../issues/40-parked/BPR-1245-agent-composition-courses-predate-rfc-0023-private-drafts-and-runs.md)
(15 agent composition rows),
[BPR-1246](../issues/60-done/BPR-1246-review-submission-composition-expects-a-left-side-draft-projection.md)
(3 review submission rows),
[BPR-1247](../issues/30-in-progress/BPR-1247-large-review-accepted-metadata-render-exceeds-its-budget.md)
(2 large-review rows),
[BPR-1248](../issues/60-done/BPR-1248-rfc0016-capture-read-only-route-inventory-drifted.md)
(RFC-0016 capture pin),
[BPR-1249](../issues/30-in-progress/BPR-1249-tour-cli-terminal-courses-fail-on-macos.md)
(two tour CLI courses outside the dev preset) and
[BPR-1250](../issues/40-parked/BPR-1250-the-windowless-macos-menu-bar-drops-help.md)
(windowless Help).

## Integration pass

At `625a74087` (every product and test change of the milestone):
`cmake --preset dev` and `cmake --build --preset dev -- -k 0` complete with no
failed step, and `ctest --test-dir build/dev -j 8 --timeout 900` runs 577
tests: **555 pass, 22 fail**. Every failure has a card:

| Rows | Card |
|---|---|
| `application_agent_{chat,finding,invocation}_*` (15) | BPR-1245 |
| `application_review_submission_composition_{public_path,scale_125,scale_200}` | BPR-1246 |
| `application_large_review_{responsiveness,launch}` | BPR-1247 |
| `rfc0016_device_capture_security` | BPR-1248 |
| `application_profile_empty_desktop_native_menu_bar` | BPR-1250 |

The baseline run at the start of the milestone, with only BPR-1228's and
BPR-1240's first changes in the working tree, had 38 failures out of 577. The
16 now passing are the twelve Qt scale rows, the defect composition contract,
`coding_format_check`, `dependency_modernization_contract` and
`contained_temporary_usage_contract`. The full dev build itself was red at the
base (`test_scoped_writing_agent`).

None of the 22 is a Windows-only row. The Windows station's own reds
(BPR-1229 to BPR-1237 and BPR-1239) are Windows-specific and not observed
here. `python3 scripts/verify_documentation.py --root .` reports
`DOCUMENTATION_OK`.

## Owed

- Owner judgment on `BPR-1241-retarget-key-v001` (session
  `cbd48b91000b00f1e72e1ad62c7c384c`, prepared, no server started by this
  milestone).
- Windows and Linux runs of the milestone's changes; only macOS was run.

## Linux station, 2026-09-30

bpr-main-linux: Ubuntu 26.04 on WSL2, GCC 15.2, clang-format 22.1.8, Qt 6.11.2
offscreen, `build/dev` at `69493e235` (with the Windows station's `0dd1eb53f`).

- **Full dev build.** At `69493e235` it did not compile with GCC.
  `754502ff7` fixes two test sources: `-Wdangling-pointer` in
  `agent_launch_tests.cpp` and `-Wshadow=local` in `gh_cli_transport_tests.cpp`.
  After it the whole tree builds.
- **Release graph.** The tests-off `-O3 -D_FORTIFY_SOURCE=3` graph that
  `application_composition_contract` builds does not compile with GCC.
  `4519a44f8` fixes five translation units: four GCC `-Wmaybe-uninitialized`
  false positives on designated aggregates, and a discarded `write()` result.
  The sixth, `tour_publication_plan.cpp`, is inside the round-trip pin. A patch
  is proposed for a reviewed reseal (BPR-1228). The contract row has no Linux
  verdict yet: it reached its 600 s limit building the Release graph.
- **Pass:** `application_review_round_trip_composition_contract`,
  `application_defect_composition_contract`, `application_review_tour_writing`,
  `qt_composed_github_scale_*`, `qt_workbench_focus_order_scale_*`,
  `dependency_modernization_contract`, `contained_temporary_usage_contract`,
  `github_review_comment_permalink` (BPR-1230),
  `application_private_review_native/file/retry` (BPR-1231) and
  `live_review_submission_restart_recovery` (BPR-1233).
- **Fail on Linux, recorded, not changed:**
  - `coding_format_check`: clang-format 22 and 23 align four
    `github_application.cpp` case lines differently (BPR-1243).
  - `qt_document_tabs_scale_*`: at the window floor the tab viewport is 179 px,
    under the 220 px minimum tab width (BPR-1241).
  - `qt_workbench_window_scale_*`: the doubled DejaVu review bar needs 983 px
    at 982 px (BPR-1242).
  - `application_profile_window_review_restoration_public_path`: the same
    failure as Windows (BPR-1236).
- `application_agent_finding_mcp_construction_cut_contract` fails as BPR-1245
  records.

## Windows station — bpr-main-win, 2026-09-30

Windows 11 Home 10.0.26200, MSVC 19.51, clang-format 22.1.8, Qt 6.11.2, offscreen, `build/windows-dev` Debug with warnings as errors, at `69493e23`. The build needed no source change.

| Row | Windows | Card |
|---|---|---|
| `application_defect_composition_contract` | pass, 33.19 s | BPR-1238 |
| `application_review_round_trip_composition_contract` | pass, 11.51 s; again after `fd05277e`'s reseal, 11.02 s | BPR-1228 |
| `application_composition_contract` | timed out at 600 s through `ctest`. Run directly in the same environment, all 8 cases pass in 822.8 s. `0f8b9ebe` gives it 1,800 s on Windows only | BPR-1228 |
| `application_review_tour_writing` | contained, exit 1 with no line. Uncontained it passes, and `test_scoped_writing_agent` builds. `0f8b9ebe` makes it `WINDOWS_UNCONTAINED` with that pin. Then it passes, 22.79 s | BPR-1240 |
| `qt_composed_github_scale_{100,125,200}` | pass, 75.67, 59.43 and 63.81 s | BPR-1242 |
| `qt_workbench_focus_order_scale_{100,125,200}` | pass | BPR-1242 |
| `qt_document_tabs_scale_{100,125,200}` | **fail**: one 220-pixel tab in a 200-pixel strip, so the active tab cannot be shown whole | [[BPR-1251]] |
| `qt_workbench_window_scale_{100,125,200}` | **fail**: the title gives way, then returns at 1,099 pixels | [[BPR-1251]] |
| `coding_format_check` | failed: clang-format 22 and 23 align `dispatch_tour_action`'s short cases differently. `fd05277e` writes four of them one statement per line, which neither version aligns, with a reviewed reseal on top of BPR-1246's. Then it passes, 32.32 s | BPR-1243 |
| `dependency_modernization_contract` | pass, 127.13 s | BPR-1243 |
| `contained_temporary_usage_contract` | pass, 4.66 s | BPR-1243 |

`test_network_isolation_manifest` still fails for the same 47 older rows
([[BPR-1237]]). It names neither changed row. The document-tab and title rows
are filed as `11deadba` ([[BPR-1251]]), with the probe's measurements. Nothing
in them was changed. The logs are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\` (`stab.log`,
`st2.log`, `composition-timed.log`), and are not committed.
