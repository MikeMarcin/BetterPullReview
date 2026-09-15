---
schema: "implement-rfc.card.v1"
id: "BPR-0773"
title: "Put the device sign-in inside the account row as one gesture"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0772"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"accounts-page","acceptance":["A2"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 — One Sign in gesture shows the code and browser controls inside the card with no disclosure or scroll","outcome":"pass","revision":"fea32882cc666c69f7eb1600fd686ded915296d4","evidence":{"kind":"command","detail":"qt_composed_github at 100, 125 and 200 (scratchpad/ctest-d2-0773-final.log). test_one_sign_in_gesture_shows_the_code_in_the_card: the chosen github.com card's Sign in emits exactly one begin_device_authorization_intent for its native profile and no GitHub intent. After the awaiting presentation, the address sentence, address, code, Copy code, Open browser (default), validity, status sentence and Cancel are all visible in that card's account row and none is under Details. The header says Signing in. The panel lies inside the Accounts page's scroll viewport at the size Settings opens (no larger than 960×640). The code is fixed-pitch at 1.6×, and Copy puts exactly the code on the clipboard and says Copied. test_the_account_action_presses_the_chosen_cards_sign_in (D5) shows the Account action beginning the same flow once signed out on github.com, and doing nothing but opening the page when signed in or on a GHES host. test_device_sign_in_is_mounted_in_the_chosen_cards_account_row proves the mount and that a settled row moves with the chosen host. Limitation: offscreen render; the rendered and attended course is BPR-0779."}},{"criterion":"AC2 — Cancel, expiry, denial, unavailable and manual-token cases render in the row; no disabled button or truncated field is shown","outcome":"pass","revision":"fea32882cc666c69f7eb1600fd686ded915296d4","evidence":{"kind":"command","detail":"test_terminal_and_token_rows_stay_in_the_card. Cancel emits exactly one cancel intent. Cancelled, expired, denied and unavailable each show their sentence and exactly one Sign in again inside the card, with no Cancel, Open or Copy, header Signed out, no visible disabled button and no field narrower than its text. Only unavailable offers Sign in with a token; its click emits exactly one authenticated activate for the chosen native profile and no device gesture. Authorized plus a ready session returns the row to the signed-in account. The panel course test_the_row_hides_what_it_cannot_do_and_offers_the_token_way_in (qt_device_authorization at 3 scales) covers the idle, awaiting, denied, unavailable, disabled and expired sets, token-dispatch gating, and Copy code / Copied. The A4 signing-in state still names each fact once, with Open browser as the page's one filled control. Negative control (scratchpad/negative-control-0773.log), run on top of fea32882. Planting a Cancel that stays visible when the core disables it makes qt_composed_github_scale_100 fail in test_terminal_and_token_rows_stay_in_the_card at 'cancelled: the row shows no Cancel, Open browser or Copy code'. The file was restored from HEAD, and the course passes again. Limitation: the manual-token prompt itself is the composition's modal, and this Qt surface proves it is reached through the Connect action's intent, not the prompt's contents."}},{"criterion":"AC3 — The device public-path course passes unchanged in what it proves about authorization; only presentation moved","outcome":"pass","revision":"fea32882cc666c69f7eb1600fd686ded915296d4","evidence":{"kind":"command","detail":"The device self-tests pass with their pass expressions unchanged: application_device_authorization_{registered_course,refresh_course,orphan_course,browser_handoff,browser_absent,panel_absent,browser_address}, application_public_path_wait, application_device_binding_restart, application_logging, application_github_public_path_github_com, application_setup_destination_public_path and application_review_submission_composition_public_path (scratchpad/ctest-d2-0773-final.log). The existing qt_device_authorization legality, focus, announcement, handoff, copy, terminal, manual-token and render courses pass unchanged. The panel's dispatch, commands and projection are untouched, and main.cpp's begin_through_device_panel still finds the panel by its accessible name. application_device_authorization_panel_course and _ghes_refusal fail in this session exactly as at HEAD 53de5f35: identical marker lines apart from timing against scratchpad/ctest-d2-0771-head-control.log (the panel course's Settings-visibility check on the hidden per-window Settings, and the keychain vault). Limitation: those two courses cannot pass in this environment."}}]
---

# BPR-0773: Put the device sign-in inside the account row as one gesture

## Current frontier — BPR-0773 accepted; accounts-page continues, bpr-shell-surfaces-20260915 d2 / rfc-integrator-d2 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `accounts-page`, with obligations `BPR-0771:*` through `BPR-0774:*` and both exits. This card closes `BPR-0773:*`; the milestone continues with `BPR-0774`.
- **Candidate.** Local `main`: promote `700cd451`, claim `48564412`, source `fea32882` (8 files), and this record in the finish commit. Nothing is pushed.
- **Result.** Device sign-in is the account row of the card holding the profile it is bound to.
  - One Sign in shows, inside that card: "Enter this code at" with the address, the code in the fixed-pitch font at 1.6× with Copy code, Open browser filled, the validity, one status sentence and Cancel. The header says Signing in.
  - A disabled command is hidden. A terminal state offers one Sign in again. Where the device flow is unavailable or turned off, "Sign in with a token" triggers the window's Connect action.
  - The toolbar Account action presses the chosen card's Sign in when that card offers the device flow (D5).
- **Decisions to carry.**
  - A live code stays on the card of the host it was asked for, and the row follows a newly chosen host once the flow settles.
  - The manual-token way in is the composition's existing token prompt, reached through Connect; no credential UI was added.
  - The panel's stage texts are now sentences ("Waiting for the browser to authorize this Mac."). Only `main.cpp` uses a panel string (the begin button's accessible name), and it is unchanged.
- **Carried to BPR-0774.**
  - A card that is removed must first release the device sign-in (`release_device_sign_in`).
  - Remembered servers already become native GHES host profiles, and so cards, through the queue projection and `add_setup_profile`. Forgetting removes the saved record, but the profile registered in this session remains.
  - RFC-BPR-0034's shared `info_glyph` is assigned to `BPR-0790` (appearance milestone), while RFC-BPR-0032 wants it on the Remember row. `BPR-0774` must either build the minimal shared widget for `BPR-0790` to adopt, or record the ordering drift.
  - Design inputs: `scratchpad/bpr-0773-0774-design.md`.
- **Environment note.** `application_device_authorization_panel_course` and `_ghes_refusal` still fail in this session exactly as at HEAD `53de5f35`.

## Outcome

Present the existing `device_authorization_panel` state machine as the account row's signing-in state: the address sentence, the selectable code in the code font at 1.6×, Open browser filled, Copy code, the validity, one status sentence and Cancel; denied, expired, unavailable and cancelled states in the same row with one retry; the manual-token path in the same row where the device flow is unsupported.

## Acceptance

- One Sign in gesture shows the code and browser controls inside the card with no disclosure or scroll.
- Cancel, expiry, denial, unavailable and manual-token cases render in the row; no disabled button or truncated field is shown.
- The device public-path course passes unchanged in what it proves about authorization; only presentation moved.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Accepted evidence — source `fea32882`

- **AC1, one gesture, inside the card.** `qt_composed_github` passes at 100, 125 and 200 (`scratchpad/ctest-d2-0773-final.log`):
  - `test_one_sign_in_gesture_shows_the_code_in_the_card`: one Sign in gives one begin. After awaiting, eight row parts are visible in the card's account row, none under Details, and the header says Signing in. The panel lies inside the page's scroll viewport at the size Settings opens. The code is fixed pitch at 1.6×, and Copy code copies exactly the code.
  - `test_the_account_action_presses_the_chosen_cards_sign_in` (D5).
  - `test_device_sign_in_is_mounted_in_the_chosen_cards_account_row`.
  - Limitation: offscreen render only; the rendered and attended course is `BPR-0779`.
- **AC2, the terminal and token rows.** Passed at every scale:
  - `test_terminal_and_token_rows_stay_in_the_card`: cancelled, expired, denied and unavailable each show their sentence and one Sign in again. No disabled or truncated control shows. The token way in dispatches exactly one activate.
  - `test_the_row_hides_what_it_cannot_do_and_offers_the_token_way_in` (`qt_device_authorization`).
  - The signing-in A4 state.
  - Negative control (`scratchpad/negative-control-0773.log`): a Cancel left visible while disabled makes the cancelled-row claim fail; restored from HEAD, the course passes.
  - Limitation: the token prompt's contents are the composition's modal and are not shown here.
- **AC3, only presentation moved.**
  - The device self-tests pass with unchanged pass expressions: registered, refresh, orphan, browser handoff, absent and address, panel-absent, public-path wait, binding restart, logging, github.com public path, setup destination and review submission.
  - The existing `qt_device_authorization` courses pass unchanged.
  - `_panel_course` and `_ghes_refusal` fail as at HEAD, with identical markers apart from timing.
- **Gates.** `git clang-format` leaves 0 files to change on all 8 paths; `git diff --check` is clean; the documentation gate passes.

### Candidate evidence — accounts-page d2 (BPR-0773 working record)

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `accounts-page`; obligations `BPR-0771:*` to `BPR-0774:*` and both exits.
  - `BPR-0771` is done: source `284d1eac`, finish `41353ba8`.
  - `BPR-0772` is done: source `aee19b26`, finish `4e891655`.
  - This card: promoted `700cd451`, claimed `48564412`, on local `main`, not pushed.
- **Starting point.**
  - `device_authorization_panel` is still a standalone widget on the Accounts page, above the server cards. The window shows it while no session is accepted.
  - The composition applies device presentations directly to it (`github_application.cpp`, `observe_device_*`). The window never sees the stage.
  - The toolbar Account action begins sign-in only when that standalone panel is visible (D5).
- **Design being implemented.**
  - **Mount.** The panel is mounted in the account row of the card that holds the profile it is bound to. Rebinding (choosing a host) moves it. The standalone mount leaves the page column.
  - **When the row is the device sign-in.** From a begun flow until a new idle, the card's login, terms and ways in give way to the panel. Authorized hands back to the session. A renewal that requires re-enrollment also shows the panel. The header word is Signing in while the stage is requested, awaiting authorization or cancelling.
  - **Panel row form.**
    - Opening sentence: "Enter this code at", then the address, frameless and sized to its text so it is never truncated.
    - Code: in the fixed font at 1.6×, selectable, with a flat copy glyph named "Copy code" (tooltip, "Copied" shown in place).
    - Controls: Open browser filled; the validity; one status sentence from the typed stage; the handoff note only when a handoff was tried; Cancel flat at the right.
    - Commands keep their core legality (`setEnabled`), and a disabled one is hidden, so no disabled button shows.
    - Terminal states keep the sentence and offer one "Sign in again".
    - Where device sign-in is unavailable or disabled, "Sign in with a token" in the same row triggers the window's Connect action. That opens the composition's existing token prompt; no credential UI is added.
  - **Window sees the stage.** The panel gains a presentation observer, so `update_server_cards` can decide the row and the Signing in word.
  - **D5.** The Account action opens Accounts and, when the current card offers device Sign in, presses that Sign in.
- **Progress.**
  - The source is edited, uncommitted:
    - `device_authorization_panel.{hpp,cpp}` (row form, observer, token dispatch).
    - `server_card.{hpp,cpp}` (adopt and release the device sign-in; the flow row).
    - `workbench_window.{hpp,cpp}` (`place_device_sign_in`, stage in `update_server_cards`, D5, the panel no longer in the page column).
  - **Source build: green** (`scratchpad/build-d2-0773-src.log`). Qt courses (`scratchpad/ctest-d2-0773-src.log`):
    - Passed: `qt_device_authorization` at 100, 125 and 200 (its existing legality, focus, announcement, copy, terminal and render courses unchanged); `qt_settings_window` at all four scales; `qt_workbench_window_scale_100`; `qt_anonymous_public_preview_scale_100`.
    - Failed: `qt_composed_github_scale_100`, only at the old claim "the sign-in button opens Accounts and hosts with Sign in focused". The standalone Sign in it focused is now hidden inside the card until a flow begins; the worker repoints that claim.
  - The stale mount comments in the window constructor are corrected (comment-only).
  - **Device self-tests: 13 of 15 pass** on the comment-corrected build (`scratchpad/ctest-d2-0773-selftest.log`):
    - Passed: `application_device_authorization_{registered_course,refresh_course,orphan_course,browser_handoff,browser_absent,panel_absent,browser_address}`, `application_public_path_wait`, `application_github_public_path_github_com`, `application_device_binding_restart`, `application_logging`, `application_review_submission_composition_public_path` and `application_setup_destination_public_path`.
    - `_panel_course` and `_ghes_refusal` fail, as at HEAD `53de5f35` in this session. Their marker lines match `scratchpad/ctest-d2-0771-head-control.log` apart from timing (29 and 33 lines).
  - **The rfc-worker returned uncompiled BPR-0773 course edits**, with no confirmed source defect (checkpoint `scratchpad/worker-0773-qt-tests.md`):
    - Repointed: the mount course, now `test_device_sign_in_is_mounted_in_the_chosen_cards_account_row`, and the Home landing sign-in claims.
    - New: `test_one_sign_in_gesture_shows_the_code_in_the_card`, `test_terminal_and_token_rows_stay_in_the_card`, `test_the_account_action_presses_the_chosen_cards_sign_in` (D5), and an A4 "signing in on GitHub.com" state.
    - New panel course `test_the_row_hides_what_it_cannot_do_and_offers_the_token_way_in`.
  - **Source guards from the worker's risks:**
    - The code font now also sets a monospace style hint and fixed pitch, so a platform with no code font still gets a fixed-pitch face.
    - A live code stays on the card of the host it was asked for: `place_device_sign_in` does not move the row while a flow is signing in, and the presentation observer places it once the flow settles.
  - **Compile and run on the guarded build: 14 of 14 Qt courses pass** (`scratchpad/build-d2-0773-qt.log`, `scratchpad/ctest-d2-0773-qt.log`):
    - `qt_composed_github` at 100, 125 and 200, including the new A2, D5 and signing-in A4 courses.
    - `qt_device_authorization` at 100, 125 and 200, including the new row-form course.
    - `qt_anonymous_public_preview` at 100, 125 and 200; `qt_settings_window` at all four scales; `qt_workbench_window_scale_100`.
  - I adjudicated the worker's diff:
    - No-scroll maps the panel into the page's scroll viewport at the size Settings opens (at most 960×640).
    - Disabled buttons and truncated fields are named widget by widget.
    - The code font is checked for fixed pitch at 1.6×.
    - D5 covers native signed out (one begin), signed in (none) and a chosen GHES host (none).
  - **Final selection: 27 of 29 pass** on a full rebuild after formatting (`scratchpad/final-0773.log`, `scratchpad/ctest-d2-0773-final.log`):
    - Formatting changed 3 files and left 0 to change; `git diff --check` is clean.
    - Passed: all 14 Qt courses and 13 device self-tests.
    - Failed: `_panel_course` and `_ghes_refusal`. On this binary their markers again match the HEAD control apart from timing (29 and 33 lines).
  - **Source committed:** `fea32882` "Put the device sign-in inside the account row" (8 files, +1189/−243). The negative control runs on top (`scratchpad/negative-control-0773.log`): it plants a Cancel that stays visible when disabled, expects the composed course to fail, and restores the file from HEAD.
- **Verification plan.**
  - `qt_device_authorization` at all scales, with the legality courses unchanged.
  - Composed Qt courses:
    - One Sign in gesture shows the code, Open browser, Copy, validity, status and Cancel inside the card, with no disclosure and no scroll.
    - Cancel, expiry, denial, unavailable plus token rows show no visible disabled button and no truncated field.
    - The Account action begins the device sign-in.
    - The A4 inventory holds in the signing-in state.
    - The repointed mount, Home sign-in and Settings mount courses pass.
  - Self-tests: the device registered, refresh, orphan, handoff, browser and address courses, the panel-absent course, and `application_device_binding_restart`. `application_device_authorization_panel_course` and `_ghes_refusal` fail at HEAD in this session.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
