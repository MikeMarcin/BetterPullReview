---
schema: "implement-rfc.card.v1"
id: "BPR-0776"
title: "Render the list as state panels with one row anatomy"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0775"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A6"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 — Rows are in state order with one anatomy for both models; colour appears only on the reason word and the rail or mark","outcome":"pass","revision":"2729b93af90636a67e3ff783157fee048d370509","evidence":{"kind":"command","detail":"qt_home_list_scale_100/125/200 pass on source 2729b93a (rfc-worker log d3-0776-ctest-final-qt-2.log). The courses cover:\n- Panels in the order Needs me, Waiting on others, Following, Recently viewed, over core::ordered_queue with all seven buckets, keeping source order within each state.\n- Both models answer every home_row_role and share home_row_delegate.\n- Rows are at least 44 points and wrap without elision.\n- On a greyscale test palette, saturated pixels appear only in the reason word, the rail and the header, including on a selected row.\nqt_review_landing_model and qt_appearance_profile pass the seven reason words, the summaries, and 4.5:1 text contrast in all four variants. Negative control (d3-0776-negative-control.log): the panels planted in reverse order make qt_home_list_scale_100 fail at 'the panels are Needs me, Waiting on others, Following, then Recently viewed'; reverted with a matching sha256, it passes again."}},{"criterion":"AC2 — Each state panel collapses by chevron, Space, Return, Left and Right, keeps its count and summary, and its header sticks under scroll","outcome":"pass","revision":"2729b93af90636a67e3ff783157fee048d370509","evidence":{"kind":"command","detail":"qt_home_list_scale_100/125/200:\n- A click on the chevron toggles, and so do Space, Return and Enter; Left collapses and Right expands.\n- Collapsed, the count stays and the summary equals home_state_summary.\n- The accessible state reports expanded or collapsed, with one state-change event per toggle.\n- The sticky header sits at viewport y 0 under scroll and stops at its panel's bottom, with a tint raster sample at each scale; bands also stick under their group header.\nqt_composed_github_scale_100/125/200 and qt_workbench_window_scale_100/125/200 pass through the window composition."}},{"criterion":"AC3 — Empty states are absent, the all-empty list is one sentence whose link opens Settings › Reviews & queue, and no empty box renders","outcome":"pass","revision":"2729b93af90636a67e3ff783157fee048d370509","evidence":{"kind":"command","detail":"qt_home_list_scale_100/125/200:\n- With only Following rows, the other state panels are hidden and are not focus stops.\n- With every state empty, Needs me holds exactly 'Nothing needs me. Pull requests from saved repositories and observer rules appear here.' Its link opens settings_page::reviews_and_queue once on Return, Enter or Space.\n- No expanded visible panel is without rows or the sentence; while restoring there are no panels, and when filters hide every row there is no sentence.\n- Resume is a row action on hover and on focus and in the row menu, and the heading-row button is gone.\nShipped composition: application_queue_composition_contract, application_setup_destination_public_path and application_profile_multi_window_public_path pass (d3-0776-ctest-e2e-2.log). application_profile_window_review_restoration_public_path passes (ctest-d3-0776-restoration.log, 549.7 s). architecture_boundary_contract passes; architecture_boundaries stops on a GitHub adapter CMakeLists.txt inside the unrelated .claude/worktrees/bpr-dif-integration-rfc-3c2904 checkout (environmental; that worktree is not ours to touch). Review corrections to BPR-0775 in d0eded6c: Dif stays out of the corner under a transient status message, including one showing when Home arrives, with Home's bottom inset sized from the status bar; the hidden landing buttons carry no stale names; with the queue ready, Add enterprise server opens the draft with the hostname focused; the restoration waits exclude the loading sentence. Rechecks on the corrected tree: qt_composed_github_scale_100/125/200 and qt_workbench_window_scale_100/125/200 pass, and application_profile_window_review_restoration_public_path passes in 548.6 s (d3-0776c-chain.log, d3-0776c-qt.log, d3-0776c-post.log). With the message gate removed, qt_workbench_window_scale_100 fails at the transient-message corner check, so the gate is load-bearing; that course exits at its first failure, so the new message-before-Home check is not separately shown to discriminate (d3-0776c-neg.log)."}}]
---

# BPR-0776: Render the list as state panels with one row anatomy

## Current frontier — BPR-0775 and BPR-0776 accepted; home-page continues with BPR-0783, bpr-shell-surfaces-20260915 d3 / rfc-integrator-d3 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `home-page`: obligations `BPR-0775:*` (accepted), `BPR-0776:*` (this card), `BPR-0783:*` (open) and both exits. The milestone is not complete.
- **Candidate.** Local `main`, not pushed.
  - `BPR-0775`: source `edb90493`, finish `51fcca79`.
  - This card: promote `e7588939`, claim `f269c2a6`, source `2729b93a`, review corrections to `BPR-0775` in `d0eded6c`, and this record in the finish commit.
  - The claim helper's commit `6faf13ed` has no attribution trailer and is left as is.
- **Usable behaviour on Home.**
  - The Open bar leads; the first-launch card, or the return row over a review; the session line; Dif on the card, else in the corner.
  - The list is Needs me, Waiting on others and Following as panels, then Recently viewed.
  - One row delegate paints both models: identity, title, why sentence, reason word in its state colour, and time where projected.
  - Panels collapse by pointer and keyboard, keep their count and summary, and have sticky headers.
  - Empty states are absent; an all-empty queue is one sentence linking to Settings › Reviews & queue.
  - Resume is a row action.
- **Exit status.**
  - `home-page:exit:1` is partial. First-launch, default, long-list, empty and over-a-review render, with the bar focused on arrival. The four grouping states belong to `BPR-0783`.
  - `home-page:exit:2` is partial. Queue and recent rows share one delegate and panel, and no empty box renders. The surviving preferences in Filter and per-window view state belong to `BPR-0783`.
- **Continuation for a fresh integrator (`BPR-0783`).**
  - Promote and claim `BPR-0783`, then implement Group, State headers, account chips, the Filter menu, the refresh glyph and per-window persistence.
  - Design summary and open decisions: scratchpad `frontier-d3.md` (from the d3 design leaf; no code written).
    - Persistence: a `home_view_state` application type in the RFC-BPR-0024 per-window `window_layout` store (SQLite v7 to v8, keeping `one_pane`), with the host merging a pending view at load; no core change.
    - Filter: a window-local copy of the four surviving fields, with profile preferences as defaults.
  - Decisions still owed:
    - (a) Group-panel collapse keys would name owners, repositories or accounts, but the settings store must not record reviewed content (`test_the_schema_stores_no_review_content`). Choose session-only group collapse or an owner decision.
    - (b) No whole-queue refresh intent exists for the strip's refresh glyph.
    - (c) The Filter labels versus the typed values.
    - (d) Account chips derive from the distinct host and account of the rows; avatars are initial tiles only.
  - The `home_panel` band style and the `set_leading`/`set_trailing` header slots are ready for group panels.
  - Then the milestone integration pass and an independent critical review of the store migration.
- **Carried forward.**
  - Human, for `BPR-0779`:
    - The 40 px Dif corner rendering.
    - The attention tones and the reason and summary wording.
    - Whether a session failure is discoverable from Home.
    - RFC A5's "nothing else" versus the signed-out Recently viewed.
  - Follow-up for main to allocate: Dif resting in the corner on review surfaces (mandate r2, window-level).
  - Native, outside this sandbox: the seven credential-store, fixture-seed and stale-expression courses listed on `BPR-0775`.
  - `architecture_boundaries` stops on `.claude/worktrees/bpr-dif-integration-rfc-3c2904/src/adapters/github/CMakeLists.txt`, a checkout that is not ours; `architecture_boundary_contract` passes.
- **Ownership.** Quiescent. The d3 scouts, the `BPR-0776` worker, the `BPR-0783` design leaf and the reviewer have all finished or are idle, and no command is running.

## Outcome

Add a `home_panel` widget (bordered, rail in the state colour, tinted sticky header with name, count and chevron, collapsible by pointer and keyboard with a one-line summary when collapsed) and a `home_row_delegate` (identity, title, why sentence, reason word in the state colour, time and progress) shared by the queue and Recently viewed models; rows always in state order from the core attention projection; absent empty states; the one-sentence all-empty state with its Reviews & queue link; Resume as a row action; Recently viewed as the last panel.

## Acceptance

- Rows are in state order with one anatomy for both models; colour appears only on the reason word and the rail or mark.
- Each state panel collapses by chevron, Space, Return, Left and Right, keeps its count and summary, and its header sticks under scroll.
- Empty states are absent, the all-empty list is one sentence whose link opens Settings › Reviews & queue, and no empty box renders.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Verification — d3

| Check | Direct surface and command | Result |
|---|---|---|
| Worker courses | `ctest -R '^(qt_home_list_scale_(100\|125\|200)\|qt_review_landing_model\|qt_appearance_profile\|qt_composed_github_scale_(100\|125\|200)\|qt_workbench_window_scale_(100\|125\|200))$' -j1` on source `2729b93a` | 11/11 pass (scratchpad `d3-0776-ctest-final-qt-2.log`) |
| Shipped composition | `application_queue_composition_contract`, `application_setup_destination_public_path`, `application_profile_multi_window_public_path` | 3/3 pass (`d3-0776-ctest-e2e-2.log`) |
| Restoration course | `application_profile_window_review_restoration_public_path` | Passes on `2729b93a` (549.7 s, `ctest-d3-0776-restoration.log`) and on `d0eded6c` (548.6 s, `d3-0776c-post.log`) |
| Negative control, state order | Panels planted in reverse order in `home_list.cpp` | `qt_home_list_scale_100` fails at "the panels are Needs me, Waiting on others, Following, then Recently viewed"; reverted with a matching sha256, it passes (`d3-0776-negative-control.log`) |
| Boundaries | `architecture_boundary_contract`, `architecture_boundaries` | The contract passes. `architecture_boundaries` stops on a GitHub adapter `CMakeLists.txt` in the unrelated `.claude/worktrees` checkout (environmental; `d3-0776-ctest-boundaries.log`) |
| Format | `git clang-format`, `git diff --check` | Clean on both commits (`d3-0776-format-4.log`, `d3-0776c-chain.log`) |

#### Independent review of BPR-0775 and its corrections (`d0eded6c`)

- No blocking finding.
- Fixed in `d0eded6c`, each with a recheck:
  - (1) Dif stays out of the corner under a transient message, including one showing when Home arrives.
  - (2) With the queue ready, the card's Add enterprise server opens the draft with the hostname focused.
  - (5) The hidden landing buttons carry no stale names.
  - (6) Home's bottom inset follows the status bar height.
  - (7) The restoration waits exclude the loading sentence.
- Rechecks: `qt_composed_github_scale_100/125/200` and `qt_workbench_window_scale_100/125/200` pass, and the restoration course passes (`d3-0776c-qt.log`, `d3-0776c-post.log`).
- Negative control: with the message gate removed, `qt_workbench_window_scale_100` fails at the transient-message corner check, so the gate is load-bearing. That course exits at its first failure, so the new message-before-Home check is not separately shown to discriminate (`d3-0776c-neg.log`).
- Recorded for `BPR-0779` and `BPR-0780`, not changed:
  - (3) A session failure is said only on its host's card.
  - (4) RFC A5's "nothing else" versus the signed-out Recently viewed.

Limitations: offscreen renders are layout evidence only. Native style, VoiceOver's expanded and collapsed reading, and the attended verdicts on tones and wording are owed to `BPR-0779`.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
