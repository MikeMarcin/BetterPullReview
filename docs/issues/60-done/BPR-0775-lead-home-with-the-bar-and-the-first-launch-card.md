---
schema: "implement-rfc.card.v1"
id: "BPR-0775"
title: "Lead Home with the bar and the first-launch card"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"home-page","acceptance":["A5"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 — Signed in, the bar is the first row, labelled, focused; the landing has no focus-only buttons; a refusal sits under the bar","outcome":"pass","revision":"edb9049380ad506f056c09d6fa52f5783ed393fa","evidence":{"kind":"command","detail":"qt_composed_github_scale_100/125/200 pass on the source revision. test_home_leads_with_the_bar_signed_in: signed in, no visible Home child sits at or above homeOpenFrame; the frame holds the label Open (buddy of the bar), the placeholder 'Paste a link, or write owner/name#123' and the Command-L hint; the bar has focus on arrival; homeFocusBarAction is Ctrl+L as a window shortcut and focuses the bar with its text selected, from Home and from an open review. test_home_landing_funnels_to_an_account_and_then_a_pull_request: signed in with nothing open the landing and its buttons are hidden. test_a_load_refusal_is_said_under_the_home_bar is unchanged and passes. A first load in progress is said under the bar without Copy details (test_held_read_paints_loading). Negative controls (scratchpad negative-control-0775.log): with Home's line planted back to 'No pull request is open in the workbench.', qt_composed_github_scale_100 fails at 'the status bar says the signed-out session line'; with a Dif that never rests in the corner, qt_workbench_window_scale_100 fails at 'Home wins and rests the same presentation in the bottom-left corner'. Both plants were reverted with no marker left, the tree rebuilt, and both courses pass again."}},{"criterion":"AC2 — First launch and signed out show only the strip, the relabelled bar, the sign-in card with its two ways in, and the status bar line","outcome":"pass","revision":"edb9049380ad506f056c09d6fa52f5783ed393fa","evidence":{"kind":"command","detail":"test_home_signed_out_is_one_paste_field, at all three scales: the card 'Sign in to GitHub' with Sign in (default, accessible name 'Sign in to GitHub') and Add enterprise server (bordered, not default) follows the bar; the bar reads 'Paste a public pull request link to read it without an account'; no homeOpenButton and no workbenchHomeOpenLabel heading; every visible strip action is Home, account, settings or a spacer; the status line is 'Not signed in. Public pull requests open read-only.'; no stack page, notice, setup row or recently viewed list; Add enterprise server opens Accounts & hosts. Sign in begins the device flow on the card of the selected host (landing funnel course). application_setup_destination_public_path, application_profile_window_review_restoration_public_path and application_profile_management_public_path pass through the shipped composition."}},{"criterion":"AC3 — The status bar never reads that no pull request is open while Home is shown; Home over a review keeps its one return row","outcome":"pass","revision":"edb9049380ad506f056c09d6fa52f5783ed393fa","evidence":{"kind":"command","detail":"Home's line is the session line: 'Signed in as octo-reviewer on ghe.example.invalid.' signed in, and the signed-out line otherwise (test_home_leads_with_the_bar_signed_in, test_a_pull_request_without_a_comparison_still_lists_its_files, test_shell_accessibility_status_and_scale, test_review_history_state_is_explicit_in_workbench_text). Over a review, the landing is one shaded row above the bar (under 80 px): 'Reviewing …' with Back to the review and its Command-0 hint, and no second button or body. The bar has focus; the window's focus child returns to the review control, and to a strip control once the strip is settled, on Back to the review and on a modality. application_profile_multi_window_public_path passes its returning-from-Home focus, selection and scroll check. The mandate r2 Dif corner: qt_workbench_window_scale_100/125/200 test_dif_moves_to_the_explicit_ai_area_without_owning_it (40 px over workbenchDifCornerSlot at the status line's leading edge, a single widget, one decode, no pointer or focus path, hidden during a transient status message, AI slot restored on the review). Negative controls (scratchpad negative-control-0775.log): with Home's line planted back to 'No pull request is open in the workbench.', qt_composed_github_scale_100 fails at 'the status bar says the signed-out session line'; with a Dif that never rests in the corner, qt_workbench_window_scale_100 fails at 'Home wins and rests the same presentation in the bottom-left corner'. Both plants were reverted with no marker left, the tree rebuilt, and both courses pass again."}}]
---

# BPR-0775: Lead Home with the bar and the first-launch card

## Current frontier — BPR-0775 accepted; home-page continues with BPR-0776, bpr-shell-surfaces-20260915 d3 / rfc-integrator-d3 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `home-page`: obligations `BPR-0775:*`, `BPR-0776:*`, `BPR-0783:*` and both exits. This card closes `BPR-0775:*`; the milestone stays open.
- **Candidate.** Local `main`, not pushed: promote `be4d3f35`, claim `6faf13ed`, source `edb90493`, and this record in the finish commit.
- **Usable behaviour.**
  - Home's first row is the bordered Open bar: label, placeholder and Command-L. It has focus when Home appears, and the review's focus returns with the review.
  - Signed out, the card "Sign in to GitHub" offers Sign in (filled) and Add enterprise server (bordered), and the bar is relabelled for public reading.
  - Over a review, one shaded return row sits above the bar.
  - The stack's empty page is off Home. A first load is said under the bar, and a session failure on its host card.
  - The status line on Home is the session line.
  - Dif stays on the first-launch card, and otherwise rests at 40 px in the bottom-left corner (mandate r2; see Decisions).
- **Verification.** Recorded under Verification: the Qt courses at three scales, a full build, a 67-course integration selection (seven failures identical to baseline) and two caught negative controls.
- **Carried forward.**
  - Human: the 40 px corner rendering needs its own attended candidate in `BPR-0779`.
    **Answered 2026-09-16: accepted** (`bpr-0779-dif-corner`, decision 123f144b, on executable
    sha256 `d500ef7a…` from source `ed2b3f14`). Owner amendment r2 is settled.
  - Follow-up for main to allocate: Dif resting in the corner on review surfaces (window-level, RFC-BPR-0031 B6 courses).
  - Native, outside this sandbox: `application_public_path`, `_ghes_cli_unavailable`, `application_device_authorization_public_path`, `_ghes_refusal`, `_panel_course` and `application_saved_server_contract` (credential store, fixture seed), plus `application_anonymous_public_preview_public_path`'s stale pass expression.
  - The design leaves for `BPR-0776` (`rfc-worker`) and `BPR-0783` (`rfc-worker-deep`) have returned designs and are idle. An independent review of `6faf13ed..edb90493` is running; its adjudication goes on `BPR-0776`'s record.
- **Next.** Promote and claim `BPR-0776`, then resume its design leaf to implement the state panels and the row delegate.

## Outcome

Make the bar an address field alone in Home's first row (label, placeholder, ⌘L, Return, focused on arrival); remove the landing's focus-only buttons; make the signed-out landing the first-launch card with Sign in filled and Add enterprise server bordered and relabel the bar for public reading; show the strip with Home, account and settings only while signed out; seat a load refusal under the bar with Copy details; keep the pull-request stack's empty page off Home; keep the over-a-review return as one row; carry the session line in the status bar on Home.

## Acceptance

- Signed in, the bar is the first row, labelled, focused; the landing has no focus-only buttons; a refusal sits under the bar.
- First launch and signed out show only the strip, the relabelled bar, the sign-in card with its two ways in, and the status bar line.
- The status bar never reads that no pull request is open while Home is shown; Home over a review keeps its one return row.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Decisions — d3, mandate r2

- **Dif, mandate r2 reconciled with RFC-BPR-0031 (D1, Home part).**
  - The owner asked on 2026-09-15 for Dif to act mostly like AllTheThings' Nib. Nib has a first-run intro at hero size, then a 40-point footer presentation at the leading edge of the main window's footer, and never two at once.
  - Signed out, which is the first-launch and sign-in experience, Dif keeps RFC-BPR-0031's Home placement verbatim: beside the card's sentence at 96 px, 72 px below 1100 px of landing width, and hidden below 960 px before the controls would be squeezed.
  - On every other Home (signed in, and Home over a review) the same single widget rests at 40 px over `workbenchDifCornerSlot`. That slot is the first widget of the status bar, so the status line begins after him. He keeps Home playback and has no pointer or focus path, and he hides with the slot while the status bar shows a transient message.
  - RFC-BPR-0031's rules that yield to the owner's direction:
    - "No persistent toolbar/status mascot" yields on Home only.
    - "Place Dif beside the landing guidance" now applies to the first-launch card only.
    - The 64 px reviewed floor does not govern the corner; the rendered corner needs its own attended verdict (see below).
  - Kept unchanged: B5 reachability on signed-out and signed-in Home and over a review; the one-sprite rule; B6's AI tools slot on review surfaces; the preferences and the lifecycle rules.
  - Recorded in RFC-BPR-0032's Home section ("Dif on Home (owner direction, 2026-09-15)"); RFC-BPR-0031 is settled and not edited.
- **Bounded out of this card.**
  - Resting in the corner while a review is read, and not only on Home, is window-level. It would change RFC-BPR-0031's B6 and "all other surfaces hide it" courses, so it is carried as a follow-up for main to allocate.
  - Dif's controls on the Appearance page belong to `appearance` (BPR-0790, D1 Appearance part).
- **Signed-out Recently viewed.** First launch has no reading history, so nothing renders there. A signed-out Home with public reading history keeps BPR-0733's Recently viewed: RFC-BPR-0032 lists what it removes, and this is not on the list.
- **The session line says what the projection carries.** It reads "Signed in as {login} on {host}." for the one accepted session, "Connecting to {host}." while connecting, and "Not signed in. Public pull requests open read-only." otherwise. There is no second account and no refresh time, because no projection carries either. A host failure and an unavailable saved layout are still appended; review progress and review history are the review surface's.
- **The stack's empty page.** It is hidden and parented in the pull-request column. A first load in progress is said under the bar without Copy details. A session failure is said only on its host's card (Last problem and Copy details).
- **Pending human evidence.** The corner rendering at 40 px, readable edges and out of the way, gets its own attended candidate question in `BPR-0779` (one question per criterion), separate from A8's Home boards. Nothing attended is inferred here.

### Verification — d3

| Check | Direct surface and command | Result |
|---|---|---|
| Qt Home courses | `ctest --test-dir build/dev -R '^(qt_workbench_window_scale_(100\|125\|200)\|qt_composed_github_scale_(100\|125\|200))$'` on the tree committed as `edb90493` | All six pass (scratchpad `ctest-d3-0775-qt10.log`, `ctest-d3-0775-gh11.log`, `ctest-d3-0775-scales.log`) |
| Build and format | `git clang-format` on the six touched C++ files, `git diff --check`, then `cmake --build build/dev` | No remaining format difference, diff check clean, the whole dev tree builds (`format-d3-0775.log`, `build-d3-0775-all.log`) |
| Integration selection | d2's accounts-page selection plus `application_profile_management_public_path`, `application_profile_window_review_restoration_public_path`, `application_device_authorization_public_path`, `qt_dif_runtime` and `dif_assets_contract` (67 courses) | 60 pass, including the restoration, multi-window, window layout, setup destination and queue composition courses. Seven fail, with markers identical to d2's baseline logs. Six are the known set, compared marker by marker with `ctest-final-0774.log`. The seventh, `application_device_authorization_public_path`, matches d2's head-control run at `53de5f35` on all 38 markers: the credential store is unavailable at stage 1, before any Home gesture (`ctest-d3-0775-integration.log`) |
| Negative controls | `negative_control_0775.py` plants Home's old "No pull request is open" line, and separately a Dif that never rests in the corner; each is rebuilt and run at 100% | Both caught: `qt_composed_github_scale_100` fails at "the status bar says the signed-out session line", and `qt_workbench_window_scale_100` fails at "Home wins and rests the same presentation in the bottom-left corner". Both reverted with no marker left; after a full rebuild both courses pass (`negative-control-0775.log`, `ctest-d3-0775-postneg.log`) |

Limitations: offscreen rendering is layout evidence only. Nothing native, VoiceOver or attended is claimed. The rendered Home boards and the Dif corner verdict belong to `BPR-0779`.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
