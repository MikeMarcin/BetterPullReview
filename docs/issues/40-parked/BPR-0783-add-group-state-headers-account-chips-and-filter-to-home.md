---
schema: "implement-rfc.card.v1"
id: "BPR-0783"
title: "Add Group, State headers, account chips and the Filter menu to Home"
status: "Parked"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0776"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A7"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 — Each Group value renders with State headers on and off against a two-account fixture, with avatars and chips placed per the account count and hosts shown only where they differ","outcome":"pass","revision":"ce62ed52a97a5a0e9a4e404edb4c449ae08d8084","evidence":{"kind":"command","detail":"qt_home_list_scale_100/125/200, qt_workbench_window_scale_100/125/200, qt_composed_github_scale_100/125/200, qt_anonymous_public_preview_scale_100/125/200, qt_review_landing_model, qt_appearance_profile, qt_settings_effect_boundary and architecture_boundary_contract: 16/16 pass (scratchpad d4.1-qt-ctest.log, 175 s). The courses cover:\n- test_each_grouping_renders_with_state_headers_on_and_off: every Group value with State headers on and off over a sanitized two-account fixture; a group is a neutral panel headed by an initial tile, its name and count, with state bands nested when headers are on and marked state-ordered rows when off; empty groups and bands are absent and Recently viewed is last in every grouping.\n- test_account_chips_follow_the_account_count_and_the_group: with two accounts each row carries its account and the host only where it differs; a single-account group carries the chip in its header and its rows drop it; a group holding two accounts keeps the chips on the rows and none in the header (mixed_account_queue); under Account grouping no row carries one and the header states its host; a single-account profile shows none anywhere.\n- test_a_sort_orders_rows_within_a_state: with Sort = repository the rows order within each state, flat and grouped, rather than across states.\n- test_group_none_without_state_headers_is_one_marked_list, test_a_band_collapses_and_sticks_under_its_group_header, test_the_keyboard_walks_groups_bands_and_rows, test_an_empty_queue_is_one_sentence_in_every_grouping, test_a_large_grouped_queue_stays_incremental.\nOffscreen captures of the accepted boards (scratchpad d4-captures/): group-none-headers-on, group-none-headers-off, organization-headers-off, organization-headers-on, account-headers-on, filter-menu-open. Negative control (d4-qt-negative-control.log): planting Accounts.size() >= 1U makes qt_home_list fail at 'a single-account profile shows no chip on its rows'; reverted with a matching sha256, it passes again. Limitation: offscreen renders are layout evidence only; native style, VoiceOver and the attended verdict on these boards belong to BPR-0779."}},{"criterion":"AC2 — Filter holds exactly the four surviving preferences over the existing store with reset unchanged; the strip shows the controls signed in and none signed out","outcome":"pass","revision":"ce62ed52a97a5a0e9a4e404edb4c449ae08d8084","evidence":{"kind":"command","detail":"qt_workbench_window_scale_100/125/200 (scratchpad d4.1-qt-ctest.log):\n- test_home_strip_holds_group_state_headers_filter_and_refresh: Group, State headers, Filter and the refresh glyph sit before the divider, account and settings while Home shows with an account, and are absent rather than disabled signed out, so RFC-BPR-0028's rule still holds.\n- test_home_filter_menu_and_its_gesture: the menu holds exactly My role, Added by, Quiet items and Sort within a group with its footer; one gesture sends the existing typed queue-preferences intent with that one field changed, preserving Repository, Bucket and notifications, and marks the window's own four values; Home then filters by the window's filter, an owning window ignores a later profile change while Settings still shows the profile's answer, and a window that never filtered follows the profile with its Filter control ticked and marked accordingly.\n- test_the_six_queue_controls_live_on_settings_queue_defaults: the six combos are Settings' Queue defaults with Reset unchanged and out of the workbench's focus order.\n- Repository and Bucket are kept for the profile and filter nothing on Home.\nIndependent review of the Qt slice raised one blocking finding, that the Filter control went stale in a window following the profile. Fixed by apply_home_filter_controls() called from apply_queue_preferences; before/after evidence in scratchpad d4-qt-stale-before.out ('FAILED: the profile preference a window follows is ticked in its Filter menu') and d4-qt-stale-after.out ('workbench-window-tests:pass').\nThe Filter menu's ampersand was being read as a Qt mnemonic, so its footer rendered 'Reviews _queue'; escaped through menu_literal() and asserted on the displayed text, with the regenerated capture d4-captures/filter-menu-open.png showing 'Settings › Reviews & queue'."}},{"criterion":"AC3 — Group, State headers, collapsed panels and Filter survive a window restart independently in two windows of one profile","outcome":"pending","revision":"ce62ed52a97a5a0e9a4e404edb4c449ae08d8084","evidence":{"kind":"command","detail":"Passing, through the shipped composition: application_profile_window_layout_public_path (24.09 s, scratchpad d4.1-integration.log) now drives Home's own arrangement across a real restart of two windows of one profile. In the seed phase window A groups by organization with the state headers off while window B keeps the defaults, collapses Recently viewed and takes a Requested filter; after the restart A restores its grouping and neither B's collapse nor B's filter, B restores its collapsed Recently viewed and its own owned filter, and a fresh window starts at Home's defaults rather than a sibling's. tests/end_to_end/profile_window_layout_contract.py reads the durable rows between phases and requires the two windows' home_grouping, home_state_headers, home_collapsed, home_filter_owned and home_role to differ exactly as chosen, and the surviving window to keep its own after the other is closed.\nStore and host: sqlite_workbench_settings_storage and application_workbench_host pass 4/4 in core-only (d4.1-coreonly.log). The window_layout table goes to schema version 8 carrying every version-seven value forward inside one transaction, refusing a newer or foreign file and every out-of-vocabulary stored value; negative control d4-persist-negative-control.log shows the migration test failing when one_pane is dropped from the copy and passing again after a byte-identical revert. A view applied before the record arrives merges into it; a view whose record can never be read is applied, published and reported as settings_unavailable rather than lost (the probe in d4-persist-probe.log caught that the publication was withheld, and the publish gate now admits it). No window's Home view can become a profile default. An independent critical review of the migration and the host's pre-load merge found no blocking defect.\nPENDING, and the reason this card finishes human-pending: a group panel's collapse is kept for the window's life only and does not survive a restart. Its key names an owner, a repository or an account, and the workbench settings store has never recorded what was reviewed (BPR-0189, test_the_schema_stores_no_review_content). Whether to change that is the owner's call; the exact question is recorded in this card's Decisions (a) and is the one thing outstanding for this criterion."}}]
---

# BPR-0783: Add Group, State headers, account chips and the Filter menu to Home

## Current frontier — BPR-0783 human-pending; home-page's last card, bpr-shell-surfaces-20260915 d4.2 / rfc-integrator-d4 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `home-page`: `BPR-0775:*` and `BPR-0776:*` were accepted in d3; this card closes the rest of `BPR-0783:*` except the one owner question below.
- **Candidate.** Local `main`, not pushed: promote `336e1262`, claim `d089cfe5`, source `ce62ed52`, and this record in the finish commit.
- **Usable behaviour.**
  - The strip carries Group, State headers, Filter and the refresh glyph while Home shows with an account, and none of the three signed out.
  - Group renders None, Organization, Repository and Account; a group is a neutral panel with an initial tile, its name and count, state bands nested inside when State headers is on and marked state-ordered rows when off. Rows stay in state order under every grouping, and a sort orders rows within a state.
  - Account chips appear only when the queue names more than one host and account: on each row, or in a single-account group's header, never under Account grouping or for one account.
  - Filter holds exactly My role, Added by, Quiet items and Sort within a group over the existing typed preferences. A gesture sends the same typed intent the combos sent and, from then on, the window keeps its own four values; a window that never filtered follows the profile. The six combos are Settings' Queue defaults.
  - Group, State headers, the three state panels' and Recently viewed's collapse, and Filter survive a restart per window, through `window_layout` schema v8.
- **Exit status.**
  - `home-page:exit:1` is met for the rendered states: first-launch, default, long-list, empty, over-a-review and the four grouping states, with the bar focused on arrival. The attended verdict on those boards is `BPR-0779`'s.
  - `home-page:exit:2` is met except for one part of "view state persists per window": a group panel's collapse is kept only while the window lives.
- **The one thing outstanding.** Decisions (a)'s owner question, parked on `human-review`. Nothing else on this card is open.
- **Ownership.** Quiescent. Both correction workers stopped and reported; no child, build or test process remains.
- **Note for whoever runs the next dispatch.** Both workers were still editing this tree during d4.1 while I had been told they had exited, which produced one duplicated fix (since removed) and cost a rebuild. If ownership moves again, confirm it with the worker rather than from a process check alone.

## Outcome

Put Group (None, Organization, Repository, Account), the State headers toggle, Filter and the refresh glyph in the strip before account and settings; render groups as neutral panels headed by the owner's or account's avatar from the existing avatar cache, with state bands nested inside when State headers is on and plain state-ordered rows when off; show a host only where it differs; place the account chip at the right of rows when the profile has more than one account, in a single-account group's header instead, and nowhere for a single-account profile; fold the surviving typed queue preferences into Filter as My role, Added by, Quiet items and Sort within a group, dropping Repository and Bucket as filters; persist Group, State headers, collapse and Filter per window.

## Acceptance

- Each Group value renders with State headers on and off against a two-account fixture, with avatars and chips placed per the account count and hosts shown only where they differ.
- Filter holds exactly the four surviving preferences over the existing store with reset unchanged; the strip shows the controls signed in and none signed out.
- Group, State headers, collapsed panels and Filter survive a window restart independently in two windows of one profile.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Decisions — d4

- **(a) What collapse is remembered.**
  - Group, State headers, Filter, and the collapse of the three state panels and Recently viewed are stored per window in the RFC-BPR-0024 `window_layout` row (settings store v8).
  - A group panel, and the state bands inside it, keep their collapse while the window lives. Their keys name an owner, a repository or an account. The workbench settings store holds nothing that names what was reviewed, a rule since BPR-0189 that `test_the_schema_stores_no_review_content` enforces.
  - Remembering group collapse across a restart would mean writing those names into that file, which is the owner's call. It is parked as a human-review question rather than decided here.
- **(b) The refresh glyph.**
  - It is the existing Refresh action (Command-R), placed in Home's strip group whenever the strip shows Home's controls.
  - It acts on the pull request open beneath Home and is disabled when none is open.
  - No whole-queue refresh intent exists. Adding one would be a provider change this card does not authorize; it is a follow-up for main, and a question for the attended Home boards in `BPR-0779`.
- **(c) Filter words over the typed vocabularies.** Each section lists every typed value, in the board's word where the board has one:
  - My role: Any, Author, Reviewer, Requested, Participant, No role.
  - Added by (the observer reason): Any, Repository rule, Author rule, Path rule. The board's "Rule", "Follow" and "Request" have no typed value.
  - Quiet items: Hide, Show, Only quiet.
  - Sort within a group: Priority, Repository, Pull request number. No update time is projected, so there is no "Updated".
  - The footer says nothing here changes GitHub and links to Settings › Reviews & queue.
- **(d) Account chips and avatars.**
  - The account set is the distinct host and account of the decided queue's rows, whatever the filter; chips appear when it has more than one.
  - A chip is a round initial tile and the login, plus "on host" off github.com.
  - There is no avatar cache, so every avatar is an initial tile. Owner kind is not projected: an owner tile is round when the owner is one of those accounts' logins and square otherwise, and an account group's tile is round.
- **(e) Filter and the existing store.**
  - A Filter gesture sends the existing typed queue-preferences intent, as the six combos did. The profile's store and Reset are unchanged, and the last choice becomes what a new window starts with.
  - From its first Filter gesture a window also keeps its own four values in its view state, so two windows of one profile filter independently across a restart.
  - Home filters by neither Repository nor Bucket.
  - The six combos move into Settings › Reviews & queue › Queue defaults, in place of the selectors that mirrored them.
- **Group order.** Groups follow their first row in state order, with ties broken by name. Recently viewed is last in every grouping.

### Verification — d4 and d4.1

| Check | Direct surface and command | Result |
|---|---|---|
| Qt courses | `ctest --test-dir build/dev -R '^(qt_home_list_scale_(100\|125\|200)\|qt_review_landing_model\|qt_workbench_window_scale_(100\|125\|200)\|qt_composed_github_scale_(100\|125\|200)\|qt_anonymous_public_preview_scale_(100\|125\|200)\|qt_settings_effect_boundary\|qt_appearance_profile\|architecture_boundary_contract)$' -j1` on the tree committed as `ce62ed52` | 16/16 pass, 175 s (scratchpad `d4.1-qt-ctest.log`) |
| Store and host | `build/core-only`: `sqlite_workbench_settings_storage`, `application_workbench_host`, `application_workbench_preferences_consumers`, `architecture_boundary_contract` | 4/4 pass (`d4.1-coreonly.log`) |
| Shipped composition | `application_queue_composition_contract`, `application_setup_destination_public_path`, `application_profile_multi_window_public_path`, `application_profile_window_layout_public_path`, `application_profile_window_review_restoration_public_path` | 5/5 pass, 674 s (`d4.1-integration.log`) |
| Home's view across a restart | The layout course's seed, restore and verify phases, with `tests/end_to_end/profile_window_layout_contract.py` reading the durable rows between them | Passes in 24.09 s: two windows keep different Group, State headers, collapse and Filter, a closed sibling's row does not leak, and a fresh window starts at Home's defaults |
| Negative controls | Chips (`d4-qt-negative-control.log`); the v7→v8 copy dropping `one_pane` (`d4-persist-negative-control.log`); the stale Filter before and after (`d4-qt-stale-before.out`, `d4-qt-stale-after.out`); the withheld publication on a failed load (`d4-persist-probe.log`) | Each caught its defect and passed again after a byte-identical revert |
| Independent review | Qt slice, and a critical review of the store migration and the host's pre-load merge | One blocking finding (the Filter control went stale in a window following the profile), fixed and rechecked; the migration review found no blocking defect |
| Build and format | Whole `build/dev` build; `git clang-format --force`; `git diff --check` | Build exit 0 (`d4.1-devbuild.log`); four Qt files reformatted; check clean |
| Rendered boards | Offscreen captures of Group None with headers on and off, Organization with headers on and off, Account, and the Filter menu | Six captures in scratchpad `d4-captures/`, regenerated after the ampersand fix |

Limitations: offscreen renders are layout evidence only. Native style, VoiceOver and the attended verdicts on these boards belong to `BPR-0779`. The seven sandbox-blocked courses recorded on `BPR-0775` were not re-run and are unchanged by this card. A group panel's collapse does not survive a restart, which is the parked question in Decisions (a).

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
