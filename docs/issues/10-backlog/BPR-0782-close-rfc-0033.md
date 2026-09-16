---
schema: "implement-rfc.card.v1"
id: "BPR-0782"
title: "Verify and close RFC-BPR-0033"
status: "Backlog"
rfcs: ["RFC-BPR-0033"]
depends_on: ["BPR-0779", "BPR-0780"]
closeout_for: ["RFC-BPR-0033"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"verification-closeout","acceptance":["B1","B2","B3","B4","B5"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0033

RFC base: `38f32226351f5cc8e4657d55c3e151d8450c6ce5`.

This card tracks formal closeout for its RFC. The checklist rows are the canonical
criteria of the live RFC; every row records the check run, its result and the commit.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **B1 — A close is a close.** Closing the last window of the last open profile never presents the chooser: on macOS the application stays resident with its menu bar and Dock icon and no window; on Windows and Linux it quits through the existing drain and Quit path; the restore records distinguish this close from Quit as RFC-BPR-0024 requires. | **Pass on macOS and by injected policy; Windows and Linux are NOT RUN.** `application_profile_management_public_path` proves the last close presents no chooser, both injected `empty_desktop_policy` outcomes, and that the restore records still tell this close from a Quit as RFC-BPR-0024 requires. **Not run, and recorded as not run rather than as passing:** target-native Windows and Linux quit behaviour. Only the injected quit outcome is proven; `native_empty_desktop_policy()` answers `stay_resident` on this macOS host, so the quit branch is never taken here. RFC-BPR-0033's own verification note keeps this as platform-lane work. No card carrying `platforms` metadata exists for it and none was invented on this lane. | src `48aa547f`; finish `37962922` |
| **B2 — Reopening.** With no window on macOS, application activation, ⌘N, and choosing a profile from the Profiles menu open the most recently used profile's window at Home through the existing intents, without a second runtime owner and without touching another profile. | **Pass through the existing intents; the native half is outstanding.** The public path proves application activation, Command-N and the Profiles menu each open the most recently used profile's window at Home exactly once, under one runtime owner and without touching another profile, via `test_the_desktop_menu_bar_reopens_the_most_recent_profile` and the repointed activation, owner-admission and restart courses. **Unresolved:** that the parentless `qt::desktop_menu_bar` really merges into the macOS menu bar with no window open, and the Dock behaviour behind 'activation', are native facts an offscreen course cannot show; carried as attended candidate `bpr-0779-empty-desktop-dock`. | src `48aa547f`; finish `37962922` |
| **B3 — Failures go to a window.** A failed profile operation with a live window is reported in that window and does not present the chooser; the chooser shows its notice only while open, and is presented for a failure only when no window exists. | **Pass.** With a live window an injected failure is reported in that window - the Profiles menu's status item and the window's status bar - and no chooser is presented; the chooser shows its notice only while open and is presented for a failure only when no window exists, which is the startup case RFC-BPR-0024 already describes. Proven by the desktop composition with injected failures alongside `qt_profile_management_window`. | src `48aa547f`; finish `37962922` |
| **B4 — The chooser is rows.** The Profiles sheet shows one row per profile with avatar, name and state, selects the current profile on open, opens or focuses on Return and double-click, renames and creates in place with Save and Cancel, keeps text on Escape and on failure, shows one failure notice with Try again only after a failure, and has no permanent name field, status sentence or Reload button. | **Pass, with one deliberate wording divergence.** `qt_profile_management_window`, `test_the_sheet_is_rows_edited_in_place` and `test_keyboard_accessibility_and_compact_layout` prove one row per profile with letter avatar, name and state, the current profile selected on open, Return and double-click opening or focusing, in-place rename and create with Save and Cancel, text kept on Escape and on failure, one failure notice with Try again present only after a failure, and the absence of the permanent name field, status sentence and Reload Profiles button from the widget tree. **Divergence:** the notice reuses the existing `profile_management_failure_text` rather than the RFC's illustrative 'Profiles could not be saved. The name is kept.' | src `21a2add7`; finish `8e756fe6` |
| **B5 — Rendered and reachable.** The sheet in its default, renaming, creating and failed states renders at 100, 125, 150 and 200 percent in the four appearances, every control is reachable by keyboard with an accessible name and the row state as its description, and the owner accepts the real macOS application through the attended-review workflow, including closing the last window and reopening from the Dock. | **Partial - the objective half is done, the attended half is outstanding.** `qt_surface_capture_course_scale_100/125/150/200` produces **64 Profiles captures**: default, renaming, creating and failed at four appearances x four scales. The accessibility walk reaches all 6 stops with **unnamed=0**, and the row's state is asserted to be its `Qt::AccessibleDescriptionRole` literally, which is what this criterion asks for. **Outstanding and parked on `BPR-0779` (`human-review`):** the macOS VoiceOver walk, and the owner's own close-the-last-window and reopen-from-the-Dock course, bound to executable sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b`. Carried as `bpr-0779-profiles-rendered`, `bpr-0779-voiceover-profiles` and `bpr-0779-empty-desktop-dock`. | src `ed2b3f14`; captures `scratchpad/d7-captures-final` |

## Outcome

Reconcile every B row with direct evidence or a named unresolved criterion, run one integration pass over the desktop lifecycle and management courses, and record the truthful formal status.

## Acceptance

- All B1–B5 rows reference direct evidence or a specific unresolved criterion.
- One integration pass over the desktop lifecycle, activation and management courses is recorded with its revision.
- Formal closeout obeys canonical order and actual platform and attended results; RFC-BPR-0024 is not reopened.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Integration pass - d7, revision `ed2b3f14`

One integration pass over the desktop lifecycle, activation and management
courses, inside the 34-course selection run at zero pending build actions
(`scratchpad/d7-int-pass-01.log`, **100% passed, 34/34, 271.77 s**):
`application_profile_management_public_path`, `application_profile_activation_public_path`,
`application_profile_owner_admission_public_path`, `application_profile_window_restart_public_path`,
`application_profile_multi_window_public_path` and `qt_profile_management_window` all pass.

**Windows and Linux remain not run.** Only the injected policy outcome is proven;
`native_empty_desktop_policy()` answers `stay_resident` on this macOS host, so the
quit branch is never taken here. This is recorded as not run, never as passing, and
no card carrying `platforms` metadata was invented on this lane to hold it.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
