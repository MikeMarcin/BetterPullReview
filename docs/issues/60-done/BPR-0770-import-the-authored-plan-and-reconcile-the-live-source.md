---
schema: "implement-rfc.card.v1"
id: "BPR-0770"
title: "Import the authored plan and reconcile the live source"
status: "Done"
rfcs: ["RFC-BPR-0032", "RFC-BPR-0033", "RFC-BPR-0034"]
depends_on: []
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"intake-baseline","acceptance":["A1","B1","C1"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 — Live HEAD, RFC ID mapping, allocation receipts and source drift recorded","outcome":"pass","revision":"0efe3bf1789db9d8620fded1ac44afda981a2a3d","evidence":{"kind":"observation","detail":"Card Intake evidence rows Live HEAD and base, Pack integrity (shasum 36/36 OK exit 0), RFC mapping (overlay equals live apart from milestone_plan metadata), Allocation (three receipts committed with the import revisions, NEXT_ID BPR-0796) and Source drift, with drift items D1-D5 and the function-level seam table. The promote and claim commits 14e3f6b9 and ae739767 change only the card path and status."}},{"criterion":"AC2 — Every card belongs to the remapped authored plan; nothing settled in RFC-BPR-0018, 0024 or 0028 is reopened","outcome":"pass","revision":"0efe3bf1789db9d8620fded1ac44afda981a2a3d","evidence":{"kind":"command","detail":"Plan membership check: 17 plan work refs equal 17 pack-tagged live cards with no duplicate or unplanned card, and each card equals its pack card after alias remap. milestone_plan.py discover and inspect report valid, resolved, no diagnostics. issue_board.py validate reports valid (787 cards). git diff --stat 38f32226..0efe3bf1 over RFC-BPR-0018, 0024 and 0028 is empty, and git diff --stat b5122f09..0efe3bf1 over pack, plan, receipts, RFCs and cards is empty."}},{"criterion":"AC3 — Technical readiness stays separate from formal closeout; no unrelated predecessor work is added","outcome":"pass","revision":"0efe3bf1789db9d8620fded1ac44afda981a2a3d","evidence":{"kind":"observation","detail":"This slice changes only BPR-0770 (promote, claim, frontier and evidence record). RFC-BPR-0032, 0033 and 0034 remain Proposed. Closeout cards BPR-0781, BPR-0782 and BPR-0791 are untouched with every row not run. No card, RFC, plan or source file is added or edited, and cross-RFC drift (D1 RFC-BPR-0031, D2 RFC-BPR-0015) is recorded for the owning milestones rather than implemented. python3 scripts/verify_documentation.py --root . reports DOCUMENTATION_OK with exit 0 on the final card record (log: scratchpad doc-gate-d1.1.log)."}}]
---

# BPR-0770: Import the authored plan and reconcile the live source

## Current frontier — intake-baseline accepted, bpr-shell-surfaces-20260915 d1.1 / mandate r1

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `intake-baseline`, obligation `BPR-0770:*`, both exits. `milestone_plan.py inspect` at `0efe3bf1` recomputed definition `sha256:18240b49…`, obligation `sha256:89c9ca1b…` and prerequisites `sha256:44136fa3…`, identical to the assignment.
- **Candidate.** Documentation-only intake on local `main`: promote `14e3f6b9`, claim `ae739767`, and this record in the finish commit. No application source, plan, pack asset, RFC or other card changed. Nothing pushed.
- **Result.** The pack as imported on 2026-09-13 still matches the live board and plan with no drift. The seams are located by function in the table under Evidence. Five source-drift items (D1–D5) are handed to the milestones that own them.
- **Pending here.** Nothing. Formal closeout stays with `BPR-0781`, `BPR-0782` and `BPR-0791`, where every row is still **not run**. All three RFCs stay Proposed.
- **Next.** `accounts-page`, `home-page`, `profile-chooser` and `appearance` each depend only on this milestone and may run in any order. Plan order puts `accounts-page` (`BPR-0771` to `BPR-0774`) first; it must settle D2 before `BPR-0771` retires the session panel. `home-page` must settle D1 and D5, `profile-chooser` D4, `appearance` D1.

### Source drift for successors

- **D1. Dif on Home and Appearance (RFC-BPR-0031, Auto-Verified 2026-09-15, after this pack was authored).**
  - **Home.** Dif sits beside the `workbench_landing` guidance. `workbench_window::update_dif_presentation` (4421) creates one shared widget, `workbenchDif` (4433), and hands it either to `HomeLanding->set_illustration` (4449, enabled at 4436) or to `DifAiSlot`. The `homeDif` object name added by `4bbecb48` was retired in `ad0bb67d`. RFC-BPR-0031 B5 requires Dif to be reachable on signed-out and signed-in Home, including Home over a review.
  - **Appearance.** `appearance_settings_panel.cpp` gained a "Dif mascot" group: Show Dif mascot, Motion, and Reset Dif preferences with `appearance_reset_scope::dif`. B3 covers these controls.
  - **Gap.** RFC-BPR-0032's signed-in Home has no landing card, and RFC-BPR-0034's three cards (Theme, Type, Colours) have no Dif controls. Neither RFC reopens RFC text, so both milestones must keep RFC-BPR-0031's placement and preferences reachable and record the narrow layout adaptation.
  - **Owner question.** If the home-page integrator finds no adaptation within RFC-BPR-0031's own rule (hide Dif before squeezing controls), the placement becomes an owner question to batch with A8/C6.
- **D2. CLI trust warning (RFC-BPR-0015).**
  - **Where it lives.** `github_session_panel.cpp` holds `githubCliTrustWarning` in its identity group (built at lines 302–307, updated at 753–759; commit `3ffd3be2`).
  - **Requirement.** RFC-BPR-0015 requires the opt-in account row to name the configured-CLI trust risks.
  - **Constraint.** `BPR-0771`/`BPR-0772` must carry the warning into the server card before the session panel leaves Settings. `BPR-0788` (close RFC-BPR-0015) is still open.
- **D3. Line drift only.** Every audited seam kept its function; only the line numbers moved (see the table). The seam files changed after the audit only through the Dif, large-PR and CLI work listed in the table note.
- **D4. Unaudited quit branch.** `profile_desktop::implementation::eventFilter` quits (`quit(0)`) when the Profiles window is closed while no runtime exists (lines 316–320, commits `8380e6f8` and `826fb871`). Once `BPR-0777` removes the last-runtime presentation, this branch serves only the startup and no-window failure presentation. The empty-desktop policy must account for it.
- **D5. Toolbar and landing seams the pack does not name.**
  - **Account action.** `workbench_window::build_primary_toolbar` opens Accounts & hosts and begins sign-in when `DevicePanel` is visible (lines 612–622).
  - **Landing secondary action.** `build_central_surface` wires the landing's secondary action to focus `SessionPanel->owner_editor()`, which is the locator (lines 849–852).
  - **Constraint.** Both must be repointed when the locator and device panel move (`BPR-0771`, `BPR-0773`, `BPR-0775`).

## Outcome

Verify the pack; record the live HEAD, the RFC and issue allocations and the exact composition seams the design names (the Accounts page adoption in `workbench_window.cpp`, the last-runtime branch and failure presentation in `profile_desktop.cpp`, `main.cpp`'s quit policy). Import the authored five milestones rather than repartitioning the work.

## Acceptance

- Live HEAD, RFC ID mapping, allocation receipts and any drift from the pack's source audit are recorded.
- Every card belongs to the remapped authored plan; nothing settled in RFC-BPR-0018, 0024 or 0028 is reopened.
- Technical readiness stays separate from formal closeout; no unrelated predecessor work is added.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Intake evidence — d1/d1.1, source revision `0efe3bf1789db9d8620fded1ac44afda981a2a3d`

All checks are read-only observations of the live checkout. The promote and claim commits changed only this card's path and status.

| Check | Direct surface and command | Result |
|---|---|---|
| Live HEAD and base | `git rev-parse HEAD`; `git merge-base --is-ancestor 38f32226 HEAD` | `main` at `0efe3bf1`, clean at dispatch. Pre-import RFC base `38f32226351f5cc8e4657d55c3e151d8450c6ce5` is an ancestor. 187 commits since import commit `7bf4579d`. Unrelated worktree `.claude/worktrees/bpr-dif-integration-rfc-3c2904` untouched |
| Pack integrity | `shasum -a 256 -c SHA256SUMS` in `docs/rfcs/assets/bpr-shell-surfaces-v1/`; `git status` of `docs/rfcs/assets` and `docs/rfcs/plans` | 36 of 36 OK, exit 0, 36 archived files. Protected paths clean. `scripts/verify_pack.py` was not rerun: the bytes are unchanged, so the import record's pass still holds |
| RFC mapping | Compare live `docs/rfcs/RFC-BPR-003{2,3,4}-*.md` to `repository-overlay/docs/rfcs/` | Each live RFC differs only by the adopted `milestone_plan` metadata line. The IDs keep their pack numbers, with no collision and no renumbering |
| Allocation | `docs/issues/_reservations/` receipts; `docs/issues/NEXT_ID`; `docs/rfcs/plans/bpr-shell-surfaces-v1/id-map.json` | `BPR-0770--BPR-0782--c28923131dc6.json` (base `00a1a7d0`), `BPR-0783--BPR-0783--8eb2b8816cfa.json` (base `7105a11d`) and `BPR-0789--BPR-0791--9065278f98c2.json` (base `bd4f07b3`) were committed in `7bf4579d`, `cc243784` and `b5122f09`. `BPR-0784` to `BPR-0788` belong to RFC-BPR-0015 work. `NEXT_ID` is `BPR-0796`, past every pack ID. The alias map covers all 17 aliases |
| Plan membership | Plan work refs versus live cards tagged with the pack; each card compared to `cards/PACK-SHELL-NN.md` after alias remap; `milestone_plan.py discover` and `inspect --milestone intake-baseline`; `issue_board.py validate` for the three RFCs | 17 refs match 17 cards, with no duplicate ID and no unplanned card. Every card equals its pack card except the `milestone_plan` link replacing `alias_requires_live_allocation` and the three closeout cards' RFC base `38f32226…`. Each card's metadata milestone equals its plan group. The plan is found and structurally valid with references resolved and no diagnostics; semantic coverage is unknown. The board validates (787 cards). `8bec6810` deleted only the stale revision-1 duplicate files of `BPR-0775`/`BPR-0776`; nothing references them |
| Drift on pack records | `git diff --stat b5122f09..0efe3bf1` over pack, plan, receipts, the three RFCs, 17 cards and index rows; `git diff --stat 38f32226..0efe3bf1` over RFC-BPR-0018, 0024 and 0028 | Both diffs are empty. Nothing settled in RFC-BPR-0018, 0024 or 0028 is reopened |
| Source drift | `git log 38f32226..0efe3bf1` per audited file; `git blame` on the seam lines; object-name greps | D1 to D5 above. Audited files changed since the audit: `workbench_window.cpp` (+259/−12, Dif and large-PR loading), `profile_desktop.cpp` (+8, Dif), `main.cpp` (+72/−3, Dif preferences, large-PR epochs, CLI profile), `appearance_settings_panel.cpp` and `appearance_model.hpp` (Dif preferences), `workbench_landing.cpp` (Dif illustration), `github_session_panel.cpp` (CLI trust warning). No changes: `account_card.cpp`, `device_authorization_panel.cpp`, `profile_management_window.cpp`, `appearance_profile.cpp`, `theme_preview_strip.cpp`, `settings_window.cpp`. None of the new names `server_card`, `home_panel`, `home_row_delegate`, `empty_desktop_policy`, `info_glyph`, `type_preview` or `colour_role_list` exists yet |

#### Composition seams in the live tree

| Seam | Source audit (`38f32226`) | Live file, function and lines (`0efe3bf1`) |
|---|---|---|
| Accounts page adoption | `workbench_window.cpp`, about 1555–1600 | `src/adapters/qt/src/workbench_window.cpp`, `workbench_window::build_github_header()` (1532): `SessionPanel` 1566, `DevicePanel` 1595, `SettingsWindow = new settings_window{ this }` 1615, `AccountCard` 1624, column adds 1632–1634, "Enterprise servers" heading 1639, `build_enterprise_servers_editor(accounts, *accountsColumn)` 1645, `SettingsWindow->adopt_panel(settings_page::accounts_and_hosts, accounts)` 1646 (1563 and 1594 at the audit revision) |
| Enterprise servers editor | about 2682 | `workbench_window::build_enterprise_servers_editor` 2734 |
| Home construction | about 1290–1460 | `workbench_window::build_central_surface()` (762): `HomeLanding` 824 (818 at the audit revision, outside the audit's range), open bar 1342–1381 (`homeOpenBar`, `homeOpenButton`, `homeOpenNotice`), "Recently viewed" 1428; `workbench_window::update_home_landing()` 4292 |
| Pull-request stack's empty page | about 5041 | `workbench_window::update_github_presentation_state()` (5173), "Pull request unavailable" 5280 |
| Status bar text | about 6770–6790 | `workbench_window::update_status_text()` (7006), lines 7031 and 7033 |
| Failure presentation (a) | about 234 | `src/app/src/profile_desktop.cpp`, `profile_desktop::implementation::publish` (187), lines 235–236 |
| Manage Profiles closure (b) | about 471 | `implementation::wire` (453), line 473 |
| Last-runtime branch (c) | about 717, `519ddb2e` | `implementation::poll` (683), lines 718–719, blame `519ddb2e` |
| Recovery notice and reload | `initialize_management` | `implementation::initialize_management` (485): `RecoveryNotice` 492–496, `ReloadProfiles` 497–505; adopted in `publish` 212–216 |
| Profiles window close quits | not audited | `implementation::eventFilter` (314), lines 316–320 |
| Existing quit and drain path | not audited | `implementation::quit` 117, `force_shutdown` 131, `drain` 138; `perform` quit command 262–264; `set_desktop_quit_handler` 641 |
| Quit policy | `main.cpp`, about 7234 | `src/app/main.cpp`, `main(int, char**)` (7264), `QApplication::setQuitOnLastWindowClosed(false)` 7303 (7234 at the audit revision) |

Named regression courses for later milestones: `qt_composed_github_scale_100`, `qt_composed_github_scale_125`, `qt_composed_github_scale_200`, `application_device_authorization_public_path`, `application_saved_server_contract`, `application_profile_management_public_path`, `application_profile_management_host` and `qt_profile_management_window`.

Limitations: this intake boundary ran no build, no test and no application, and none was implied. Line numbers will drift; function names are the anchor. The object-name grep over `tests/` is not a repoint inventory, because several courses select by accessible name. D1 and D2 are recorded here, not designed. No native, rendered or attended evidence exists or is inferred.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
