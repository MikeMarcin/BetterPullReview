---
schema: "implement-rfc.card.v1"
id: "BPR-0782"
title: "Verify and close RFC-BPR-0033"
status: "Backlog"
rfcs: ["RFC-BPR-0033"]
depends_on: ["BPR-0779", "BPR-0780", "BPR-0796"]
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
| **B1 — A close is a close.** Closing the last window of the last open profile never presents the chooser: on macOS the application stays resident with its menu bar and Dock icon and no window; on Windows and Linux it quits through the existing drain and Quit path; the restore records distinguish this close from Quit as RFC-BPR-0024 requires. | **The macOS half is met on the fixed build; the owner's re-verdict is prepared and not taken. Passes by injected policy. Windows and Linux are NOT RUN.** `application_profile_management_public_path` proves the last close presents no chooser, both injected `empty_desktop_policy` outcomes, and that the restore records still tell this close from a Quit as RFC-BPR-0024 requires. **The macOS half was false when the owner reviewed it.** On 2026-09-16 they closed the last window on the shipped build (sha256 `d500ef7a…`, source `ed2b3f14`) and reported: *"Closing last window closes menu bar."* Attended candidate `bpr-0779-empty-desktop-dock`, decision f4a23296, rejected. d8 measured it natively and **d9 re-ran that measurement before believing it**: the bar was never lost - longest stretch without the Profiles menu **0 ms** - but the close took **View** off the bar and `Preferences…` out of the application menu, leaving `Apple` + `~File` + `Profiles` against `Apple` + `~File` + `Profiles` + `View` with a window. RFC-BPR-0033's own words are "the menu bar remains (File, Profiles, View and the rest)". **[[BPR-0796]] fixed it at `1c40d04b`.** `qt::desktop_menu_bar` now carries the window menu's View - Line numbers, One pane and `Settings…` - offered and disabled, and `Settings…` keeps the preferences role that keeps the merged item in the application menu; the desktop retires its own bar when a profile window becomes active, so a parentless bar no longer takes the menu bar from a window that has one. `application_profile_empty_desktop_native_menu_bar` now snapshots the windowed bar and requires the windowless bar to equal it - same visible titles, same application-menu items, same View items - and it failed on exactly that comparison before the change (`scratchpad/d9-course-fails-first.log`). After it, both bars read `Apple[Preferences...;…]` + `~File[]` + `Profiles[Default;New Window;Manage Profiles…]` + `View[Line numbers;One pane]`, with a 0 ms gap through the close and through every reopening, on executable sha256 `1f60b0a2…` at zero pending build actions (`scratchpad/d9-course-after-fix-2.log`). **Still human-only and never claimed:** whether AppKit *repaints* a windowless active application's bar, the Dock tile and a literal Dock click. The hide-and-unhide activation gesture is **not run**, because this console session is screen-locked, as it was for d8. One attended candidate is prepared against the fixed digest and has not been performed. **Not run, and recorded as not run rather than as passing:** target-native Windows and Linux quit behaviour; `native_empty_desktop_policy()` answers `stay_resident` on this macOS host, so the quit branch is never taken here. [[BPR-0798]] and [[BPR-0799]] carry it per platform. | src `1c40d04b`; measured `a85a1365`, `d500ef7a…` then `1f60b0a2…` |
| **B2 — Reopening.** With no window on macOS, application activation, ⌘N, and choosing a profile from the Profiles menu open the most recently used profile's window at Home through the existing intents, without a second runtime owner and without touching another profile. | **Passes through the existing intents, and now at the AppKit level on the fixed build as well.** The public path proves application activation, Command-N and the Profiles menu each open the most recently used profile's window at Home exactly once, under one runtime owner and without touching another profile, via `test_the_desktop_menu_bar_reopens_the_most_recent_profile` and the repointed activation, owner-admission and restart courses. **What was carried here as unresolved - "that the parentless `qt::desktop_menu_bar` really merges into the macOS menu bar with no window open" - is answered, and it is true**: with no window `[NSApp mainMenu]` **is** `desktop_menu()->toNSMenu()`. What was wrong was the bar's contents, which is B1's row and [[BPR-0796]]. **The native gestures are proven at AppKit level on the fixed source `1c40d04b`**: the Dock's `applicationShouldHandleReopen:hasVisibleWindows:NO` delegate message, Command-N synthesized as an `NSEvent` through `-[NSApplication sendEvent:]` with no key window, and `performActionForItemAtIndex:` on the native Profiles - Default item each reopen Default at Home exactly once under one runtime owner, and each further close returns the resident bar with a 0 ms gap - now measured in both directions, because the desktop's bar is retired when a window becomes active and rebuilt when the desktop empties. Quit from the merged application-menu item quits the desktop. **A defect this criterion never reached was found and fixed on the way**: once the desktop had built its bar, focusing the workbench's menubar-less Settings window handed the macOS menu bar to the desktop's bar, taking it from the window that has one; the course now pins that after a reopening, not only before the first close. **What is still not observed:** the console session was screen-locked for d8 and for d9, so the course delivers the two key-window changes through the QPA entry point the cocoa plugin uses and records the hide-and-unhide activation gesture as **not run**; the Dock tile and a literal Dock click stay human-only, as does whether AppKit repaints a windowless active application's bar. B2's bar is no longer short of menus, and its remaining human half rides with B5's prepared candidate. | src `1c40d04b`; earlier `48aa547f`, finish `37962922` |
| **B3 — Failures go to a window.** A failed profile operation with a live window is reported in that window and does not present the chooser; the chooser shows its notice only while open, and is presented for a failure only when no window exists. | **Pass.** With a live window an injected failure is reported in that window - the Profiles menu's status item and the window's status bar - and no chooser is presented; the chooser shows its notice only while open and is presented for a failure only when no window exists, which is the startup case RFC-BPR-0024 already describes. Proven by the desktop composition with injected failures alongside `qt_profile_management_window`. | src `48aa547f`; finish `37962922` |
| **B4 — The chooser is rows.** The Profiles sheet shows one row per profile with avatar, name and state, selects the current profile on open, opens or focuses on Return and double-click, renames and creates in place with Save and Cancel, keeps text on Escape and on failure, shows one failure notice with Try again only after a failure, and has no permanent name field, status sentence or Reload button. | **Pass, with one deliberate wording divergence.** `qt_profile_management_window`, `test_the_sheet_is_rows_edited_in_place` and `test_keyboard_accessibility_and_compact_layout` prove one row per profile with letter avatar, name and state, the current profile selected on open, Return and double-click opening or focusing, in-place rename and create with Save and Cancel, text kept on Escape and on failure, one failure notice with Try again present only after a failure, and the absence of the permanent name field, status sentence and Reload Profiles button from the widget tree. **Divergence:** the notice reuses the existing `profile_management_failure_text` rather than the RFC's illustrative 'Profiles could not be saved. The name is kept.' | src `21a2add7`; finish `8e756fe6` |
| **B5 — Rendered and reachable.** The sheet in its default, renaming, creating and failed states renders at 100, 125, 150 and 200 percent in the four appearances, every control is reachable by keyboard with an accessible name and the row state as its description, and the owner accepts the real macOS application through the attended-review workflow, including closing the last window and reopening from the Dock. | **NOT MET, and now for one reason only: the verdict has not been taken on the fixed build.** The objective half is done and two of the three attended questions were accepted; the one this criterion names by name was rejected, and the defect behind it is fixed. `qt_surface_capture_course_scale_100/125/150/200` produces **64 Profiles captures**: default, renaming, creating and failed at four appearances x four scales. The accessibility walk reaches all 6 stops with **unnamed=0**, and the row's state is asserted to be its `Qt::AccessibleDescriptionRole` literally, which is what this criterion asks for. **Attended on 2026-09-16, bound to executable sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b` from source `ed2b3f14`: two of three accepted, one rejected.** The rendered sheet was accepted (`bpr-0779-profiles-rendered`, ef0a7de2) and so was the macOS VoiceOver walk over it (`bpr-0779-voiceover-profiles`, 86bd6b22). **The owner's own close-the-last-window and reopen course was rejected** (`bpr-0779-empty-desktop-dock`, f4a23296, *"Closing last window closes menu bar."*), and this criterion names that course explicitly. **[[BPR-0796]] fixed the defect at `1c40d04b`** and d9 prepared exactly one candidate against the fixed executable, sha256 `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`. It was **not performed**: this dispatch was told to prepare, not to perform, and a fix cannot verify itself. A verdict on the old build cannot be carried forward onto a new one, so this criterion stays **not met** until the owner answers the prepared candidate. | src `1c40d04b`; captures `scratchpad/d7-captures-final`, 400 files, SHA-256 manifest `ec9e6100cdebff710f77b54dcdf157e298017529c7b27cfbf657b740182384f6` (d8); candidate bound to `1f60b0a2…` |

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
quit branch is never taken here. This is recorded as not run, never as passing.
Since d8 it has a home per platform, allocated on main: [[BPR-0798]] and
[[BPR-0799]].

## Formal status: RFC-BPR-0033 stays `Proposed`, for two independent reasons

**One, this RFC has a criterion whose verdict has not been taken on the build
that answers it.** B5 asks in its own words for "the owner accepts the real
macOS application through the attended-review workflow, including closing the
last window and reopening from the Dock". On 2026-09-16 the owner ran exactly
that course on the reviewed build and rejected it. A fix cannot turn that into
an acceptance; only a new attended verdict on a fixed build can. **The defect is
now fixed** — [[BPR-0796]] at `1c40d04b`, with B1's macOS half and B2's native
half met on the fixed build — and d9 prepared exactly one candidate against the
fixed executable, sha256 `1f60b0a2…`. It was deliberately **not performed**: the
dispatch that makes a change to what a reviewer sees does not get to accept it,
and the mandate asked for preparation rather than performance. So B5 stays open,
and it is open on a person rather than on the work.

**Two, closure is ordered and this RFC's dependency is not closed.** The RFC
index is the sequencing authority and says "Closure remains strictly ordered: no
RFC becomes `Auto-Verified` before every listed dependency is `Auto-Verified`."
RFC-BPR-0033 depends on RFC-BPR-0024, which is `Deferred`; its closeout card is
[[BPR-0595]]. So even with B1, B2 and B5 satisfied this RFC could not close
today. This milestone's own boundary asks for precisely this behaviour: "formal
closeout follows canonical order and never falsifies a status."

B3 and B4 are met and are not in question. The Profiles sheet itself was
accepted, rendered and by VoiceOver. What is not met is one attended verdict on
the last window, and a dependency that is `Deferred`.

## Current frontier - the pack's one open closeout, bpr-shell-surfaces-20260915 d8 / rfc-integrator-d8 / mandate r5

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `verification-closeout`, obligations
  `BPR-0779:*`, `BPR-0780:*`, `BPR-0781:*`, `BPR-0782:*`, `BPR-0791:*` and both exits.
- **What closed.** `BPR-0779` is Done: the attended review ran, all 17 verdicts are recorded and
  bound to executable sha256 `d500ef7a…` from source `ed2b3f14`. `BPR-0780` was already Done.
  `BPR-0781` and `BPR-0791` are Done: every A and C row carries evidence or a named unresolved
  criterion.
- **What is open, and it is this card.** RFC-BPR-0033's B1, B2 and B5. See above.
- **What a successor needs first.** [[BPR-0796]] holds d8's diagnosis, and it is no longer a
  hypothesis: the bar is not lost, it is down to one visible menu where the RFC names four. The
  next slice is a bounded presentation change to `qt::desktop_menu_bar` with the existing native
  course extended to assert the new contract, and it needs no new decision - RFC-BPR-0033's own
  words are the specification. **Only then** does an attended session make sense, and it needs
  exactly **one** candidate: close the last window, look at the menu bar, then Dock activation,
  ⌘N and the Profiles menu, prepared against the fixed build's own digest. No candidate was
  prepared in d8, because the build has not changed and there would be nothing new to look at.
- **Not run, and never to be inferred.** Windows and Linux native empty-desktop quit: [[BPR-0798]]
  and [[BPR-0799]].
- **Unrelated to this pack, and reported by the owner in the same sitting:** [[BPR-0797]].

### Integration pass - d8, source revision `ed2b3f14`, courses at `a85a1365`

One integration pass at the pack's combined boundary, run by the integrator, at
**zero pending build actions** (`ninja -C build/dev -n` reported "no work to do"
immediately before and immediately after) and against the **same executable the
owner reviewed**, sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b`.

**48 courses, 100% passed, 48/48, 882.21 s** (`scratchpad/d8-int-pass-01.log`,
zero `***Failed` lines): every `qt_composed_github`, `qt_settings_window`,
`qt_workbench_window`, `qt_home_list`, `qt_anonymous_public_preview`,
`qt_device_authorization` and `qt_surface_capture_course` scale variant, the
appearance set (`qt_appearance_*`, `qt_colour_role_list`, `qt_theme_preview_strip`,
`qt_type_preview`, `qt_code_sample_painter`, `application_appearance_host`,
`sqlite_appearance_settings_storage`), `qt_profile_management_window`,
`qt_settings_effect_boundary`, `application_workbench_host`,
`application_queue_composition_contract`,
`application_setup_destination_public_path`, `architecture_boundary_contract`,
`sqlite_workbench_settings_storage`, all seven `application_profile_*_public_path`
courses, and the new `application_profile_empty_desktop_native_menu_bar`.

`src/` is byte-identical to the reviewed revision - `git log ed2b3f14..HEAD -- src/`
is empty - so this pass confirms the pack's behaviour at exactly the revision the
owner judged, rather than at a moved one. `test_network_isolation_manifest` was
run separately because d8 adds a test registration; it fails on the
`.claude/worktrees` checkout's duplicate `add_test` ownership and on
`gh_cli_git_smart_transport`, exactly as before, and names nothing from the new
course (`scratchpad/d8-isolation-manifest.log`).

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
