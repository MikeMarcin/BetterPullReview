---
schema: "implement-rfc.card.v1"
id: "BPR-0740"
title: "The layout public path no longer opens the exact public review"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"7c044de4","found_by":"attended-prep-2026-09-10 d16"}
acceptance: [{"criterion":"application_profile_window_layout_public_path passes in ctest --preset dev","outcome":"pass","revision":"working tree over f35bbeb8 (tests/end_to_end/profile_window_layout_public_path_tests.cpp only)","evidence":{"kind":"command","detail":"ctest --preset dev -R '^application_profile_window_layout_public_path$' passes, 23.63 s, build/logs/bpr-0740-run-23.log; all four phases (seed, restore, verify, broken) through profile_window_layout_contract.py. Siblings: application_workbench_composition_contract 21.54 s and application_profile_window_review_restoration_public_path 565.78 s pass, build/logs/bpr-0740-siblings-24.log. clang-format --dry-run --Werror clean on the test."}},{"criterion":"The course still drags the files handle and restores the independent window layouts it was written to pin (BPR-0708's criteria), not a weakened wait","outcome":"pass","revision":"working tree over f35bbeb8","evidence":{"kind":"observation","detail":"The pull_request_ready wait, the two files-handle drags (370/430), the independent drawer and line-number states, the immediate-Quit layout, B's restored width, the orphan cleanup and the fresh-window profile defaults are unchanged. Finding 2 settled by instrumentation as course composition (window B's landing sign-in enrolled a second grant whose identity change retired every window's session; A had read anonymously because PublicPreviewSignedOut is false before the first paste); the course now signs in the first window of each phase before its first paste. The broken phase's 'saved workbench layout is unavailable' line is not produced since BPR-0718's composed-defaults fallback; the phase pins that fallback (files width 320, store preserved by the contract) and the lost visibility is carded as BPR-0742. No product change; github_application.cpp and its digests unchanged."}}]
---

# The layout public path no longer opens the exact public review

`application_profile_window_layout_public_path` is red at HEAD. Its
`open_review` helper (`tests/end_to_end/profile_window_layout_public_path_tests.cpp:50-66`)
types `https://github.com/octo-org/review-repository/pull/N` into the Home
open bar, triggers the open action, clicks `homeSetupContinue` when it
appears, and waits for the session panel to reach `pull_request_ready`;
the wait times out with "exact public review opens". Reproduced on
2026-09-11 at 7c044de4 with the [BPR-0737](BPR-0737-the-epoch-line-and-the-comparison-menu-are-one-full-width-row-under-the-strip.md)
edits stashed, so it is not that slice's. The test last changed at
ad3abf50 ([BPR-0708](BPR-0708-window-layout-restoration.md));
the Home open path it drives changed under
[BPR-0732](BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
(the signed-out Home is one paste field that opens a public link in one
gesture, with no "Read without signing in" and no `homeSetupContinue`
step) and [BPR-0733](BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md).
No focused selector of either card names this test, which is how it went
red unnoticed; the composed courses it shares a fixture with pass.

## Scope

- Diagnose whether the course or the product is wrong: the course may be
  waiting for a Continue button that BPR-0732 removed, or the
  `RegisteredDeviceCourse` fixture may no longer reach `pull_request_ready`
  from the paste field. Fix the one that is wrong; the accepted BPR-0732
  behaviour is not reopened by this card.
- Owned paths: `tests/end_to_end/profile_window_layout_public_path_tests.cpp`,
  and only if the product is at fault, `src/app/src/github_application.cpp`
  (with both application digests repinned) or the Qt Home surface.

## Acceptance

- `application_profile_window_layout_public_path` passes in `ctest --preset dev`.
- The course still drags the files handle and restores the independent
  window layouts it was written to pin (BPR-0708's criteria), not a
  weakened wait.

## Diagnosis, 2026-09-11 (d17)

Two findings, the first settled and the second open.

1. The course is stale, as suspected, but not for the reason first
   guessed. `homeSetupContinue` still exists and is shown for the
   `add_server` and `sign_in` stages. Since
   [BPR-0732](BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
   a pasted link resolves to `public_preview` wherever preview is
   composed, and on macOS `shipped_public_preview_capability()` composes
   it for every github.com fixture course
   (`src/app/src/github_application.cpp:1965-1977`, `:4654-4674`), so the
   button is hidden by design and the read goes out anonymously; the
   `github_com` fixture answers that anonymous read of pull 17 once and
   never pull 18 (`src/app/src/github_public_path_fixture.cpp:932`,
   `:1368-1374`), which is the "GitHub returned data that BetterPullReview
   could not safely use" the window showed. `PublicPreviewEnabledCourse`
   is a no-op on macOS. The restoration course converted to the account
   path under BPR-0732 (sign in from the landing card, then paste; an
   account opens without a stop); this course was not, and neither was
   the multi-window course
   ([BPR-0741](BPR-0741-the-multi-window-public-path-course-still-waits-for-a-sign-in-stop.md)).
   On Linux and Windows the capability is not shipped and the sign-in
   stop is still reached, so the red is macOS-only. The course is now
   converted: Home is shown before every paste (a restarted window shows
   Home with the saved reading under Recently viewed since BPR-0733), the
   first signed-out window signs in from the landing card, the Continue
   click is gone, and the wait for `pull_request_ready` is unchanged. Both
   windows now open their pull requests and the files-handle drags land.

2. Open: after window B opens pull 18, window A's review is closed. A
   reports session status 2, "No pull request is open in the workbench.
   Review history is inactive.", then "An obsolete review-history result
   was ignored." That is `workbench_application_host::invalidate_now`
   (`src/application/src/workbench_application_host.cpp:927-945`), whose
   only ingress is the per-window
   `github_application::invalidate(pull_request_authority_invalidation)`
   (`src/app/src/github_application.cpp:8661-8708`) on an authority match,
   or a session retirement through
   `github_window_read_host::retire(const github_profile_session&)`
   (`src/application/src/github_window_read_host.cpp:964-974`), whose
   caller in the app is not yet named. Tried without effect: the
   `profile_window_restoration_v1` fixture in place of `github_com`;
   signing in only while `PublicPreviewSignedOut`; `PersistDeviceBindings`
   with a `SharedDeviceVault`; waiting for `device_binding_durable` before
   the second window (it never settles within 30 s in this course, with
   or without persisted bindings, where the restoration course reaches it
   after `accepted` and `select_reading`). The restoration course never
   re-reads window A after B opens, so it cannot see this; the multi-window
   course pins it at `:236` ("second read preserves first review") but is
   red earlier (BPR-0741), so the line has not run since BPR-0732. Whether
   this is the course's account composition (a second enrollment
   superseding the first window's session) or a product regression in the
   window-session lifecycle is undetermined. The deciding step is to
   instrument the caller of `retire`/`release` in
   `github_window_read_host` and read `WorkbenchInvalidations`
   (`github_application.cpp:8679`) at the moment B opens: an
   authority-matched invalidation names the app path, a session retirement
   names the profile session lifecycle. Logs: `build/logs/bpr-0740-*.log`.

The multi-window course's own conversion is BPR-0741; if finding 2 is a
product regression it is that course's `:236` that pins the fix, and this
card's owned paths widen to `src/app/src/github_application.cpp` with both
digests repinned, as the scope allows.

## Settled, 2026-09-11 (d18)

Finding 2 is course composition, not a product regression. Instrumented
(`retire_windows` and its callers in `github_application_host.cpp`,
`github_window_read_host::retire`/`release`, the app's `invalidate`
override with `WorkbenchInvalidations`, the device-grant deliveries and
the course's own steps; logs `build/logs/bpr-0740-run-14.log` to `-18`):

- At window A's first paste `PublicPreviewSignedOut` is still false,
  because the shipped signed-out session is activated *by* that paste
  (`activate_public_preview`, the first `invalidate_github_identity_request`),
  so the d17 condition never selected the landing sign-in for A; A read
  pull 17 anonymously (the one anonymous answer the fixture gives).
- Window B then saw `PublicPreviewSignedOut` true and clicked the landing
  sign-in: device grant delivered with no setup staged, `enroll_profile`,
  `activate_host_profile_intent`, a second `invalidate_github_identity_request`,
  `retire_windows` over both windows, and window 1's `release` sent its
  active epoch authority through `Epochs->invalidate` to the app's
  `invalidate` (`WorkbenchInvalidations` 0 before it) and so to
  `invalidate_now`. An identity change retiring every window's session is
  the pre-BPR-0732 design; B's *open* of pull 18 did not touch A.
- No `github_window_read_host::retire` caller in the app exists; the only
  caller is the profile host's `retire_windows`.

Course, as landed: `open_review` takes an explicit `aSignIn`; the first
window of each phase signs in from the landing card before its first
paste (asserted to be signed out there) and every later open pastes with
the account. With that the seed, restore and verify phases pass unchanged
in their BPR-0708 criteria (independent widths, drawer and line-number
states, immediate-Quit layout, orphan cleanup, fresh-window defaults).

The broken phase (`PRAGMA user_version=999` on the layout store) then
failed at "optional layout failure is visible": traced through the
workbench host, no `settings_unavailable` is published because since
[BPR-0718](BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md)
a store that cannot open at composition yields `session_workbench_settings`
for every window (`WorkbenchPersistenceAvailable` false, no
`profile_window_layout_service`), whose load succeeds with the composed
defaults; the per-window load failure BPR-0708 pinned no longer occurs.
The phase now pins the accepted BPR-0718 behaviour: Home usable, the
window opens its review with the profile defaults (files width 320, not
the saved width) and the contract still checks the store's version and
the allocator are preserved. That the unreadable store is now silent to
the user is [BPR-0742](../20-ready/BPR-0742-an-unreadable-workbench-layout-store-is-silent-since-bpr-0718.md).

No product change; `github_application.cpp` and both digests are
unchanged. `application_profile_window_layout_public_path` passes
(23.6 s, `build/logs/bpr-0740-run-23.log`);
`application_workbench_composition_contract` (21.5 s) and
`application_profile_window_review_restoration_public_path` (565.8 s,
slow but green) pass (`build/logs/bpr-0740-siblings-24.log`). The
multi-window course stays red for its own reason,
[BPR-0741](BPR-0741-the-multi-window-public-path-course-still-waits-for-a-sign-in-stop.md),
whose `:236` "second read preserves first review" holds on the account
path once the course signs in first.
