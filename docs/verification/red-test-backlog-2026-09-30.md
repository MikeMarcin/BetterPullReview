# Red-test backlog milestone, macOS arm64, 2026-09-30

Coordinator-implementer milestone "red-test backlog" (recipe revision 2), after
[main stabilization](main-stabilization-2026-09-30.md): the 22 carded Mac reds,
BPR-1244's cross-platform half, and Mac confirmation runs for the Windows
station's fixes. Base: `69493e235`. Station: macOS arm64 (Darwin 25.6), dev
preset (Debug, Apple clang), Qt 6.11.2 offscreen. The Windows and Linux stations
pushed concurrently; `git pull --rebase` renamed several commits after they were
first recorded, and each card notes its published hash.

## Cards

| Card | Outcome | Commit on main |
|---|---|---|
| [BPR-1248](../issues/60-done/BPR-1248-rfc0016-capture-read-only-route-inventory-drifted.md) | Done. f44c95dd7 had edited the owner-attended capture's recorded probe document; the attended review binds that fixture by digest, so the recorded bytes were restored and marked as evidence, not resealed | `708426196` |
| [BPR-1246](../issues/60-done/BPR-1246-review-submission-composition-expects-a-left-side-draft-projection.md) | Done. Product fix: a private-review promotion (note or chat reply) reported a refusal while it wrote the draft; it now completes on the draft's acceptance. Course follows BPR-0587, BPR-1211 and BPR-1242 | `68599e10c` |
| [BPR-1245](../issues/60-done/BPR-1245-agent-composition-courses-predate-rfc-0023-private-drafts-and-runs.md) | Done 2026-10-03. 15 of 15 rows pass on macOS. The owner accepted `BPR-1245-workshop-v001` at hand-back 869, and it is built: a workshop press records the draft as an immutable reviewer revision in Private history and asks with it as the only source | `c7ba632ea` |
| [BPR-1247](../issues/60-done/BPR-1247-large-review-accepted-metadata-render-exceeds-its-budget.md) | Done 2026-10-03: the owner accepted `BPR-1247-debug-limits-v001` at hand-back 869. Bisected: f44c95dd7 adds about 110 ms in Debug and 11 ms in Release; the limit never passed in Debug (284 ms when set) and was justified only by optimized runs. Release at main: 68–73 ms. Debug reports, and optimized builds enforce, the six Release-justified limits | `d3f05b94b` |
| [BPR-1249](../issues/60-done/BPR-1249-tour-cli-terminal-courses-fail-on-macos.md) | Done. Product fix: `bpr tour edit` refused every edit on a Mac with the default `TMPDIR` (`/var` links to `/private/var`); the projection is now made under the canonical temporary folder. The publication course resolves its root | `c3df92bf5` |
| [BPR-1250](../issues/60-done/BPR-1250-the-windowless-macos-menu-bar-drops-help.md) | Parked, human-review. Candidate: the windowless bar keeps Help, Open Project… and the two tour commands, disabled. Attended item `BPR-1250-help-v001`; the unlocked Aqua run is owed | `9605cb7da` |
| [BPR-1244](../issues/60-done/BPR-1244-windows-a-new-symbolic-link-is-left-out-of-the-publication-preview-silently.md) | Backlog (Windows card). Cross-platform half done: a path the final capture saw but could not read is listed with its reason and counted. Reparse-point reading stays with the Windows station | `e464f13da` |
| [BPR-1230](../issues/60-done/BPR-1230-windows-review-comment-permalink-canonical-path-escaping-fails.md), [BPR-1231](../issues/60-done/BPR-1231-windows-private-review-public-path-rows-lack-qt-path-and-refuse-registry.md), [BPR-1233](../issues/60-done/BPR-1233-windows-live-review-submission-restart-recovery-store-does-not-open.md) | Done with the macOS runs beside the Windows and Linux ones | `fc462f174`, `0c2e4f012`, `5f46ad52e` |
| [BPR-1236](../issues/30-in-progress/BPR-1236-windows-profile-window-review-restoration-fails-failed-reading-identities.md) | Mac results recorded; the owner decides. Passed once at `69493e235` (615 s, beside five rows), then failed twice at `f79d5a97d` (full run 631 s, alone 456 s) at the same final check as Windows and Linux | `3a190f50f` |

## Attended items

Session `cbd48b91000b00f1e72e1ad62c7c384c`, prepared and protected, no server
started: `BPR-1245-workshop-v001` (`db162ddbd`), `BPR-1247-debug-limits-v001`
(`d3f05b94b`), `BPR-1250-help-v001` (`9605cb7da`), beside the earlier
`BPR-1241-retarget-key-v001`.

## Integration pass

At `f79d5a97d` (every change of this milestone and the stations' pushes):
`cmake --preset dev` and `cmake --build --preset dev -- -k 0` complete with no
failed step. `ctest --test-dir build/dev -j 8 --timeout 900` runs 577 tests:
**574 pass, 3 fail** (22 failed at the milestone's base).

| Row | Status |
|---|---|
| `application_agent_finding_workshop_press_contract` | Expected at `f79d5a97d`: parked on `BPR-1245-workshop-v001`. Built in `c7ba632ea` and passing on macOS since |
| `application_profile_window_review_restoration_public_path` | BPR-1236's failure, shared by all three platforms (fails alone too); recorded on that card |
| `application_setup_destination_public_path` | Timed out at 60 s under the full run; passes alone in 50.6 s. Filed as [BPR-1252](../issues/60-done/BPR-1252-setup-destination-public-path-runs-close-to-its-timeout-on-macos.md) |

A `BPR_BUILD_TOUR_CLI` Debug build (`build/dev-tour-cli`) passes its six tour
CLI rows. `python3 scripts/verify_documentation.py --root .` reports
`DOCUMENTATION_OK`, and its 64 self-tests pass.

## Owed

- Owner judgment on the four attended items above, and the unlocked Aqua run of
  `application_profile_empty_desktop_native_menu_bar`.
- Windows and Linux runs of this milestone's changes, in particular
  `core_review_cycle_outgoing`, `tour_cli_process_commands` (Windows) and the
  composition courses. BPR-1244's Windows reading or reported-row assertion
  belongs to the Windows station.
