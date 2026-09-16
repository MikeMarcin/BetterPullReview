---
schema: "implement-rfc.card.v1"
id: "BPR-0778"
title: "Redesign the chooser as rows with in-place rename and create"
status: "Done"
rfcs: ["RFC-BPR-0033"]
depends_on: ["BPR-0777"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"profile-chooser","acceptance":["B4"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 - The component course proves rows, selection on open, Return and double-click, in-place rename and create, and text kept on Escape and failure","outcome":"pass","revision":"21a2add77782927d3520129abe3fdb726ab6fe96","evidence":{"kind":"command","detail":"qt_profile_management_window's test_the_sheet_is_rows_edited_in_place, on the shown sheet (scratchpad d5-ctest-0778-regression.log, 0.24 s). One row per profile carrying the name and the state; the current profile's row selected when the sheet opens, from the profile the composition names; the footer's filled action reading New window for an open profile and Open for a closed one. Rename turns the selected row's name into an editor holding its literal label, with Save and Cancel in place of the footer's buttons, and Save sends the existing rename intent with the selected ID and the literal candidate. New profile appends a row already in its editor with the placeholder \"Profile name\", and Save sends the existing create intent carrying only its label. A refusal keeps both the candidate text and the editor, and keeps the selected row's identity; a later unrelated publication still preserves that refused candidate and the stable rows. Escape cancels the edit, dispatches nothing, leaves the name as it was and takes the pending row with it. test_keyboard_accessibility_and_compact_layout adds Return and double-click: each opens the selected profile through the same existing open intent. Through the shipped composition, the rename, create and activation gestures still drive the real host: application_profile_management_public_path (3.06 s), application_profile_window_restart_public_path (6.69 s), application_profile_multi_window_public_path (72.81 s), application_profile_activation_public_path (5.05 s) and application_profile_owner_admission_public_path (2.42 s) all pass."}},{"criterion":"AC2 - The failure notice appears only after a failure and its Try again reloads; the retired controls are absent from the widget and accessibility trees","outcome":"pass","revision":"21a2add77782927d3520129abe3fdb726ab6fe96","evidence":{"kind":"command","detail":"Same component course: before any failure the sheet shows no status sentence and no retry, and after a refused rename it shows one notice whose sentence is the shared profile-management vocabulary, with a Try again that dispatches the existing reload intent. An accepted edit closes the editor and the notice, and nothing announces \"Profile saved.\" - the renamed row is the confirmation. The retired controls are absent from the widget tree: no permanent name field exists outside an edit (name_editor() is hidden and editing() is false), and findChild finds neither the reloadProfiles button nor the profileRecoveryNotice label that [[BPR-0777]] stopped appending. Negative control (d5-nc0778b-planted.log): changing adopt()'s no-failure branch from Notice->hide() to Notice->show() makes the course fail at \"no status sentence and no retry are shown before a failure\"; a byte-identical revert, sha256 6cc6a3a482e55ab3141df8c6755c75b94535bf445a2e5c14aeb8fb7aae3c057c, passes again (d5-nc0778b-reverted.log). An earlier attempt at this control planted Notice->show() in the constructor instead and PASSED, because adopt() legitimately hides the notice again when no failure is projected; that attempt proved nothing and was replaced by the one above."}},{"criterion":"AC3 - Accessible names, row state descriptions and the declared tab order pass the existing focus checks","outcome":"pass","revision":"21a2add77782927d3520129abe3fdb726ab6fe96","evidence":{"kind":"command","detail":"Same component course: every footer action carries an accessible name, and while an edit is open the in-place editor is named \"Profile name\" and Save and Cancel are named too. Each row's state is its accessible description (Qt::AccessibleDescriptionRole equals \"Open in 2 windows\" for the open profile with two windows and \"Closed\" for the closed one), which is what a screen reader reads after the name. The declared tab order runs rows, New profile, Rename, then the filled action, and the editor's order runs editor, Cancel, Save. present() puts keyboard focus on the rows; Escape with no edit open hides the sheet without opening a profile. Layout: the sheet opens at 440 by 360 and rows measure 44 high through visualItemRect, and the actions stay inside the sheet's bounds. Regression on the same revision (d5-ctest-0778-regression.log, 203.77 s): architecture_boundary_contract, qt_composed_github_scale_100, qt_workbench_window_scale_100, qt_profile_management_window, application_profile_management_host, application_setup_destination_public_path, application_profile_startup_public_path, application_profile_management_public_path, application_profile_multi_window_public_path, application_profile_activation_public_path, application_profile_owner_admission_public_path, application_profile_window_restart_public_path and application_profile_window_layout_public_path all pass; verify_documentation.py reports DOCUMENTATION_OK and verify_documentation_tests.py runs OK. The longest course over this lifecycle, application_profile_window_review_restoration_public_path, also passes on this revision in 544.42 s (d5-ctest-restoration-0778.log). LIMITATION: this is offscreen layout and behaviour evidence only. The sheet's four states at four scales in four appearances, the VoiceOver walk and the owner's verdict are BPR-0779's attended course, not claimed here."}}]
---

# BPR-0778: Redesign the chooser as rows with in-place rename and create

## Current frontier — BPR-0778 accepted; the profile-chooser milestone is complete, bpr-shell-surfaces-20260915 d5 / rfc-integrator-d5 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `profile-chooser`: obligations `BPR-0777:*` and
  `BPR-0778:*`, and both exits. `BPR-0777` closed earlier in this dispatch; this card closes the rest.
- **Candidate.** Local `main`, not pushed: promote `f4bacac5`, claim `df2e7081`, source `21a2add7`, and
  this record in the finish commit. `BPR-0777`'s own commits were `f555c9dd`, `b0830ab8`, `48aa547f`
  and `37962922`.
- **Usable behaviour.** The chooser is the Profiles sheet: a row per profile with a letter avatar, the
  name and its state at the right; the current profile's row selected on open; Return and double-click
  open the selected profile or focus it when it is already open; one filled footer action that says Open
  for a closed profile and New window for an open one, beside New profile and Rename. Rename and New
  profile edit in place with Save and Cancel; Escape cancels and keeps the name. A failure is one notice
  above the rows with a single Try again, and is absent otherwise. No permanent name field, no permanent
  status sentence, no Reload Profiles button, and nothing announces "Profile saved."
- **Exit status.** `profile-chooser:exit:2` is met by the component course, with the rendered and attended
  verdicts left to `BPR-0779`. `profile-chooser:exit:1` was met on `BPR-0777` for what this machine can
  observe; Windows and Linux **native** quit behaviour remains not run and belongs to the platform lanes.
- **Concurrent writer in this checkout.** Another session settled `BPR-0783`'s parked owner question in
  this same tree during this dispatch (`fda85932`, `cf819a1c`, `8bac7e4b`, `6357e950`), so **home-page is
  no longer human-pending**. Its commits touch no file of this milestone; every commit here named explicit
  paths. Its quiescence is unknown: it is not this integrator's child and was not reachable from here.
- **Next.** `appearance` (`BPR-0789`, `BPR-0790`) is independent and unclaimed; `verification-closeout`
  (`BPR-0779`, `BPR-0780`, `BPR-0781`, `BPR-0782`, `BPR-0791`) now waits only on `appearance`.

### Decisions — d5

- **The window count comes from the composition, not a new projection field.** RFC-BPR-0033's row reads
  "Open in 2 windows", but `profile_management_item` carries a runtime state and no count, and the RFC
  changes neither the core nor the application host. The desktop, which already answers
  `windows(profile)`, hands the sheet those counts; Qt counts nothing and no record changed.
- **The current profile is told to the sheet, not inferred.** `set_current_profile()` is called with the
  invoking window's profile from the window's Manage Profiles closure, and with the most recently used
  profile from the menu-bar and no-window paths, so "the current profile's row is selected on open" is
  exact rather than "row 0".
- **One failure vocabulary, unchanged.** The sheet's notice reuses `profile_management_failure_text`
  rather than the RFC's illustrative "Profiles could not be saved. The name is kept." `BPR-0777` is
  already accepted against those sentences and the window says the same thing about the same refusal;
  one vocabulary across both surfaces is what B3 and B4 together require.
- **Activation is the row's gesture; the footer offers a new window.** Return and double-click dispatch
  the existing open intent, which focuses an already-open profile. The footer's filled action therefore
  means New window for an open profile. Three courses drove activation through that button and now use
  Return: the management, multi-window and window-restart courses.
- **The radius is painted, not styled.** RFC-BPR-0018 forbids a stylesheet, so the sheet's one 8-point
  radius is drawn from the palette's own Base and Mid roles.

## Outcome

Rebuild `profile_management_window` as the Profiles sheet: rows with avatar, name and state; current profile selected on open; Open or New window as the filled footer action; Rename and New profile editing in place with Save and Cancel; one failure notice with Try again only after a failure; no permanent name field, status sentence or Reload button; the same dispatch and projection.

## Acceptance

- The component course proves rows, selection on open, Return and double-click, in-place rename and create, and text kept on Escape and failure.
- The failure notice appears only after a failure and its Try again reloads; the retired controls are absent from the widget and accessibility trees.
- Accessible names, row state descriptions and the declared tab order pass the existing focus checks.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Verification — d5, source revision `21a2add77782927d3520129abe3fdb726ab6fe96`

| Check | Direct surface and command | Result |
|---|---|---|
| The sheet's component course | `ctest --test-dir build/dev -R '^qt_profile_management_window$'` | Passes in 0.24 s: rows with name and state, the current profile selected on open, Return and double-click, in-place rename and create, Escape and refusal both keeping the text, the notice only after a failure with its Try again, the retired controls absent, accessible names and each row's state as its accessible description, 440 by 360 with 44-high rows (`d5-ctest-0778-regression.log`) |
| The same gestures through the shipped composition | `application_profile_management_public_path`, `application_profile_window_restart_public_path`, `application_profile_multi_window_public_path`, `application_profile_activation_public_path`, `application_profile_owner_admission_public_path` | 3.06 s, 6.69 s, 72.81 s, 5.05 s and 2.42 s, all pass; create, rename and activation reach the real host through the sheet's own gestures |
| Regression selection | The thirteen courses listed on `BPR-0777`, rerun after this change and after formatting | All pass, 203.77 s (`d5-ctest-0778-regression.log`) |
| Window restoration across restarts | `ctest --test-dir build/dev -R '^application_profile_window_review_restoration_public_path$'`, rerun for this change | Passes in 544.42 s (`d5-ctest-restoration-0778.log`) |
| Negative control | Change `adopt()`'s no-failure branch from `Notice->hide()` to `Notice->show()`, rebuild, run | Fails at "no status sentence and no retry are shown before a failure"; a byte-identical revert (sha256 `6cc6a3a4…`) passes again (`d5-nc0778b-planted.log`, `d5-nc0778b-reverted.log`). A first attempt planted the same call in the constructor instead and **passed**, because `adopt()` legitimately hides the notice again when no failure is projected; it proved nothing and was replaced |
| Build and format | Whole `build/dev` build; `git clang-format --force` on the touched files; `git diff --check` | Build exit 0; two files reformatted and rebuilt before the evidence runs; check clean (`d5-build-0778-6.log`) |
| Documentation | `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py` | `DOCUMENTATION_OK`, exit 0; tests OK (`d5-doc-gate-0778.log`, `d5-doc-tests-0778.log`) |

Limitations. This is offscreen layout and behaviour evidence only. The sheet's default, renaming, creating
and failed states at four scales in four appearances, the keyboard walk, VoiceOver names and descriptions,
and the owner's verdict are `BPR-0779`'s attended course and are not claimed here. Another session was
committing unrelated work into this checkout while these courses ran; its contents are ancestors of this
revision and touch none of this card's files.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
