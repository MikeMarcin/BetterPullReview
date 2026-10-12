# RFC-BPR-0066 verification and closeout

Assignment `/root/home_authored_implementer`; mandate `home-authored-r1`; adopted plan `home-authored-v1`, milestone `authored-home`. RFC_BASE_SHA: `b87b7e4ae8f62696c3af88abe3c943eaef65c6f5`. Import `dc96961cc`, claim `341df1b49`, acceptance-heading correction `c5bc21a70`. Product candidate `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5` on main. Q1 is unchanged: waiting-only authored rows remain in Waiting on others. The original packet and its eight matched source hashes are retained by the [import record](../rfcs/assets/home-authored-v1.LIVE_IMPORT.md).

## Criterion evidence

| Criterion | Direct evidence | Result |
|---|---|---|
| AC01 | `core_review_queue_attention`: both accepted authorship sources with coexisting enrollment reasons across every bucket; a different account without authorship stays Needs me. `qt_home_list_scale_100/125/200`: two exact account rows sharing repository/number, only the other account counted in Needs me. | Pass |
| AC02 | Core waiting/unread precedence stays unchanged. Home rows preserve unread count, reason word, attention color, quiet and Unknown; collapsed summary counts action and unread and names Unknown distinctly. Core notification eligibility remains unchanged; repository drawer still reads the untouched bucket, with composed regression passing. | Pass |
| AC03 | `qt_home_list_scale_100/125/200`: every Group × State headers setting rendered, exact row reachable, hidden headers keep section order, collapse by Left/Right uses its independent bit, filtered count excludes quiet and preserves unknown, authored-only Home has no empty Needs me panel. Offscreen captures inspected at 100% with named groups and hidden headers. Existing focus/sticky/group/keyboard courses continue to pass. | Pass |
| AC04 | `sqlite_workbench_settings_storage` in dev and core-only: exact version-18 table upgrades, older bits/views preserved, authored panel expanded despite waiting collapse, independent windows store and reopen authored collapse. All historical migrations pass. `application_profile_window_layout_public_path`: separate processes seed/restore/verify two window rows, one with bits 8+16, and explicit Close/fresh-window isolation. Named group collapse's existing session-only course passes. | Pass |
| AC05 | `application_personal_discovery_public_path`: ordinary sibling Refresh observes fake-server reviewer requests ending, waiting→My pull requests keeps exact selected row, then Return opens that same #4001 in the originating window without changing its sibling. Existing read budget remains 16 lightweight reads per explicit refresh; section placement adds none. CLI version confirms accepted refresh and selection; its existing ordinary #17 row opening remains passing. Recently viewed, repository onboarding, signed-out and public-preview courses pass. | Pass |

All passing checks apply to candidate content committed as `a34080e0fbce7d7e8d9fdade436aa5826e21e4a5`. Offscreen evidence is deterministic layout evidence, not native appearance, VoiceOver, installed-package or Windows/Linux qualification. The new RFC criteria have no human/native/independent acceptance gate. BPR-0779's historical native/owner verdict remains bound to its original candidate; this closeout neither replaces it nor closes RFC0032's wider pending scope.

## Verification commands and results

- Focused builds in dev and core-only pass; shipped `app_better_pull_review` target builds.
- Core ownership and Qt model checks pass; Home at 100%, 125% and 200% passes.
- The eight-course Home integration pass passes all unaffected rows; the new native authored-open course's strict fixture correction then passes its focused recheck. Courses: `qt_composed_github_scale_100`, `qt_composed_home_sign_in`, `qt_anonymous_public_preview_scale_100`, `qt_workbench_window_scale_100`, `qt_home_list_scale_100`, `application_personal_discovery_public_path`, `application_personal_discovery_cli_public_path`, `application_profile_window_layout_public_path`.
- `architecture_boundaries` and `architecture_boundary_contract` pass in dev and core-only.
- Clang-format 22.1.8 checks all twenty touched C++ files. Tidy on six touched product sources exits 0; inherited virtual-template warnings in `home_list.hpp` and the unchanged alignment-flag narrowing in `home_panel.cpp` remain. No new tidy finding is claimed fixed.
- `python3 scripts/verify_documentation.py --root .`, the verifier's 67 tests, and `git diff --check` pass.

Private command output and runtime checkpoint: `.git/implement-rfc-home-authored/`. No logs, credentials, screenshots, build outputs or private data are committed.

## Failures retained and corrected

The import needed the verifier's canonical Acceptance criteria heading. First compile caught an uncalled new SQLite test. An old authored Unknown expectation was updated to include ownership. Defensive SQLite refused a writable_schema fixture shortcut; legacy fixtures now rebuild a real table with their actual old CHECK bound. The exact-v18 upgrade test caught the real hard-coded predecessor cap, fixed from 17 to 18 for schema19. Old v8/v9 fixtures also needed their old bound restored, and the hostile unknown bit is now 32 because 16 is valid. The native authored-open fixture initially included an extra actor field that strict recording rejects; it now uses the existing actor schema. The CLI full-read fixture does not serve #4001, so that new ordinary-open assertion runs on the native recording; CLI accepted-refresh/selection and its existing ordinary-open course remain exercised.

## Documentation and ownership

Applied the authorized Home amendments to RFC0026, RFC0028, RFC0032 and the Home specification; documented the core/host/Qt section boundary and schema19 in system architecture. Historical Done cards and plan exits are untouched. Used `frontend-design:frontend-design` from the installed Claude plugin; native font, palette roles, layout and state colors govern the design.

Attended-review conflicts are empty in both the default registry and the active shared repository registry. Other review servers were observed and left running; this task owns no server. No worktree was created. No provider read or mutation, owner desktop/browser access, push or credential access occurred. The unrelated `.gitignore`, `.mask.json`, `.mask.lock.json` changes and untracked authoring drafts remain preserved. All five criteria and the single milestone exits are complete; no next milestone remains.
