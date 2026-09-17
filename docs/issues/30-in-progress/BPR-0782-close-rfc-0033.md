---
schema: "implement-rfc.card.v1"
id: "BPR-0782"
title: "Verify and close RFC-BPR-0033"
status: "In Progress"
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
| **B1 — A close is a close.** Closing the last window of the last open profile never presents the chooser: on macOS the application stays resident with its menu bar and Dock icon and no window; on Windows and Linux it quits through the existing drain and Quit path; the restore records distinguish this close from Quit as RFC-BPR-0024 requires. | **The macOS half is met on the fixed build, and the owner's re-verdict has now been taken. Passes by injected policy. Windows and Linux are NOT RUN.** `application_profile_management_public_path` proves the last close presents no chooser, both injected `empty_desktop_policy` outcomes, and that the restore records still tell this close from a Quit as RFC-BPR-0024 requires. **The macOS half was false when the owner reviewed it.** On 2026-09-16 they closed the last window on the shipped build (sha256 `d500ef7a…`, source `ed2b3f14`) and reported: *"Closing last window closes menu bar."* Attended candidate `bpr-0779-empty-desktop-dock`, decision f4a23296, rejected. d8 measured it natively and **d9 re-ran that measurement before believing it**: the bar was never lost - longest stretch without the Profiles menu **0 ms** - but the close took **View** off the bar and `Preferences…` out of the application menu, leaving `Apple` + `~File` + `Profiles` against `Apple` + `~File` + `Profiles` + `View` with a window. RFC-BPR-0033's own words are "the menu bar remains (File, Profiles, View and the rest)". **[[BPR-0796]] fixed it at `1c40d04b`.** `qt::desktop_menu_bar` now carries the window menu's View - Line numbers, One pane and `Settings…` - offered and disabled, and `Settings…` keeps the preferences role that keeps the merged item in the application menu; the desktop retires its own bar when a profile window becomes active, so a parentless bar no longer takes the menu bar from a window that has one. `application_profile_empty_desktop_native_menu_bar` now snapshots the windowed bar and requires the windowless bar to equal it - same visible titles, same application-menu items, same View items - and it failed on exactly that comparison before the change (`scratchpad/d9-course-fails-first.log`). After it, both bars read `Apple[Preferences...;…]` + `~File[]` + `Profiles[Default;New Window;Manage Profiles…]` + `View[Line numbers;One pane]`, with a 0 ms gap through the close and through every reopening, on executable sha256 `1f60b0a2…` at zero pending build actions (`scratchpad/d9-course-after-fix-2.log`). **The owner rejected the fixed build too** (`bpr-0796-empty-desktop-menu-bar`, c5c571e8): *"When I close the last window it stays running in the bottom dock but not in the top menu bar."* **d10 settled it on an unlocked console, and the fix stands.** Their shell history shows they launched the pinned binary at 07:05:48 and ran `shasum -a 256` on that same path twenty seconds later; it records the commands and not their output, so what it establishes is which binary they launched. With the application in front, the close is invisible: the menu-bar strip photographed with a window open and after the close is the **same PNG**, sha256 `5bb204b1…7858`. So **AppKit does repaint** a windowless active application's bar, which was the open question here, and the **Dock tile was clicked for real** - it reopens exactly one window at Home. What the owner's sentence describes is reproducible on the same build in one gesture, and it is not about menus: `[NSApp mainMenu]` is this process's property and reads the same whoever is active, while the bar on screen belongs to the **active** application alone. Close the last window while this application is not the active one - which a click on an inactive window's close button does - and the process is alive, windowless, in the Dock, with another application's bar on screen (`/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/d10-evidence/cap-d-inactive-bar.png`). A plain AppKit application, built for the comparison and run six times, and `Preview` behave identically: in the four runs where this console let the reference become active at all, it stayed active through its last window's close, four out of four. So this is macOS and not this pack. The course now also pins the half it was blind to - a close watched while this process is active must leave it active - counted over every close in a run rather than asserted on one, because a strict per-close form failed twice here with nothing injected while another application took the front. Making every close resign activation fails the committed contract; a single deactivation on the first close is reported and passes. Both injected sources and both logs are kept. **The hide-and-unhide gesture ran for the first time** - every earlier session was screen-locked - and failed, on an assertion that was wrong about AppKit rather than about this application: `-[NSApplication unhide:]` restores windows and does not take activation. It now asks for activation and records whether macOS granted it. **What was the owner's alone** - whether *they* closed that window while this application was in front - was put to them on a replacement candidate carrying the photographs. **THE OWNER ANSWERED ON 2026-09-16 AND THIS CLAUSE IS CLOSED.** They accepted `bpr-0796-empty-desktop-menu-bar-activation` at 15:46:17Z - decision `71d8fde6-594d-4fc8-aaa4-d049b58ac125`, fingerprint `a10a4230…c464b6`, bound to `1c40d04b` / app sha256 `1f60b0a2…c17d` / courses d10, which is the build this row describes - with the note, verbatim: *"I don't care about this right now. move on."* The note is an instruction to stop asking rather than a report of having watched the clauses the candidate named, and it is recorded that way on [[BPR-0796]] rather than rounded up into an observation. What it settles is the attended gate; the objective support for the clauses stays d10's, above. Per the owner's own words the question is closed: no further candidate about it is prepared and nothing reopens it. **Not run, and recorded as not run rather than as passing:** target-native Windows and Linux quit behaviour; `native_empty_desktop_policy()` answers `stay_resident` on this macOS host, so the quit branch is never taken here. [[BPR-0798]] and [[BPR-0799]] carry it per platform. | src `1c40d04b`; measured `a85a1365`, `d500ef7a…` then `1f60b0a2…` |
| **B2 — Reopening.** With no window on macOS, application activation, ⌘N, and choosing a profile from the Profiles menu open the most recently used profile's window at Home through the existing intents, without a second runtime owner and without touching another profile. | **Passes through the existing intents, and now at the AppKit level on the fixed build as well.** The public path proves application activation, Command-N and the Profiles menu each open the most recently used profile's window at Home exactly once, under one runtime owner and without touching another profile, via `test_the_desktop_menu_bar_reopens_the_most_recent_profile` and the repointed activation, owner-admission and restart courses. **What was carried here as unresolved - "that the parentless `qt::desktop_menu_bar` really merges into the macOS menu bar with no window open" - is answered, and it is true**: with no window `[NSApp mainMenu]` **is** `desktop_menu()->toNSMenu()`. What was wrong was the bar's contents, which is B1's row and [[BPR-0796]]. **The native gestures are proven at AppKit level on the fixed source `1c40d04b`**: the Dock's `applicationShouldHandleReopen:hasVisibleWindows:NO` delegate message, Command-N synthesized as an `NSEvent` through `-[NSApplication sendEvent:]` with no key window, and `performActionForItemAtIndex:` on the native Profiles - Default item each reopen Default at Home exactly once under one runtime owner, and each further close returns the resident bar with a 0 ms gap - now measured in both directions, because the desktop's bar is retired when a window becomes active and rebuilt when the desktop empties. Quit from the merged application-menu item quits the desktop. **A defect this criterion never reached was found and fixed on the way**: once the desktop had built its bar, focusing the workbench's menubar-less Settings window handed the macOS menu bar to the desktop's bar, taking it from the window that has one; the course now pins that after a reopening, not only before the first close. **d10 ran the whole set on a real screen**, the first console in this pack that was unlocked: a literal Dock-tile click, ⌘N and the native Profiles item each reopen one window at Home, the Settings-focus check holds, Quit quits, and the windowless bar is photographed and identical to the windowed one. The hide-and-unhide gesture ran too, and its assertion turned out to be wrong about AppKit - `unhide:` restores windows and does not take activation - so it now asks for activation and records whether macOS granted it, which it does intermittently. **What was still not observed** was nothing about this build: it was what the owner did on their own console when they closed that window, which is B5's question and rode with the replacement candidate. **THE OWNER ANSWERED ON 2026-09-16 AND THIS CLAUSE IS CLOSED.** They accepted `bpr-0796-empty-desktop-menu-bar-activation` at 15:46:17Z - decision `71d8fde6-594d-4fc8-aaa4-d049b58ac125`, fingerprint `a10a4230…c464b6`, bound to `1c40d04b` / app sha256 `1f60b0a2…c17d` / courses d10, which is the build this row describes - with the note, verbatim: *"I don't care about this right now. move on."* The note is an instruction to stop asking rather than a report of having watched the clauses the candidate named, and it is recorded that way on [[BPR-0796]] rather than rounded up into an observation. What it settles is the attended gate; the objective support for the clauses stays d10's, above. Per the owner's own words the question is closed: no further candidate about it is prepared and nothing reopens it. | src `1c40d04b`; earlier `48aa547f`, finish `37962922` |
| **B3 — Failures go to a window.** A failed profile operation with a live window is reported in that window and does not present the chooser; the chooser shows its notice only while open, and is presented for a failure only when no window exists. | **Pass.** With a live window an injected failure is reported in that window - the Profiles menu's status item and the window's status bar - and no chooser is presented; the chooser shows its notice only while open and is presented for a failure only when no window exists, which is the startup case RFC-BPR-0024 already describes. Proven by the desktop composition with injected failures alongside `qt_profile_management_window`. | src `48aa547f`; finish `37962922` |
| **B4 — The chooser is rows.** The Profiles sheet shows one row per profile with avatar, name and state, selects the current profile on open, opens or focuses on Return and double-click, renames and creates in place with Save and Cancel, keeps text on Escape and on failure, shows one failure notice with Try again only after a failure, and has no permanent name field, status sentence or Reload button. | **Pass, with one deliberate wording divergence.** `qt_profile_management_window`, `test_the_sheet_is_rows_edited_in_place` and `test_keyboard_accessibility_and_compact_layout` prove one row per profile with letter avatar, name and state, the current profile selected on open, Return and double-click opening or focusing, in-place rename and create with Save and Cancel, text kept on Escape and on failure, one failure notice with Try again present only after a failure, and the absence of the permanent name field, status sentence and Reload Profiles button from the widget tree. **Divergence:** the notice reuses the existing `profile_management_failure_text` rather than the RFC's illustrative 'Profiles could not be saved. The name is kept.' | src `21a2add7`; finish `8e756fe6` |
| **B5 — Rendered and reachable.** The sheet in its default, renaming, creating and failed states renders at 100, 125, 150 and 200 percent in the four appearances, every control is reachable by keyboard with an accessible name and the row state as its description, and the owner accepts the real macOS application through the attended-review workflow, including closing the last window and reopening from the Dock. | **MET as of 2026-09-16, on the third candidate, with the acceptance recorded for exactly what it is.** The objective half is done and two of the three attended questions were accepted; the one this criterion names by name was rejected, and the defect behind it is fixed. `qt_surface_capture_course_scale_100/125/150/200` produces **64 Profiles captures**: default, renaming, creating and failed at four appearances x four scales. The accessibility walk reaches all 6 stops with **unnamed=0**, and the row's state is asserted to be its `Qt::AccessibleDescriptionRole` literally, which is what this criterion asks for. **Attended on 2026-09-16, bound to executable sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b` from source `ed2b3f14`: two of three accepted, one rejected.** The rendered sheet was accepted (`bpr-0779-profiles-rendered`, ef0a7de2) and so was the macOS VoiceOver walk over it (`bpr-0779-voiceover-profiles`, 86bd6b22). **The owner's own close-the-last-window and reopen course was rejected** (`bpr-0779-empty-desktop-dock`, f4a23296, *"Closing last window closes menu bar."*), and this criterion names that course explicitly. **[[BPR-0796]] fixed the defect at `1c40d04b`** and d9 prepared one candidate against the fixed executable, sha256 `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`. **The owner performed that one on 2026-09-16 and rejected it as well** (`bpr-0796-empty-desktop-menu-bar`, c5c571e8): *"When I close the last window it stays running in the bottom dock but not in the top menu bar."* **d10 adjudicated it rather than re-fixing anything.** On the first unlocked console in this pack, the bar after the close is photographed and is the same file as the bar with a window open, and the owner's sentence is reproduced exactly by closing the last window while the application is *not* the active one, which is macOS's behaviour for every application and not this pack's. `src/` did not move. What could not be recovered is which of those two the owner did, so a **replacement candidate** is prepared and not performed: `bpr-0796-empty-desktop-menu-bar-activation`, fingerprint `a10a4230…`, bound to the same executable, carrying all three photographs, telling the reviewer how to tell which application owns the bar, and asking that one question. A verdict on one candidate cannot be carried onto another, so this criterion waited for the owner. **THE OWNER ANSWERED ON 2026-09-16 AND THIS CLAUSE IS CLOSED.** They accepted `bpr-0796-empty-desktop-menu-bar-activation` at 15:46:17Z - decision `71d8fde6-594d-4fc8-aaa4-d049b58ac125`, fingerprint `a10a4230…c464b6`, bound to `1c40d04b` / app sha256 `1f60b0a2…c17d` / courses d10, which is the build this row describes - with the note, verbatim: *"I don't care about this right now. move on."* The note is an instruction to stop asking rather than a report of having watched the clauses the candidate named, and it is recorded that way on [[BPR-0796]] rather than rounded up into an observation. What it settles is the attended gate; the objective support for the clauses stays d10's, above. Per the owner's own words the question is closed: no further candidate about it is prepared and nothing reopens it. The objective half of this row - 64 Profiles captures, the 6-stop accessibility walk with `unnamed=0`, the accepted rendered sheet and the accepted VoiceOver walk - was already complete and is unaffected. | src `1c40d04b`; captures `scratchpad/d7-captures-final`, 400 files, SHA-256 manifest `ec9e6100cdebff710f77b54dcdf157e298017529c7b27cfbf657b740182384f6` (d8); candidate bound to `1f60b0a2…` |

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

## Formal status: RFC-BPR-0033 stays `Proposed`, for one reason now

**The reason that has gone.** B5 asks in its own words for "the owner accepts the
real macOS application through the attended-review workflow, including closing the
last window and reopening from the Dock". On 2026-09-16 the owner rejected that
course twice — once on the shipped build and once on the fixed one — and d10
prepared a third candidate, deliberately unperformed, because the dispatch that
decides what a reviewer sees does not get to accept it. **They answered it the same
day: `accept`, with the note "I don't care about this right now. move on."** That
clears the gate B5 names, on the build B5 is about. It is not a fresh observation
of the empty desktop, and nothing here pretends it is; the objective support stays
d10's photographs and courses, and [[BPR-0796]] records the distinction. Per the
owner's instruction the question is now closed and no further candidate about it
will be prepared.

**The reason that remains: closure is ordered and this RFC's dependency is not closed.** The RFC
index is the sequencing authority and says "Closure remains strictly ordered: no
RFC becomes `Auto-Verified` before every listed dependency is `Auto-Verified`."
RFC-BPR-0033 depends on RFC-BPR-0024, which is `Deferred`; its closeout card is
[[BPR-0595]]. So with B1, B2 and B5 now satisfied on this
platform, this RFC still cannot close today, and the status is held where it is
rather than moved. This milestone's own boundary asks for precisely that: "formal
closeout follows canonical order and never falsifies a status."

B3 and B4 are met and are not in question. The Profiles sheet itself was
accepted, rendered and by VoiceOver. What is left is a dependency that is
`Deferred`, and B1's Windows and Linux halves, which have never been observed and
are recorded as not run on [[BPR-0798]] and [[BPR-0799]] rather than carried here
as met.

## Where the frontier is now - this closeout is finished

This card is Done and no longer carries the pack's continuation. RFC-BPR-0033's
five B rows are reconciled, one integration pass is recorded at the final source
revision, and the formal status is held at `Proposed` for the one reason that
remains. What d10 wrote here about the empty desktop, the photographs and the
activation question is preserved below and in [[BPR-0796]], which is Done.

**The pack's one open card is now [[BPR-0797]]**, which the owner reported in the
same sitting and which belongs to none of these three RFCs. Its "Current
frontier" section carries the live continuation; this card links to it rather
than copying it.

- **Binding, as discharged.** Plan `bpr-shell-surfaces-v1`, milestone
  `verification-closeout`, obligations `BPR-0779:*`, `BPR-0780:*`, `BPR-0781:*`,
  `BPR-0782:*`, `BPR-0791:*` and both exits. All five are Done.
- **Follow-ups allocated in d9, record-only:** [[BPR-0800]] (Dif resting in the
  corner on review surfaces, the remainder of owner mandate r2) and [[BPR-0801]]
  (a whole-queue refresh intent). Neither is implemented and neither belongs to
  this pack's RFCs.
- **Platform work still open:** [[BPR-0798]] and [[BPR-0799]] carry B1's Windows
  and Linux halves, which have never been observed.
- **Where d10's evidence lives**, unchanged and still cited by the rows above:
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/d10-evidence/`, with its own
  `README.md`. It is outside the repository and outside the session scratchpad,
  because `scratchpad/…` paths are session-local and the ones d8 and d9 cite no
  longer resolve.

### Integration pass - d13, source revision `a78eefcf`

Run because `src/app` moved after d10's pass. [[BPR-0797]] changed
`src/app/src/github_application.cpp`, `src/app/include/bpr/app/github_application.hpp`
and `src/app/main.cpp` at `b1f89533` and `a78eefcf`, and this RFC's desktop
lifecycle, activation and management courses all drive the composed application,
so d10's pass at `1c40d04b` no longer describes the binary in the tree. d10's pass
is not deleted and is not superseded for the revision it names; this one is the
current evidence.

**15 courses, 100% passed, 15/15, 698.47 s**, zero `***Failed` lines
(`build/bpr0782-d13-int-pass.log`). The set is this RFC's own surface: the four
`application_profile_*` host courses, `qt_profile_management_window`,
`profile_window_restoration_fixture_contract`, and the nine composed public
paths - `application_profile_startup_public_path`,
`application_profile_management_public_path`,
`application_profile_empty_desktop_native_menu_bar`,
`application_profile_multi_window_public_path`,
`application_profile_activation_public_path`,
`application_profile_owner_admission_public_path`,
`application_profile_window_restart_public_path`,
`application_profile_window_layout_public_path` and
`application_profile_window_review_restoration_public_path`.

Measured at zero pending build actions - `ninja -C build/dev -n` reported "no work
to do" immediately before the run - against application binary sha256
`abe15f40ba8575e69d84e74686e385b77a688ddd14130635c3a867f6d895d577`. The native
course `application_profile_empty_desktop_native_menu_bar`, which is the one that
carries [[BPR-0796]]'s contract, ran on an unlocked console and passed, so the
menu-bar behaviour the owner's verdict closes is still the behaviour in the tree.

Also re-run at this revision and recorded under [[BPR-0797]] rather than repeated
here: the isolation verifier's 46 positive controls (OK), the product documentation
gate (`DOCUMENTATION_OK`), and `test_network_isolation_manifest`, which stays red
on failures that predate this pack plus one new true finding about a row that
escapes the registration seam.

**Not run, and not made good by this pass:** target-native Windows and Linux quit
behaviour. `native_empty_desktop_policy()` answers `stay_resident` on this macOS
host, so the quit branch is never taken here at all. [[BPR-0798]] and
[[BPR-0799]] carry it per platform.

### Integration pass - d10, source revision `1c40d04b`, courses moved

`src/` is byte-identical to `1c40d04b` and the application binary is unchanged at
sha256 `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`, verified
before and after every build in this dispatch. The pass was run anyway, by the
integrator, over the same 48-course selection d8 and d9 used, so all three are
comparable: **48 courses, 100% passed, 48/48, 880.52 s**, with `ninja -C build/dev -n`
reporting "no work to do" and the executable's digest printed immediately before and
immediately after, in the same command
(`/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/d10-evidence/d10-int-pass-01.log`).

Beside it: the seven-course desktop-lifecycle, management and settings selection
**7/7** (`d10-focused-1.log`), and the changed course **nine** consecutive times,
9/9, spread over several minutes (`d10-stab1.log` … `d10-stab9.log`), each at zero
pending build actions. `test_network_isolation_manifest` was **not** re-run: d10 adds
no test registration and changes no `LOOPBACK_PORT_COUNT`, so d8's run of it stands.

**One thing about this console belongs in the record**, because every activation
measurement had to be read against it: something takes the front back within about
two seconds of any other application being activated - `Finder`, the plain AppKit
reference, and this application with a window open, alike. The application that ends
up in front is `Google Chrome`; **what drives it was not identified**, and a person
using the machine and an automation driving the browser are indistinguishable from
here. It reclaims rather than grabs - a passive sample of sixty observations with
nothing activated never changed hands. It is why the activation contract counts
closes rather than asserting one, and why every photograph was taken only after
`lsappinfo front` confirmed which application held the front.

### Integration pass - d9, source revision `1c40d04b`

One integration pass at the pack's combined boundary, run by the integrator, at
**zero pending build actions**: the single command that ran the pass ran
`ninja -C build/dev -n` immediately before it and immediately after it, and both printed
"no work to do" (`scratchpad/d9-int-pass-01-task.txt`, that command's own transcript;
the ctest output itself went to the log below). `ninja -n` still says the same, and the
tree has not been rebuilt since and against the executable the prepared
candidate is bound to, sha256
`1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`.

**48 courses, 100% passed, 48/48, 880.39 s** (`scratchpad/d9-int-pass-01.log`, zero
`***Failed` lines) - the same 48-course selection d8 ran, so the two are comparable:
every `qt_composed_github`, `qt_settings_window`, `qt_workbench_window`, `qt_home_list`,
`qt_anonymous_public_preview`, `qt_device_authorization` and `qt_surface_capture_course`
scale variant, the appearance set, `qt_profile_management_window`,
`qt_settings_effect_boundary`, `application_workbench_host`,
`application_queue_composition_contract`, `application_setup_destination_public_path`,
`architecture_boundary_contract`, `sqlite_workbench_settings_storage`, all eight
`application_profile_*_public_path` courses and
`application_profile_empty_desktop_native_menu_bar`.

Beside it, a 14-course desktop-lifecycle, management and settings selection passed
14/14 in 184 s (`scratchpad/d9-focused-2.log`), and the three decisive courses were
re-run by the integrator rather than accepted from a report
(`scratchpad/d9-decisive-1.log`). `test_network_isolation_manifest` was **not** re-run:
d9 adds no test registration and changes no `LOOPBACK_PORT_COUNT`, so d8's run of it
stands and names nothing new.

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
