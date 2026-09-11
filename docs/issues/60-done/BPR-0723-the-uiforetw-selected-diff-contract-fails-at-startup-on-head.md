---
schema: "implement-rfc.card.v1"
id: "BPR-0723"
title: "The UIforETW selected-diff contract fails at startup on HEAD"
status: "Done"
rfcs: ["RFC-BPR-0004", "RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"first_seen":"8ff3b664","found_during":"BPR-0719"}
acceptance: [{"criterion":"application_uiforetw_selected_diff_contract passes in build/dev","outcome":"pass","revision":"working tree over d3abe68f","evidence":{"kind":"command","detail":"build/dev ctest -R '^application_uiforetw_selected_diff_contract$' passes, all four appearances, 67.86 s (scratch verify-5.log). At HEAD it printed STARTUP_FAILED 0.5 s into the light capture (repro-1.log). Three things were needed: the pre-created --epoch-fixture-root narrowed to owner-private (the startup refusal), the SYNTAX_DISPLAYED / SYNTAX_DISPLAYED_SPANS lines printed from the status captured at the shutdown transition (BPR-0390's shown flag read empty after the BPR-0697/BPR-0705 release), and the BPR-0646/BPR-0651 rail-geometry block amended for BPR-0721's no-rails default (hidden drawers hold stale rail rectangles; the block now pins hidden, at most rail-wide, details row inside the window). The compact-row assertions at 1280 (row spans the window, exact order, edges, spacer) pass unchanged. application_appearance_theme_customization_contract passes twice on this binary (23.69 s verify-1.log, 23.80 s verify-3.log); the sibling scripts with the same omission now pass: overview-since-review 90.37 s, review-behaviour 89.06 s, saved-server 88.80 s (verify-3.log), application_epoch_maintenance_public_path 47.15 s (verify-4.log). application_native_notification_attended_contract gets past startup and refuses at NATIVE_NOTIFICATION_REFUSED:platform_tray_not_composed (a cocoa-platform precondition, not this card's). application_review_round_trip_composition_contract passes with its two github_application.cpp digests repinned (11.19 s, verify-5.log)."}},{"criterion":"The cause is recorded on the card with the commit that introduced it, or the environmental condition is named and reported by name","outcome":"pass","revision":"working tree over d3abe68f","evidence":{"kind":"observation","detail":"Card section 'Cause': registry_unavailable from resolve_profile_storage_root on a 0755 pre-created root since BPR-0695 (47de7ae8) made the fixture root the profile registry root, traced under lldb to the completed callback (error 5, registry) and load()'s resolve branch; the post-shutdown empty comparison traced under lldb to diff_workbench::invalidate from diff_application_host::invalidate_now enqueued by begin_shutdown -> WindowRead->shutdown() -> github_window_read_host::release -> invalidate_epoch_authority, introduced by BPR-0697 (9910cd47) and BPR-0705 (826fb871). Neither is environmental; the application's refusal is correct and the scripts and the evidence read were repaired."}}]
---

# The UIforETW selected-diff contract fails at startup on HEAD

`application_uiforetw_selected_diff_contract` is red at `8ff3b664`, before
any change of the v003 follow-ups. It was found while running the
[BPR-0719](BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md)
focused batch and reproduced on a clean worktree build of that HEAD
(`cmake --preset dev`, then `ctest -R '^application_uiforetw_selected_diff_contract$'`):

```
UIforETW light capture failed (1)
stdout:
STARTUP_FAILED
```

The failure comes 0.5 seconds into the first (light) capture, before the
probe runs: the application prints `STARTUP_FAILED` with nothing on stderr.
The configuration check at `src/app/main.cpp` (around line 7868) is not
the site, because it prints `UIFORETW_SELECTED_DIFF_CONFIGURATION_FAILED`;
what remains is a `github_application` start failure other than
`owner_busy` (the branch around line 7995) or the `catch (...)` around
line 8116. A developer build reports a thrown `std::exception` on stderr
as `STARTUP_FAILED_CAUSE:`, and none was printed, so the failure is either
a returned start error or a non-`std::exception` throw. The same binary
starts for `application_diff_workbench_public_path`,
`application_composition_contract` and
`application_review_round_trip_composition_contract`, which passed in the
same batch; the difference is the `--epoch-fixture-root` /
`--epoch-fixture-phase single` / `--uiforetw-selected-diff-evidence` /
`--diff-evidence-appearance` arguments.

Running the contract's exact command by hand (the app from `build/dev`,
`QT_QPA_PLATFORM=offscreen`, a fresh fixture root) reproduces the
one-word failure.

## Scope

- Name the failing startup step and repair it, or, if the failure is
  environmental to this station, name the environmental condition and make
  the application say so instead of `STARTUP_FAILED`.
- The contract is part of the RFC-BPR-0004 diff evidence and the
  RFC-BPR-0028 strip contract (compact row at 1280 and the 18-pixel rails),
  so it must be green before a closeout leans on either.

## Acceptance

- `application_uiforetw_selected_diff_contract` passes in `build/dev`.
- The cause is recorded here with the commit that introduced it, or the
  environmental condition is named and the application reports it by
  name.

## Also red at cc31119b: the theme customization contract

`application_appearance_theme_customization_contract` fails the same way
(`STARTUP_FAILED` 0.49 s into its first phase, nothing on stderr) on a
clean worktree build of cc31119b, before the BPR-0720/BPR-0724 changes,
and again after them. Its first phase passes `--github-fixture github-com
--epoch-fixture-root ... --epoch-fixture-phase single --diff-workbench-probe
--diff-interaction-evidence ... --appearance-theme-customization-probe ...
--render-evidence ...`, so the shared argument with the UIforETW contract
is `--epoch-fixture-root` / `--epoch-fixture-phase single`. The theme
contract is the direct course for the BPR-0720 palette on the application
public path; until this card lands, BPR-0720's application-path evidence is
the profile, panel, host, store and gesture selectors plus the attended
bundle.

## Cause

Two defects, neither in the application's startup.

**The one-word failure.** Under lldb the `completed` callback receives
`github_application_startup_failure::registry`, mapped from
`profile_startup_failure::registry_unavailable`, which `load()` returns
when `adapters::sqlite::resolve_profile_storage_root` refuses the
registry root. Since [BPR-0695](BPR-0695-profile-startup-composition-admission.md)
(`47de7ae8`, 2026-09-08) the `--epoch-fixture-root` is also the profile
registry root, and the registry refuses a root it did not narrow itself.
The contract scripts pre-create that root with `file(MAKE_DIRECTORY)`,
which is mode 0755 under the station's umask; the scripts that pass
(round-trip, ledger, workbench, epoch persistence, gesture, store fallback)
all call `bpr_narrow_owner_private` on the roots they pre-create, the
BPR-0436 helper. Running the exact capture command by hand with a 0700
root passes every marker. The same omission is in
`application_appearance_theme_customization_contract`,
`application_overview_since_review_contract`,
`application_saved_server_contract`, `application_review_behaviour_contract`,
`epoch_maintenance_public_path_contract` and
`application_native_notification_attended_contract` (all red at HEAD with
the same one word; `application_defect_composition_contract` already
narrows its roots). Fixed by narrowing in each script, not by relaxing the
registry.

**Behind it, the displayed-comparison check.** Once the capture starts,
every profile fails BPR-0390's `SYNTAX_DISPLAYED_SPANS:…:1` because the
status is read after shutdown and reports no comparison. Traced with lldb:
the four publications land on the one `diff_workbench`, no `clear()`
follows the last, and the writer is `diff_workbench::invalidate` from
`diff_application_host::invalidate_now`, enqueued by
`begin_shutdown` → `WindowRead->shutdown()` → `github_window_read_host::release`
→ `invalidate_epoch_authority` → `invalidate_diff_if`. That release is
[BPR-0697](BPR-0697-shared-profile-auth-session.md) (`9910cd47`) and
the shutdown call is [BPR-0705](BPR-0705-live-profile-window-composition.md)
(`826fb871`), both 2026-09-08: a closing window now gives up its epoch
authority and the comparison with it, on every course (the workbench and
theme contracts print `…:0` too; only this contract asserts it). The
release is correct lifecycle, so the evidence moves rather than the
behaviour: `transition_to_shutdown` captures `status()` before
`begin_shutdown`, and the `SYNTAX_DISPLAYED` / `SYNTAX_DISPLAYED_SPANS`
lines print from that capture. The history counters stay live.
