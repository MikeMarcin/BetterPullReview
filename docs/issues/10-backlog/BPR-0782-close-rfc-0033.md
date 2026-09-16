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
| **B1 — A close is a close.** Closing the last window of the last open profile never presents the chooser: on macOS the application stays resident with its menu bar and Dock icon and no window; on Windows and Linux it quits through the existing drain and Quit path; the restore records distinguish this close from Quit as RFC-BPR-0024 requires. | **The macOS half is met on the fixed build apart from the Dock tile a person has to look at; the owner's re-verdict is prepared and not taken. Passes by injected policy. Windows and Linux are NOT RUN.** `application_profile_management_public_path` proves the last close presents no chooser, both injected `empty_desktop_policy` outcomes, and that the restore records still tell this close from a Quit as RFC-BPR-0024 requires. **The macOS half was false when the owner reviewed it.** On 2026-09-16 they closed the last window on the shipped build (sha256 `d500ef7a…`, source `ed2b3f14`) and reported: *"Closing last window closes menu bar."* Attended candidate `bpr-0779-empty-desktop-dock`, decision f4a23296, rejected. d8 measured it natively and **d9 re-ran that measurement before believing it**: the bar was never lost - longest stretch without the Profiles menu **0 ms** - but the close took **View** off the bar and `Preferences…` out of the application menu, leaving `Apple` + `~File` + `Profiles` against `Apple` + `~File` + `Profiles` + `View` with a window. RFC-BPR-0033's own words are "the menu bar remains (File, Profiles, View and the rest)". **[[BPR-0796]] fixed it at `1c40d04b`.** `qt::desktop_menu_bar` now carries the window menu's View - Line numbers, One pane and `Settings…` - offered and disabled, and `Settings…` keeps the preferences role that keeps the merged item in the application menu; the desktop retires its own bar when a profile window becomes active, so a parentless bar no longer takes the menu bar from a window that has one. `application_profile_empty_desktop_native_menu_bar` now snapshots the windowed bar and requires the windowless bar to equal it - same visible titles, same application-menu items, same View items - and it failed on exactly that comparison before the change (`scratchpad/d9-course-fails-first.log`). After it, both bars read `Apple[Preferences...;…]` + `~File[]` + `Profiles[Default;New Window;Manage Profiles…]` + `View[Line numbers;One pane]`, with a 0 ms gap through the close and through every reopening, on executable sha256 `1f60b0a2…` at zero pending build actions (`scratchpad/d9-course-after-fix-2.log`). **The owner rejected the fixed build too** (`bpr-0796-empty-desktop-menu-bar`, c5c571e8): *"When I close the last window it stays running in the bottom dock but not in the top menu bar."* **d10 settled it on an unlocked console, and the fix stands.** Their shell history shows they launched the pinned binary at 07:05:48 and checked its digest at 07:06:08, so it was this build. With the application in front, the close is invisible: the menu-bar strip photographed with a window open and after the close is the **same PNG**, sha256 `5bb204b1…7858`. So **AppKit does repaint** a windowless active application's bar, which was the open question here, and the **Dock tile was clicked for real** - it reopens exactly one window at Home. What the owner's sentence describes is reproducible on the same build in one gesture, and it is not about menus: `[NSApp mainMenu]` is this process's property and reads the same whoever is active, while the bar on screen belongs to the **active** application alone. Close the last window while this application is not the active one - which a click on an inactive window's close button does - and the process is alive, windowless, in the Dock, with another application's bar on screen (`scratchpad/d10/i2-after-clickthrough.png`). A plain AppKit application and `Preview` behave identically, so this is macOS and not this pack. The course now also pins the half it was blind to: a close watched while this process is active must leave it active, proved able to fail by injection. **The hide-and-unhide gesture ran for the first time** - every earlier session was screen-locked - and failed, on an assertion that was wrong about AppKit rather than about this application: `-[NSApplication unhide:]` restores windows and does not take activation. It now asks for activation and records whether macOS granted it. **Still the owner's alone:** whether *they* closed that window while this application was in front. A replacement candidate asks exactly that and carries the photographs. **Not run, and recorded as not run rather than as passing:** target-native Windows and Linux quit behaviour; `native_empty_desktop_policy()` answers `stay_resident` on this macOS host, so the quit branch is never taken here. [[BPR-0798]] and [[BPR-0799]] carry it per platform. | src `1c40d04b`; measured `a85a1365`, `d500ef7a…` then `1f60b0a2…` |
| **B2 — Reopening.** With no window on macOS, application activation, ⌘N, and choosing a profile from the Profiles menu open the most recently used profile's window at Home through the existing intents, without a second runtime owner and without touching another profile. | **Passes through the existing intents, and now at the AppKit level on the fixed build as well.** The public path proves application activation, Command-N and the Profiles menu each open the most recently used profile's window at Home exactly once, under one runtime owner and without touching another profile, via `test_the_desktop_menu_bar_reopens_the_most_recent_profile` and the repointed activation, owner-admission and restart courses. **What was carried here as unresolved - "that the parentless `qt::desktop_menu_bar` really merges into the macOS menu bar with no window open" - is answered, and it is true**: with no window `[NSApp mainMenu]` **is** `desktop_menu()->toNSMenu()`. What was wrong was the bar's contents, which is B1's row and [[BPR-0796]]. **The native gestures are proven at AppKit level on the fixed source `1c40d04b`**: the Dock's `applicationShouldHandleReopen:hasVisibleWindows:NO` delegate message, Command-N synthesized as an `NSEvent` through `-[NSApplication sendEvent:]` with no key window, and `performActionForItemAtIndex:` on the native Profiles - Default item each reopen Default at Home exactly once under one runtime owner, and each further close returns the resident bar with a 0 ms gap - now measured in both directions, because the desktop's bar is retired when a window becomes active and rebuilt when the desktop empties. Quit from the merged application-menu item quits the desktop. **A defect this criterion never reached was found and fixed on the way**: once the desktop had built its bar, focusing the workbench's menubar-less Settings window handed the macOS menu bar to the desktop's bar, taking it from the window that has one; the course now pins that after a reopening, not only before the first close. **d10 ran the whole set on a real screen**, the first console in this pack that was unlocked: a literal Dock-tile click, ⌘N and the native Profiles item each reopen one window at Home, the Settings-focus check holds, Quit quits, and the windowless bar is photographed and identical to the windowed one. The hide-and-unhide gesture ran too, and its assertion turned out to be wrong about AppKit - `unhide:` restores windows and does not take activation - so it now asks for activation and records whether macOS granted it, which it does intermittently. **What is still not observed** is nothing about this build: it is what the owner did on their own console when they closed that window, which is B5's question and rides with the replacement candidate. | src `1c40d04b`; earlier `48aa547f`, finish `37962922` |
| **B3 — Failures go to a window.** A failed profile operation with a live window is reported in that window and does not present the chooser; the chooser shows its notice only while open, and is presented for a failure only when no window exists. | **Pass.** With a live window an injected failure is reported in that window - the Profiles menu's status item and the window's status bar - and no chooser is presented; the chooser shows its notice only while open and is presented for a failure only when no window exists, which is the startup case RFC-BPR-0024 already describes. Proven by the desktop composition with injected failures alongside `qt_profile_management_window`. | src `48aa547f`; finish `37962922` |
| **B4 — The chooser is rows.** The Profiles sheet shows one row per profile with avatar, name and state, selects the current profile on open, opens or focuses on Return and double-click, renames and creates in place with Save and Cancel, keeps text on Escape and on failure, shows one failure notice with Try again only after a failure, and has no permanent name field, status sentence or Reload button. | **Pass, with one deliberate wording divergence.** `qt_profile_management_window`, `test_the_sheet_is_rows_edited_in_place` and `test_keyboard_accessibility_and_compact_layout` prove one row per profile with letter avatar, name and state, the current profile selected on open, Return and double-click opening or focusing, in-place rename and create with Save and Cancel, text kept on Escape and on failure, one failure notice with Try again present only after a failure, and the absence of the permanent name field, status sentence and Reload Profiles button from the widget tree. **Divergence:** the notice reuses the existing `profile_management_failure_text` rather than the RFC's illustrative 'Profiles could not be saved. The name is kept.' | src `21a2add7`; finish `8e756fe6` |
| **B5 — Rendered and reachable.** The sheet in its default, renaming, creating and failed states renders at 100, 125, 150 and 200 percent in the four appearances, every control is reachable by keyboard with an accessible name and the row state as its description, and the owner accepts the real macOS application through the attended-review workflow, including closing the last window and reopening from the Dock. | **NOT MET, and now for one reason only: the verdict has not been taken on the fixed build.** The objective half is done and two of the three attended questions were accepted; the one this criterion names by name was rejected, and the defect behind it is fixed. `qt_surface_capture_course_scale_100/125/150/200` produces **64 Profiles captures**: default, renaming, creating and failed at four appearances x four scales. The accessibility walk reaches all 6 stops with **unnamed=0**, and the row's state is asserted to be its `Qt::AccessibleDescriptionRole` literally, which is what this criterion asks for. **Attended on 2026-09-16, bound to executable sha256 `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b` from source `ed2b3f14`: two of three accepted, one rejected.** The rendered sheet was accepted (`bpr-0779-profiles-rendered`, ef0a7de2) and so was the macOS VoiceOver walk over it (`bpr-0779-voiceover-profiles`, 86bd6b22). **The owner's own close-the-last-window and reopen course was rejected** (`bpr-0779-empty-desktop-dock`, f4a23296, *"Closing last window closes menu bar."*), and this criterion names that course explicitly. **[[BPR-0796]] fixed the defect at `1c40d04b`** and d9 prepared one candidate against the fixed executable, sha256 `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`. **The owner performed that one on 2026-09-16 and rejected it as well** (`bpr-0796-empty-desktop-menu-bar`, c5c571e8): *"When I close the last window it stays running in the bottom dock but not in the top menu bar."* **d10 adjudicated it rather than re-fixing anything.** On the first unlocked console in this pack, the bar after the close is photographed and is the same file as the bar with a window open, and the owner's sentence is reproduced exactly by closing the last window while the application is *not* the active one, which is macOS's behaviour for every application and not this pack's. `src/` did not move. What could not be recovered is which of those two the owner did, so a **replacement candidate** is prepared and not performed: `bpr-0796-empty-desktop-menu-bar-activation`, fingerprint `4907795c…`, bound to the same executable, carrying all three photographs, telling the reviewer how to tell which application owns the bar, and asking that one question. A verdict on one candidate cannot be carried onto another, so this criterion stays **not met** until the owner answers it. | src `1c40d04b`; captures `scratchpad/d7-captures-final`, 400 files, SHA-256 manifest `ec9e6100cdebff710f77b54dcdf157e298017529c7b27cfbf657b740182384f6` (d8); candidate bound to `1f60b0a2…` |

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
fixed** — [[BPR-0796]] at `1c40d04b`, with B1's macOS half and B2's native half
met on the fixed build — and the owner reviewed that build and **rejected it too**,
with a sentence about which application owns the menu bar rather than about which
menus are on it. d10 photographed both states on an unlocked console, reproduced
the owner's sentence deliberately by closing the last window while the application
was not in front, and changed no `src/`. One **replacement** candidate is prepared
against the same executable and deliberately **not performed**: the dispatch that
decides what a reviewer sees does not get to accept it, and the mandate asked for
preparation rather than performance. So B5 stays open, and it is open on a person
rather than on the work.

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

## Current frontier - the pack's one open closeout, bpr-shell-surfaces-20260915 d10 / rfc-integrator-d10 / mandate r6

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `verification-closeout`, obligations
  `BPR-0779:*`, `BPR-0780:*`, `BPR-0781:*`, `BPR-0782:*`, `BPR-0791:*` and both exits.
- **What closed.** `BPR-0779`, `BPR-0780`, `BPR-0781` and `BPR-0791` are Done. `BPR-0796` is
  **Parked on `human-review`**: four of its five acceptance bullets pass on the fixed build and
  the fifth is the owner's verdict, which is now a **second rejection** rather than a silence.
- **What the second rejection turned out to be.** *"When I close the last window it stays running
  in the bottom dock but not in the top menu bar."* Not a menu: an **activation** sentence.
  `[NSApp mainMenu]` is this process's property and reads the same whoever is active; the bar
  drawn across the top belongs to the **active** application alone, so an application can have a
  complete menu bar and not be in the menu bar. d10 had the first unlocked console in this pack
  and settled it by photograph: with the application in front, the strip captured with a window
  open and the strip captured after the last close are **the same PNG**
  (`5bb204b1…7858`); close the last window while it is *not* in front - a click on an inactive
  window's red close button does exactly that - and the owner's sentence is reproduced word for
  word. `Preview` and a bare AppKit application behave the same way, so this is macOS. **No
  `src/` change was made or needed**; the executable is still `1f60b0a2…`.
- **What did change, and where.** The course, its probe and the probe header, only. It now pins
  the half it was blind to - a close watched while this process is active must leave it active,
  judged over every close in a run - and that pin was proved able to fail twice by injection.
  The **hide-and-unhide arm ran for the first time in this pack** and failed, because its
  assertion was wrong about AppKit rather than about this application: `-[NSApplication unhide:]`
  restores windows and does not take activation, and current macOS reserves activation for the
  person. It now asks and records the answer. `focus()` falls back to the QPA delivery when the
  console is taken away mid-run instead of failing on it.
- **What is open, and it is still this card.** RFC-BPR-0033 **B5**, and only B5. The one fact
  nobody here can recover is whether the owner closed that window while BetterPullReview was the
  front application. That is what the replacement candidate asks.
- **What a successor does first: nothing but ask.** One candidate is prepared and waiting,
  `bpr-0796-empty-desktop-menu-bar-activation`, fingerprint `4907795c…`, in session
  `e966c88f3320638208adf4fcf0651e31` at
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review`, sequence 88, **19
  candidates with exactly 1 pending**. Serve that state directory and open the loopback URL, or
  use the offline page at
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review/bpr-0796-activation-review.html`
  and feed its export back with `import --file`. It is bound to executable sha256 `1f60b0a2…`
  and protects the three source files, the course, the probe, its header and the built bundle,
  so **do not rebuild `build/dev` before the verdict is in**. It carries three photographs of the
  real menu bar, which the last candidate said were impossible; it tells the reviewer to click
  the window first and how to tell which application owns the bar; and it drops the last one's
  step 8, which asked them to expect View's items **enabled** with Settings focused when at Home
  they are greyed with any window focused. d10 did not perform it and did not wait.
- **On an accept.** Record it on B5, B1 and B2, unpark [[BPR-0796]] and finish it, then this card
  can be finished. RFC-BPR-0033 still does not become `Auto-Verified` - see the formal status
  above; RFC-BPR-0024 is `Deferred`.
- **On a reject.** Read the note for the step, and read it for the activation answer before
  reaching for a code change. Every step has a machine-side counterpart in
  `application_profile_empty_desktop_native_menu_bar`, which as of d10 covers the activation half
  as well.
- **What this casts doubt on, and what it does not.** The *first* rejection
  (`bpr-0779-empty-desktop-dock`, f4a23296, *"Closing last window closes menu bar."*) was read by
  d8 and d9 as a report about the bar's titles, and on that build the close really did take `View`
  and the merged `Preferences…` away. It could equally have been the same activation observation
  as the second one; nothing recorded distinguishes them, and d10 will not pretend otherwise.
  **It does not unsettle the fix.** RFC-BPR-0033 B1 names File, Profiles and View by name, the
  windowless bar was short of them, and [[BPR-0796]] made the close subtract nothing. That is met
  on the RFC's own words whatever the owner was looking at. **It does unsettle the inference** -
  drawn in d8 and repeated in d9 - that the first note had been *accounted for*. What was
  accounted for was a defect the note was consistent with.
- **Not run, and never to be inferred.** Windows and Linux native empty-desktop quit
  ([[BPR-0798]], [[BPR-0799]]). The hide-and-unhide gesture's *reopen* is conditional: macOS
  grants this process activation after an unhide only intermittently, and the course records
  which happened on each run.
- **Follow-ups allocated in d9, record-only:** [[BPR-0800]] (Dif resting in the corner on review
  surfaces, the remainder of owner mandate r2) and [[BPR-0801]] (a whole-queue refresh intent).
  Neither is implemented and neither belongs to this pack's RFCs.
- **Unrelated to this pack, and reported by the owner in the same sitting:** [[BPR-0797]].

### Integration and focused checks - d10, source revision `1c40d04b`, courses moved

`src/` is byte-identical to `1c40d04b` and the application binary is unchanged at
sha256 `1f60b0a267878bd9525488f15fa27e97e778cf006a087b2f1e2d5a9ae4f0c17d`, verified
before and after every build in this dispatch, so d9's 48-course integration pass
still describes this composition and was not repeated for a change that moved no
`src/` file. What moved is the course, its probe and the probe's header, and those
were re-run by the integrator: the seven-course desktop-lifecycle, management and
settings selection **7/7** (`scratchpad/d10/d10-focused-1.log`), and the changed
course five consecutive times, **5/5**, 3.99-6.05 s
(`scratchpad/d10/d10-stab1.log` … `d10-stab5.log`), each at `ninja -C build/dev -n`
reporting "no work to do". `test_network_isolation_manifest` was **not** re-run: d10
adds no test registration and changes no `LOOPBACK_PORT_COUNT`.

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
