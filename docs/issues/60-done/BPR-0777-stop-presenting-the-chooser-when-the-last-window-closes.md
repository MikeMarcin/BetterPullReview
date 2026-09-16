---
schema: "implement-rfc.card.v1"
id: "BPR-0777"
title: "Stop presenting the chooser when the last window closes; stay resident on macOS and quit elsewhere"
status: "Done"
rfcs: ["RFC-BPR-0033"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"profile-chooser","acceptance":["B1","B2","B3"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 - Closing the last window presents nothing; the resident or quit outcome follows the injected policy and the restore record distinguishes close from Quit","outcome":"pass","revision":"48aa547fc4a283b0944ffc854f045c222ad732a6","evidence":{"kind":"command","detail":"application_profile_management_public_path through the shipped composition (scratchpad d5-ctest-3.log, 2.54 s). With the resident policy injected, closing the last window of the last open profile leaves the chooser hidden, records last_empty_desktop_outcome() == stay_resident, and leaves the application running with no window. With the quit policy injected, the same close reaches quiescence through the existing drain and Quit path, exit code 0, with no chooser presented on the way out. The platform mapping itself is asserted directly from native_empty_desktop_policy(), which answers stay_resident on this macOS machine. The restore record still tells this close from a Quit: after the quitting close, the next launch opens a window whose identity differs from the explicitly closed one, as RFC-BPR-0024 requires. Negative control (d5-nc-run-planted.log): putting the retired Management->present() back into poll()'s empty-desktop branch makes the course fail at 'closing the last window presents no chooser'; a byte-identical revert, sha256 6b4442a094ba3b1eec7e3a6b3ed6bcc5ba7b346723ad4ba37f10502acbe01ab8, passes again (d5-nc-run-reverted.log). LIMITATION: Windows and Linux native quit behaviour is not run on this machine and stays platform-lane work, exactly as RFC-BPR-0033's verification note states; only the injected quit outcome is proven here."}},{"criterion":"AC2 - Activation, Command-N and the Profiles menu reopen the most recent profile exactly once with one runtime owner","outcome":"pass","revision":"48aa547fc4a283b0944ffc854f045c222ad732a6","evidence":{"kind":"command","detail":"Same course: with no window, each of the three gestures reopens the most recently used profile exactly once - New Window on the application's own menu bar, the platform's activation signal (applicationStateChanged with Qt::ApplicationActive), and choosing that profile by name from the same menu. Each is followed by exactly one window under one runtime owner, arriving at Home, with the chooser still hidden. qt_profile_management_window's test_the_desktop_menu_bar_reopens_the_most_recent_profile proves the menu bar itself (d5-ctest-3.log, 0.25 s): it is parentless, so macOS uses it as the application's default menu bar; Profiles lists the profiles plus New Window and Manage Profiles; New Window is enabled with no runtime open at all and carries QKeySequence::New; the most recently used profile is the checked one; each gesture emits the existing typed open or new-window intent; management and Quit reach the desktop's own closures without inventing a profile intent; a rename keeps the QAction identity and escapes a literal ampersand; a closing profile is not offered. LIMITATION: that this parentless menu bar merges into the real macOS menu bar, and the Dock behaviour behind 'activation', are native facts an offscreen course cannot show; they belong to BPR-0779's attended course."}},{"criterion":"AC3 - A failure with a live window reports there and presents nothing; with no window the chooser is presented; the existing management course still passes","outcome":"pass","revision":"48aa547fc4a283b0944ffc854f045c222ad732a6","evidence":{"kind":"command","detail":"Same course, both directions. With a live window, a refused rename is reported in that window - its status line carries the sentence and its Profiles menu shows the non-actionable item - and the chooser is not presented. With no window, the same refusal presents the chooser, which is the startup case RFC-BPR-0024 describes. qt_profile_management_window covers the window side directly: the item is absent until a refusal, then visible and disabled, carrying profile_management_failure_text's sentence, which also appears in the window's status_text(), and a later accepted projection clears both. The existing management course still passes, and the three courses that selected the retired Reload Profiles button and recovery notice are repointed and pass: application_profile_activation_public_path (4.06 s), application_profile_owner_admission_public_path (1.86 s) and application_profile_window_restart_public_path (6.15 s), the last now observing the failed lifecycle save on the window itself, which is the behaviour B3 requires rather than the retired notice. Regression selection on the same tree (d5-ctest-regression.log, 208.64 s): architecture_boundary_contract, application_profile_management_host, qt_composed_github_scale_100, qt_workbench_window_scale_100, qt_profile_management_window, application_setup_destination_public_path, application_profile_startup_public_path, application_profile_management_public_path, application_profile_multi_window_public_path, application_profile_activation_public_path, application_profile_owner_admission_public_path and application_profile_window_layout_public_path all pass; python3 scripts/verify_documentation.py --root . reports DOCUMENTATION_OK and tests/scripts/verify_documentation_tests.py runs 64 tests OK. The longest course over this lifecycle, application_profile_window_review_restoration_public_path, also passes on this revision in 555.62 s (d5-ctest-restoration.log). LIMITATION: another session was committing unrelated work into this same checkout while these courses ran (RFC-BPR-0032, home_list_tests.cpp and its own cards, commits fda85932, cf819a1c, 8bac7e4b, 6357e950). Those contents were present in the tree during the runs and are now ancestors of this revision; none of them touches this card's files, and every commit here named explicit paths."}}]
---

# BPR-0777: Stop presenting the chooser when the last window closes; stay resident on macOS and quit elsewhere

## Current frontier — BPR-0777 accepted; profile-chooser continues with BPR-0778, bpr-shell-surfaces-20260915 d5 / rfc-integrator-d5 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `profile-chooser`: obligations `BPR-0777:*` and
  `BPR-0778:*`, and both exits. This card closes `BPR-0777:*`; the milestone stays open for the sheet.
- **Candidate.** Local `main`, not pushed: promote `f555c9dd`, claim `b0830ab8`, source `48aa547f`, and
  this record in the finish commit.
- **Usable behaviour.**
  - Closing a window closes a window. The last-runtime branch presents nothing, and an injected
    `empty_desktop_policy`, named once in the composition, decides what an empty desktop does.
  - macOS stays resident: no window, and the application keeps its own menu bar (`qt::desktop_menu_bar`)
    with Profiles, New Window (⌘N) and Quit. Windows and Linux quit through the existing drain and Quit
    path, retried while the close that emptied the desktop is still saving.
  - Activating the application, ⌘N and choosing a profile each reopen the most recently used profile at
    Home, once, under one runtime owner. That profile is an in-process fact; nothing new is persisted.
  - A profile refusal with a live window is reported in that window — an item in its Profiles menu and a
    sentence in its status line — and the chooser is presented for a failure only when no window exists.
  - The desktop appends no recovery notice and no Reload Profiles button to the chooser.
- **Exit status.** `profile-chooser:exit:1` is met for what this machine can observe: no presentation,
  both policy outcomes, and single-owner reopening. Windows and Linux **native** quit behaviour is not
  run and belongs to the platform lanes, as RFC-BPR-0033's verification note states.
  `profile-chooser:exit:2` is `BPR-0778`'s.
- **Concurrent writer in this checkout.** Another session worked in this same tree throughout d5,
  settling `BPR-0783`'s parked owner question (`fda85932`, `cf819a1c`, `8bac7e4b`, `6357e950`). Its
  commits touch RFC-BPR-0032, `home_list_tests.cpp` and its own cards, and none of this card's files;
  every commit here named explicit paths. Its build ran in the same `build/dev`. Treat its quiescence as
  unknown: it is not this integrator's child and could not be reached.
- **Next.** Promote and claim `BPR-0778`, then rebuild the chooser as the sheet of rows.

### Decisions — d5

- **The policy is a named object, not a platform test.** `empty_desktop_outcome` and
  `empty_desktop_policy` live in `bpr/app/profile_desktop.hpp`; `native_empty_desktop_policy()` is the
  single place the platform is read. The desktop asks it at both places a desktop can become empty: the
  last runtime retiring, and the chooser being closed while none exists ([[BPR-0770]]'s drift D4).
  A course injects either outcome, and `last_empty_desktop_outcome()` reports what was decided.
- **A refused durable quit is retried, not forced immediately.** `request_quit()` refuses while the
  registry is still saving the close that emptied the desktop, so the poll retries it for two seconds
  before forcing the shutdown. Nothing is on screen by then and no live window set needs preserving, and
  a process with no window and no way to reach one is the defect [[BPR-0696]] fixed.
- **The most recently used profile is an in-process fact.** It is the last profile whose window opened or
  took focus. An empty desktop only exists after a window has been open, and a fresh launch opens its
  initial profile, so nothing new is persisted and no registry field is added.
- **Reopening listens to the application's state, not to every event.** `applicationStateChanged` carries
  activation; an application-wide event filter would put every event in the process through this object.
- **The window is where a refusal with a window goes.** `workbench_window` gained one non-actionable item
  at the foot of its Profiles menu and one sentence appended to its status line, from the same wording
  the chooser uses (`profile_management_failure_text`, now shared). This is what lets the desktop keep
  the chooser away, so the two changes are one slice.
- **The retired recovery controls' courses are repointed, not deleted.** The activation, owner-admission
  and window-restart courses used the appended Reload Profiles button as both a gesture and a
  registry-idle indicator. They now use the reload Manage Profiles already performs, the chooser's Create
  enablement for idleness, and — in the restart course — the window's own refusal item, which is a
  stronger observation than the retired notice: it is the behaviour B3 requires.

## Outcome

Remove the last-runtime presentation in `profile_desktop.cpp`; add an injected, testable empty-desktop policy that keeps the application resident on macOS and quits through the existing drain and Quit path on Windows and Linux; open the most recently used profile at Home from application activation, ⌘N and the Profiles menu through the existing intents; route failures with a live window to that window and present the chooser for a failure only when no window exists.

## Acceptance

- Closing the last window presents nothing; the resident or quit outcome follows the injected policy and the restore record distinguishes close from Quit.
- Activation, ⌘N and the Profiles menu reopen the most recent profile exactly once with one runtime owner.
- A failure with a live window reports there and presents nothing; with no window the chooser is presented; the existing management course still passes.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Verification — d5, source revision `48aa547fc4a283b0944ffc854f045c222ad732a6`

| Check | Direct surface and command | Result |
|---|---|---|
| The last-window course | `ctest --test-dir build/dev -R '^application_profile_management_public_path$'` through the shipped composition | Passes in 2.54 s: no presentation on the last close, both injected policy outcomes, the three reopening gestures, B3 in both directions, and the restore record telling this close from a Quit (`d5-ctest-3.log`) |
| The component course | `ctest --test-dir build/dev -R '^qt_profile_management_window$'` | Passes in 0.25 s: the window's refusal item and status sentence, and the application menu bar's rows, shortcut, intents and action identity (`d5-ctest-3.log`) |
| The repointed recovery courses | `application_profile_activation_public_path`, `application_profile_owner_admission_public_path`, `application_profile_window_restart_public_path` | 4.06 s, 1.86 s and 6.15 s, all pass; the restart course now observes the failed lifecycle save on the window itself (`d5-ctest-3.log`) |
| Regression selection | Thirteen courses: `architecture_boundary_contract`, `application_profile_management_host`, `qt_composed_github_scale_100`, `qt_workbench_window_scale_100`, `qt_profile_management_window`, `application_setup_destination_public_path`, `application_profile_startup_public_path`, `application_profile_management_public_path`, `application_profile_multi_window_public_path`, `application_profile_activation_public_path`, `application_profile_owner_admission_public_path`, `application_profile_window_restart_public_path`, `application_profile_window_layout_public_path` | All pass, 208.64 s (`d5-ctest-regression.log`, with the restart course rerun in `d5-ctest-restart.log` after its repointing compiled) |
| Window restoration across restarts | `ctest --test-dir build/dev -R '^application_profile_window_review_restoration_public_path$'`, the longest course over this lifecycle | Passes in 555.62 s (`d5-ctest-restoration.log`) |
| Negative control | Put the retired `Management->present()` back into `poll()`'s empty-desktop branch, rebuild, run | Fails at "closing the last window presents no chooser"; a byte-identical revert (sha256 `6b4442a0…`) passes again (`d5-nc-run-planted.log`, `d5-nc-run-reverted.log`) |
| Build and format | Whole `build/dev` build; `git clang-format --force` on the touched files; `git diff --check` | Build exit 0 with no error; clang-format reported no further change; diff check clean (`d5-build-5.log`) |
| Documentation | `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py` | `DOCUMENTATION_OK`, exit 0; 64 tests OK (`d5-doc-gate-1.log`, `d5-doc-tests.log`) |

Limitations. Windows and Linux **native** quit behaviour is not run on this machine: only the injected
quit outcome is proven, and the native behaviour stays platform-lane work, exactly as RFC-BPR-0033's
verification note states. That the parentless menu bar merges into the real macOS menu bar, and the Dock
activation behind it, are native facts an offscreen course cannot show; with the rendered states they
belong to `BPR-0779`. Another session was committing unrelated work into this checkout while these
courses ran; its contents were present in the tree and are now ancestors of this revision, and none of
them touches this card's files.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
