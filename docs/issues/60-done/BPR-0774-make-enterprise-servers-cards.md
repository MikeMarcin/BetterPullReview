---
schema: "implement-rfc.card.v1"
id: "BPR-0774"
title: "Make enterprise servers cards with inline add, a remember switch and a confirmed forget"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0772"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"accounts-page","acceptance":["A3"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 — Add, remember, forget and restart drive the existing saved-server contract; the record is exact and no credential is enrolled","outcome":"pass","revision":"51c443d209bfcff9a32ccb3b637de38c81749a1a","evidence":{"kind":"command","detail":"Qt, qt_workbench_window at 100, 125 and 200: test_add_enterprise_server_opens_a_card_in_its_edit_state and test_a_kept_enterprise_server_remembers_and_forgets_on_its_card.\n- Add ' GHE.Example.Org ' with ' reviewer ' emits exactly {add, ghe.example.org, reviewer}.\n- An https:// hostname is refused with a sentence and emits nothing.\n- Switch on emits add {host, ''}, or {host, <signed-in login>} on a signed-in card.\n- Forget server emits exactly one remove of the kept record.\n- The switch never shows a state the queue does not keep, and no gesture reaches any of seven other sinks.\nComposed through the shipped composition: application_setup_destination_public_path. The enterprise server added by setup is kept, so its card remembers exactly the record kept_servers() holds. Forget server, through the confirmation, removes exactly it through the real queue host. Remember keeps it again. VaultStores is unchanged.\nRestart and the store's exactness are the unchanged contract: application_review_queue_enrollment passes.\nmain.cpp's saved-server seed now goes through the Add enterprise server card, and the reuse stage requires the kept card to remember exactly {ghe.kept.example, reviewer}.\nLimitation: application_saved_server_contract (seed then reuse over one store) fails in this session exactly as at HEAD 53de5f35. It stops at stage 4 epoch_refresh with EPOCH_FIXTURE_SEED_FAILED, identical marker lines apart from timing (scratchpad/ctest-d2-0774-setup.log against scratchpad/ctest-d2-0771-head-control.log), so the repointed seed and stage are compiled but not reached here."}},{"criterion":"AC2 — Forget confirms before acting and names the record it removes; Cancel performs no effect","outcome":"pass","revision":"51c443d209bfcff9a32ccb3b637de38c81749a1a","evidence":{"kind":"command","detail":"test_a_kept_enterprise_server_remembers_and_forgets_on_its_card.\n- Forget server, and the switch turned off, open a window-modal confirmation parented to Settings. It reads 'Forget ghe.example.com?', names the host and the account reviewer, and says nothing on the server changes and no sign-in is removed. Nothing is dispatched before an answer.\n- Cancel emits nothing, and the switch stays checked.\n- 'Forget server' emits exactly one remove of that record.\nComposed: in application_setup_destination_public_path, the confirmation names ghe.example.invalid, Cancel leaves the record kept, and confirming removes exactly it.\nNegative control (scratchpad/negative-control-0774.log), run on top of 51c443d2: planting a Forget that removes the record without confirming makes qt_workbench_window_scale_100 fail at 'Forget server opens a window-modal confirmation naming the host and the account and what it does not touch, and dispatches nothing yet'. The file was restored from HEAD, and the course passes again."}},{"criterion":"AC3 — The combo editor and its selector are gone from the page and from the composed courses","outcome":"pass","revision":"51c443d209bfcff9a32ccb3b637de38c81749a1a","evidence":{"kind":"command","detail":"test_the_servers_combo_editor_is_gone checks the Accounts page's widget tree and its QAccessible tree before a projection, with two kept servers, and with the draft open. It finds none of savedServerSelector, savedServerHostname, savedServerAccount, addSavedServerButton, forgetSavedServerButton, enterpriseServersEditor or accountsServersHeading. It finds no QComboBox on the page, no accessible name 'Remembered servers', 'Server to remember' or 'Enterprise servers', and no addSavedServer or forgetSavedServer action.\nThe accessors saved_server_selector, saved_server_hostname, saved_server_account, add_saved_server_action, forget_saved_server_action and enterprise_servers_editor are deleted, and git grep finds no source or course that names them (outside the retired-name absence assertions).\nThe bounded-collections course is repointed: kept servers without host profiles add no widgets.\nmain.cpp's saved-server stage no longer looks up savedServerSelector, addSavedServer or the hostname and account fields.\nThe A4 inventory holds with a kept GHES card and the Add enterprise server button on the page.\nLimitation: offscreen render only; the rendered and attended course is BPR-0779."}}]
---

# BPR-0774: Make enterprise servers cards with inline add, a remember switch and a confirmed forget

## Current frontier — accounts-page accepted; next milestone home-page, bpr-shell-surfaces-20260915 d2 / rfc-integrator-d2 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `accounts-page`, with obligations `BPR-0771:*` through `BPR-0774:*` and both exits. This card closes `BPR-0774:*` and the milestone.
- **Candidate.** Local `main`, not pushed:
  - `BPR-0771`: source `284d1eac`, finish `41353ba8`.
  - `BPR-0772`: source `aee19b26`, finish `4e891655`.
  - `BPR-0773`: source `fea32882`, finish `2a06779c`.
  - `BPR-0774`: promote `b5a0f61e`, claim `64df25f2`, source `51c443d2`, and this record in the finish commit.
- **Usable behaviour.** Accounts & hosts is RFC-BPR-0032's page:
  - The sentence, one server card per configured server, and Add enterprise server.
  - Native HTTPS and GitHub CLI on one hostname share a card, with the CLI offer and its trust warning.
  - The device sign-in is the account row of the host it was asked for.
  - An enterprise card remembers and forgets exactly the kept record, behind a confirmation that names it.
  - A load refusal is said under the Home bar with Copy details, and a session failure is said in its card.
- **Milestone exits.**
  - `accounts-page:exit:1`, pass. The retired objects are absent from the page's widget and accessibility trees: `test_accounts_page_carries_none_of_the_retired_chrome` and `test_the_servers_combo_editor_is_gone`. Every composed course that named them passes against its new target: the final selection and the restoration course. The only exceptions are the six courses that fail identically at HEAD in this session.
  - `accounts-page:exit:2`, pass. Each configured server renders one card, both in Qt courses and through the shipped composition in `application_setup_destination_public_path`. Sign-in goes through the existing device and Connect intents (the `BPR-0773` courses and device self-tests). Enterprise add, remember and forget go through the existing saved-server intents (the `BPR-0774` courses, and the setup-destination course through the real queue host).
- **Independent review.** No blocking finding. Two majors and one minor were fixed in `51c443d2`; the adjudication is recorded under Evidence.
- **Carried forward.**
  - Pending native evidence outside this sandbox: `application_public_path`, `_ghes_cli_unavailable`, `application_device_authorization_ghes_refusal`, `_panel_course` and `application_saved_server_contract`. They fail identically at HEAD `53de5f35` here. `application_anonymous_public_preview_public_path`'s pass expression predates the `PUBLIC_PREVIEW_EPOCH_LINE_HISTORY` line.
  - Review follow-up for `BPR-0779`/`BPR-0780`: say a refusal when readiness or validity refuses a Remember, and say a collection-limit refusal of Add beside the draft.
  - `info_glyph` exists now; `BPR-0790` adopts it for the Appearance rows.
  - Mandate r2: nothing on this page or the Home notice row occupies the workbench window's bottom-left corner. Dif's first-login Home appearance and bottom-left resting place belong to home-page and appearance, reconciled with RFC-BPR-0031 (D1).
- **Next.** home-page: `BPR-0775` (the Home bar and first-launch card, the landing part of D5, Dif per mandate r2).

## Outcome

Replace the servers combo editor with enterprise server cards: Add enterprise server appends a card in its edit state (hostname, optional account, Add, Cancel) using the existing saved-server intents; Remember this server is a switch with its consequence; Forget server confirms and names its scope; a token session is labelled session-only.

## Acceptance

- Add, remember, forget and restart drive the existing saved-server contract; the record is exact and no credential is enrolled.
- Forget confirms before acting and names the record it removes; Cancel performs no effect.
- The combo editor and its selector are gone from the page and from the composed courses.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Accepted evidence — source `51c443d2`

- **AC1, the saved-server contract drives add, remember, forget and restart.**
  - Qt, `qt_workbench_window` at 100, 125 and 200:
    - `test_add_enterprise_server_opens_a_card_in_its_edit_state`: exact add of the trimmed, lowercased record; an invalid hostname refused beside the fields; Cancel does nothing.
    - `test_a_kept_enterprise_server_remembers_and_forgets_on_its_card`: the switch shows exactly the kept record, Remember adds with the signed-in login, Forget removes exactly the record, and no other sink moves.
  - Composed: `application_setup_destination_public_path`. Forget and Remember go through the shipped queue host, the record is exact, and `VaultStores` is unchanged.
  - Restart and store exactness: `application_review_queue_enrollment` passes, unchanged.
  - Limitation: `application_saved_server_contract` (seed then reuse) stops at stage 4 `epoch_refresh`, as at HEAD, so the repointed seed and stage are not reached in this session.
- **AC2, Forget confirms first and names the record.**
  - The confirmation is window-modal, owned by `settings()`, and names the host and account. Cancel does nothing; Forget server removes exactly that record. Covered by the Qt course and the composed setup-destination course.
  - Negative control (`scratchpad/negative-control-0774.log`): a Forget that removes without confirming makes `qt_workbench_window` fail. Restored from HEAD, it passes.
- **AC3, the combo editor is gone.**
  - `test_the_servers_combo_editor_is_gone` checks the widget and accessibility trees before a projection, with kept servers, and with the draft open.
  - The accessors are deleted, and no course or stage names them.
  - The bounded-collections course is repointed, and the A4 inventory holds with a kept GHES card.
- **Integration and review.**
  - Integration pass, 62 courses (`scratchpad/ctest-integration-0774.log`), plus the restoration course (551 s).
  - Final selection after the review corrections (`scratchpad/ctest-final-0774.log`): 56 of 62. The six failures match HEAD, apart from `BPR-0771`'s intended host-term change.
  - Independent review: no blocking finding. Majors 1 and 2 and Minor 3 fixed; Minor 4 and Minor 5 recorded; notes resolved.
- **Gates.** `git clang-format` leaves 0 files to change; `git diff --check` is clean; the documentation gate passes.

### Candidate evidence — accounts-page d2 (BPR-0774 working record)

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `accounts-page`; obligations `BPR-0771:*` to `BPR-0774:*` and both exits.
  - Done: `BPR-0771` (`284d1eac`/`41353ba8`), `BPR-0772` (`aee19b26`/`4e891655`), `BPR-0773` (`fea32882`/`2a06779c`).
  - This card: promoted `b5a0f61e`, claimed `64df25f2`, on local `main`, not pushed.
  - This is the milestone's last card; both exits follow it.
- **Starting point.**
  - Accounts & hosts still ends with an "Enterprise servers" heading and a combo editor: a selector of remembered servers, hostname and account fields, and Remember and Forget tool buttons dispatching `saved_server_intent`.
  - A remembered server is already a native GHES host profile, and so a server card: the composition registers it from the queue projection and calls `add_setup_profile`.
  - Forgetting removes only the saved record. The profile registered in the running session stays.
- **Design being implemented.**
  - **No list of its own.** The heading and combo editor are removed. An enterprise server is its host's card.
  - **Enterprise card rows** (GHES cards only, while the queue is ready and a saved-server dispatch exists):
    - "Remember this server": label, the shared `info_glyph`, and a switch. The glyph's popover and accessible description carry the consequence "Keeps the hostname and account name across launches. The token is still asked for." The switch shows exactly whether the queue keeps a record for this hostname, and never a pending wish. On asks to add the record, with the signed-in login when the card has one. Off asks to forget it.
    - "Forget server" (flat), with its glyph "Stops offering this server at the next start. Nothing on the server changes." Shown only while a record is kept.
  - **Confirmation.** Forget, and the switch turned off, open a window-modal confirmation. It names exactly the record (host, and account when one is kept) and says nothing on the server changes and no sign-in is removed. Cancel does nothing; Forget server dispatches the remove intent for exactly that record.
  - **Adding a server.** One bordered "Add enterprise server" button under the cards opens a card in its edit state: hostname, account (optional), Add, Cancel. Add dispatches the add intent with the hostname lowercased and trimmed and the account trimmed. An invalid hostname gets a refusal sentence beside the fields instead of a disabled Add. Cancel closes it with no effect.
  - **Session-only token.** Already the card's grant sentence.
  - **`info_glyph`.** New shared widget (`bpr/qt/info_glyph.hpp`). Built here for the Remember row; `BPR-0790` adopts it, which is an ordering note, not a change of that card's scope.
  - **Composition courses.** The `main.cpp` saved-server seed uses the Add enterprise server card. The reuse stage requires the kept card to remember exactly `ghe.kept.example`/`reviewer`.
- **Progress.**
  - The source is edited, uncommitted:
    - New `info_glyph.{hpp,cpp}`, added to the Qt adapter `CMakeLists.txt`.
    - `server_card.{hpp,cpp}`: the Remember and Forget rows and gestures; hairlines between the five rows.
    - `workbench_window.{hpp,cpp}`: the combo editor and heading removed; the draft card, Add enterprise server button, confirmation, `kept_servers()`, and Remembered in `update_server_cards`.
    - `src/app/main.cpp`: the seed goes through the draft; the stage checks the kept record exactly.
  - **Source build: green** (`scratchpad/build-d2-0774-src.log`). First courses: 11 of 11 pass (`scratchpad/ctest-d2-0774-src.log`).
  - The setup-destination course has been extended with the A3 Remember and Forget steps through the shipped composition (`scratchpad/ctest-d2-0774-setup.log`):
    - `application_setup_destination_public_path` passes.
    - `application_review_queue_enrollment` (the unchanged contract) passes.
    - `application_saved_server_contract` fails as at HEAD `53de5f35`: stage 4 `epoch_refresh`, `EPOCH_FIXTURE_SEED_FAILED`, identical marker lines apart from timing. The repointed seed and stage are therefore not reached in this session.
  - **The rfc-worker returned the BPR-0774 course edits** with no suspected source defect (checkpoint `scratchpad/worker-0774-qt-tests.md`):
    - The bounded-collections course is repointed.
    - New: `test_add_enterprise_server_opens_a_card_in_its_edit_state`, `test_a_kept_enterprise_server_remembers_and_forgets_on_its_card` (including the session-only token sentence) and `test_the_servers_combo_editor_is_gone`.
    - New A4 state: "the queue keeps the enterprise server".
  - **Qt: 16 of 16 pass** (`scratchpad/ctest-d2-0774-qt.log`): `qt_workbench_window`, `qt_composed_github`, `qt_device_authorization` and `qt_anonymous_public_preview` at 100, 125 and 200, and `qt_settings_window` at all four scales.
  - **Integration pass for the milestone** (BPR-0771 to BPR-0774 combined), running in the background via `scratchpad/integration_0774.sh`:
    - Steps: format the 10 changed paths, rebuild, run BPR-0771's 55-course regression selection plus the saved-server, queue-enrollment and device courses and the courses that fail at HEAD, then the restoration course.
    - Logs: `scratchpad/integration-0774.log`, `scratchpad/ctest-integration-0774.log`, `scratchpad/ctest-integration-0774-restoration.log`.
    - Result (pre-review tree): formatting changed 5 files and left 0 to change; `git diff --check` clean; build green. The selection passed 56 of 62.
    - The 6 failures are all recorded at HEAD `53de5f35` in this session. `application_public_path`, `_ghes_cli_unavailable`, `application_device_authorization_panel_course`, `_ghes_refusal` and `application_anonymous_public_preview_public_path` have marker lines identical to the HEAD control apart from timing.
    - `application_saved_server_contract` differs in one marker only: `EPOCH_REFRESH_PRESENTATION_TERM:host` now expects and observes `github.com`, where HEAD expected `Active host: github.com`. That is `BPR-0771`'s intended hostname-only change. It still stops at stage 4 `EPOCH_FIXTURE_SEED_FAILED`, as at HEAD.
  - **Independent review** (rfc-reviewer, static, `cd7859da..HEAD` plus this working tree): no blocking finding. Adjudication:
    - Major 1, accepted and fixed: the Forget confirmation was parented to the window's hidden `SettingsWindow` instead of `settings()` (the profile's mounted shell). It is now owned by `settings()`, and the setup-destination course asserts that parent through the shipped composition.
    - Major 2, accepted and fixed, a correction to `BPR-0773`: a denied or expired row could follow a newly chosen host onto a card that never had the code. The window now records `DeviceFlowHost` while a flow runs, and a finished flow's row is said only on that host's card.
    - Minor 3, accepted and fixed: Sign in could show while its way in was refused. New `SignInAdmitted`: the device begin for a host with a device flow, Connect otherwise.
    - Minor 4, recorded as a follow-up for `BPR-0779`/`BPR-0780`: silent no-ops when readiness or validity refuses a Remember, and the collection-limit refusal of an Add said on Reviews & queue rather than beside the draft. Within A3 as written: Cancel still does nothing, and nothing invalid is dispatched.
    - Minor 5, a limitation: the `main.cpp` GHES device course binds the panel directly (no page gesture reaches that state), and `begin_through_device_panel` still checks the hidden per-window Settings, which is the pre-existing panel-course failure.
    - Note 6: the Connected green is the RFC's sanctioned literal, not a palette. Its comment now cites RFC-BPR-0032.
    - Notes 7-9: no change. The device address exemption in A4 is documented; a stale captured record is harmless because the store erases by hostname; nothing blocks a bottom-left Dif.
  - The restoration course passed on the pre-review tree in 551 s (`scratchpad/ctest-integration-0774-restoration.log`). The corrections do not reach its reading and restoration path.
  - **The rfc-worker added the correction courses:**
    - `test_a_finished_sign_in_is_said_only_on_its_host`.
    - `test_sign_in_waits_for_its_way_in_to_be_admitted`.
    - The landing course's claim after expiry now says the enterprise card holds the panel without showing the finished row, and shows its own Sign in.
  - **Final selection on the corrected tree** (`scratchpad/final_0774.sh`, `scratchpad/final-0774.log`, `scratchpad/ctest-final-0774.log`):
    - Formatting changed 2 files and left 0 to change; `git diff --check` is clean; full rebuild green.
    - 56 of 62 pass, including every Qt course at every scale with the correction courses.
    - The same 6 courses fail, with the same markers as the integration pass: identical to HEAD apart from timing, and the explained saved-server host term.
    - Commit paths: `scratchpad/bpr-0774-commit-paths.txt` (11 paths).
  - **Source committed:** `51c443d2` "Make enterprise servers cards with a remember switch and confirmed forget" (11 files, +1460/−257).
    - The commit also carries the independent review's corrections: the Forget confirmation owned by `settings()`, the finished device row said only on its host (a correction to `BPR-0773`), and `SignInAdmitted`. Its message does not mention them, and this record is where they are named.
    - The negative control runs on top (`scratchpad/negative-control-0774.log`): it plants a Forget that removes the record without confirming, expects `qt_workbench_window` to fail, and restores the file from HEAD.
  - The same rfc-worker owns the `workbench_window_tests` and `composed_github_tests` edits for this card (checkpoint `scratchpad/worker-0774-qt-tests.md`).
- **Verification plan.**
  - Qt courses:
    - Add, then Cancel: no effect.
    - Add with an invalid hostname: refused.
    - Add with a valid hostname: one exact add.
    - Remember on: add, with the login.
    - Remember off and Forget: a confirmation naming the record; Cancel does nothing; Forget server gives exactly one remove.
    - The retired editor is absent from the page and the accessibility tree.
    - The bounded-collections course is repointed.
    - The A4 inventory holds with a remembered GHES card.
  - Composed: the setup-destination course, where Remember and Forget go through the shipped composition's queue host and no credential is enrolled.
  - Application contract: `review_queue_enrollment_tests` (restart and record exactness), unchanged.
  - `application_saved_server_contract` (seed and reuse) fails at HEAD in this session at `epoch_refresh`, so it remains a limitation.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
