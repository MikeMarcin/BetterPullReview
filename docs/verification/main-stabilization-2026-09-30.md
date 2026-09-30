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
[BPR-1245](../issues/10-backlog/BPR-1245-agent-composition-courses-predate-rfc-0023-private-drafts-and-runs.md)
(15 agent composition rows),
[BPR-1246](../issues/10-backlog/BPR-1246-review-submission-composition-expects-a-left-side-draft-projection.md)
(3 review submission rows),
[BPR-1247](../issues/10-backlog/BPR-1247-large-review-accepted-metadata-render-exceeds-its-budget.md)
(2 large-review rows),
[BPR-1248](../issues/10-backlog/BPR-1248-rfc0016-capture-read-only-route-inventory-drifted.md)
(RFC-0016 capture pin),
[BPR-1249](../issues/10-backlog/BPR-1249-tour-cli-terminal-courses-fail-on-macos.md)
(two tour CLI courses outside the dev preset) and
[BPR-1250](../issues/10-backlog/BPR-1250-the-windowless-macos-menu-bar-drops-help.md)
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
