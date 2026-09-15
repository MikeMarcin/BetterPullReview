---
schema: "implement-rfc.card.v1"
id: "BPR-0772"
title: "Compose one server card per configured host with header, account row and rows"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: ["BPR-0771"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"accounts-page","acceptance":["A1","A4"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 — One card per configured host renders with the accepted header, account row and rows from unchanged projections","outcome":"pass","revision":"aee19b268c1e2420ecabc545c4d7f4def5ba1eea","evidence":{"kind":"command","detail":"Qt: qt_composed_github_scale_100/125/200 pass (scratchpad/ctest-d2-0772-qt.log). test_one_card_per_server_holds_its_ways_in, on a fixture shaped like the shipped composition (github.com native HTTPS and GitHub CLI, GHES native HTTPS and GitHub CLI):\n- two cards, in the order their hostnames were first configured, and each card holds its server's profiles with native HTTPS first, also when the configuration is reversed;\n- the kind lines 'GitHub.com, native HTTPS' and 'GitHub Enterprise Server, native HTTPS';\n- GitHub CLI offers in the composition's words;\n- exactly one trust warning per server, inside its card, saying 'this server';\n- the chosen card's Sign in filled, the other card's Connect;\n- add_setup_profile merges a profile on an existing hostname into its card and appends a card for a new hostname.\nexpect_one_card_per_server runs in every state of the retired-chrome and A4 courses. The card's header, account row and rows (Access, Rate limit, Details with summary; 44 px rows, hairlines between rows, 150 px label column) follow RFC-BPR-0032 and the accepted mockup, and read only the existing session, capability, rate-limit and public-preview projections.\nComposed through the shipped composition: application_setup_destination_public_path asserts that GitHub.com's native HTTPS and GitHub CLI profiles share one card, that an added enterprise server appends a second card, and that a further server is a third.\nLimitation: GHES keeps ', native HTTPS' where the mockup's example omits the transport, because A4 needs the transport once per card and a GHES host can offer GitHub CLI. Offscreen render only; the attended and rendered course belongs to BPR-0779."}},{"criterion":"AC2 — Host, login, transport and connection state each appear once on the page, checked by a text inventory in each state","outcome":"pass","revision":"aee19b268c1e2420ecabc545c4d7f4def5ba1eea","evidence":{"kind":"command","detail":"test_accounts_page_says_each_fact_once covers seven states: no profile, connecting to GHES, signed in on GitHub.com, GitHub.com failed, reading public signed out, GitHub CLI chosen signed out, and GitHub CLI failed.\nEach state is first checked for having reached its card, then inventoried with Details closed and again with Details open. The inventory takes every label, button, line-edit and combo-box text visible to the page, including the device panel and the enterprise editor outside the cards.\nIt requires:\n- per card: the host once (case-sensitive, ignoring host/ addresses); the login once when signed in; the presented transport once and only in the kind line; GitHub CLI elsewhere only in the offer; its own state word once, in the header, and no other state word;\n- page-wide: each host once, and the login once or not at all;\n- nothing outside the cards naming any of these facts;\n- at most one filled control, and that it is the chosen card's Sign in exactly when native HTTPS is presented and signed out.\nNegative control (scratchpad/negative-control-0772.log), run on top of aee19b26: planting 'native HTTPS' in the Details summary makes qt_composed_github_scale_100 fail in this course, at 'the card for github.com names the transport it speaks for once while no profile, Details closed', naming serverCardKind and serverCardDetailsSummary. The file was then restored from HEAD, and the course passes again.\nLimitation: painted text (usage ring) and accessible names or tooltips are not inventoried; the signing-in state belongs to BPR-0773."}},{"criterion":"AC3 — Connect on a non-active card selects and connects through the existing intents; no widget gains an effect","outcome":"pass","revision":"aee19b268c1e2420ecabc545c4d7f4def5ba1eea","evidence":{"kind":"command","detail":"test_one_card_per_server_holds_its_ways_in records each GitHub intent with the selected profile and the device-bound profile at the instant it leaves. GHES Connect, the GHES GitHub CLI offer and the GitHub.com offer each emit exactly one authenticated activate_host_profile_intent, for 23, 24 and 22 respectively, after that profile was chosen and bound. The workbench, change, drawer-width, device, public-preview, setup-destination and saved-server sinks all stay at zero, and adding setup profiles dispatches nothing.\ntest_accounts_offer_no_public_preview_activation (qt_anonymous_public_preview at 3 scales) walks GHES Connect, GitHub.com Connect, the GitHub.com CLI offer and GHES again, each emitting exactly one activation and no device begin. A command-only refusal hides every Connect and offer, and clicks then emit nothing. The window's gestures are unchanged: select_github_profile, then the existing Connect action.\nSelf-tests: main.cpp CLI stages now iterate each server card's GitHub CLI offer, and the GHES device course binds the server's native profile. application_public_path, _ghes_cli_unavailable and application_device_authorization_ghes_refusal still fail as at HEAD 53de5f35 in this session: identical markers apart from timing, and they expire at stage 1 on the keychain vault, so those repointed stages are compiled but not exercised here.\nPassed: the GitHub public paths (github.com, UIforETW, GHES), device binding restart and review submission."}}]
---

# BPR-0772: Compose one server card per configured host with header, account row and rows

## Current frontier — BPR-0772 accepted; accounts-page continues, bpr-shell-surfaces-20260915 d2 / rfc-integrator-d2 / mandate r2

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `accounts-page`, with obligations `BPR-0771:*` through `BPR-0774:*` and both exits. This card closes `BPR-0772:*`; the milestone continues with `BPR-0773`.
- **Candidate.** Local `main`: promote `9d02b36e`, claim `f079e831`, source `aee19b26` (9 files), and this record in the finish commit. Nothing is pushed.
- **Result.** Accounts & hosts shows one `server_card` per configured server (hostname). Every host profile on that hostname is a way in to the card, native HTTPS first.
  - The card speaks for the chosen profile when it is this server's, and otherwise for its native profile.
  - The header's kind line is the one statement of the transport.
  - Sign in is the page's one filled control while native HTTPS is presented and signed out.
  - Connect shows on a card that is not chosen, and as the retry for a chosen GitHub CLI profile.
  - RFC-BPR-0015's GitHub CLI offer is a bordered button in the composition's words, and its trust warning says "this server".
  - The rows are Access, Rate limit and Details with its summary.
- **Decisions to carry.**
  - GHES keeps ", native HTTPS" in the kind line, where the mockup's example omits it. A4 needs the transport once per card, and a GHES host can offer GitHub CLI.
  - The GitHub CLI offer stays visible while signed in natively, so switching needs no sign-out.
  - The A4 inventory helpers (`shown_texts`, `expect_each_fact_once` in `tests/adapters_qt/composed_github_tests.cpp`) are available for the signing-in state (`BPR-0773`) and the enterprise cards (`BPR-0774`).
  - The device panel and enterprise editor currently name none of the A4 facts.
- **Limitation carried.** The `main.cpp` CLI stages and the GHES device course are repointed, but in this session they expire at stage 1 on the keychain vault, as at HEAD, so those stages are compiled and not exercised.
- **Next.** `BPR-0773`: the device sign-in inside the account row, including D5 (the toolbar Account action). Design inputs are in `scratchpad/bpr-0773-0774-design.md`.

## Outcome

Add a `server_card` Qt widget that composes the existing account, connection, capability, rate-limit and diagnostic projections into header, account row, Access, Rate limit and Details, one card per configured host, with Connect on a non-active card dispatching the existing selection then connect intents. The Accounts page becomes the title, the sentence and the card column. A row whose second text is a consequence rather than a value (Remember this server) uses the shared info glyph of RFC-BPR-0034.

## Acceptance

- One card per configured host renders with the accepted header, account row and rows from unchanged projections.
- Host, login, transport and connection state each appear once on the page, checked by a text inventory in each state.
- Connect on a non-active card selects and connects through the existing intents; no widget gains an effect.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

### Accepted evidence — source `aee19b26`

- **AC1, one card per server.** These courses passed at 100, 125 and 200 (`scratchpad/ctest-d2-0772-qt.log`; smoke rerun on the formatted tests `scratchpad/ctest-d2-0772-smoke.log`):
  - `test_one_card_per_server_holds_its_ways_in` (`qt_composed_github`). On a fixture shaped like the shipped composition it gives two cards, native HTTPS first even when configured in reverse, with kind lines, offers in the composition's words, one warning per server, and the chosen card's Sign in filled. Setup profiles merge into their server's card or append a new card.
  - `expect_one_card_per_server`, run in every state of the retired-chrome and A4 courses.
  - Through the shipped composition, `application_setup_destination_public_path` gives one github.com card for native and CLI, a second card for the added enterprise server, and a third for the next server.
  - Limitation: offscreen render only; the rendered and attended course is `BPR-0779`.
- **AC2, each fact once.** `test_accounts_page_says_each_fact_once` inventories the visible page text in seven states, each with Details closed and open. Per card, the host, login, presented transport (in the kind line only) and state word (in the header only) appear once. Page-wide, each host and the login appear once, nothing outside the cards names a fact, and at most one control is filled.
  - Negative control (`scratchpad/negative-control-0772.log`): with "native HTTPS" planted in the Details summary the course fails and names both widgets. Restored from HEAD, it passes.
  - Limitation: painted text, accessible names and tooltips are not inventoried; the signing-in state belongs to `BPR-0773`.
- **AC3, Connect through the existing intents, no new effect.**
  - `test_one_card_per_server_holds_its_ways_in` records the selected and bound profile at dispatch time. GHES Connect, the GHES offer and the GitHub.com offer each emit exactly one authenticated activation (23, 24, 22). Seven other sinks stay at zero.
  - `test_accounts_offer_no_public_preview_activation` walks two cards and the refusal hide.
  - Self-tests passed: the GitHub public paths, device binding restart, review submission and setup destination.
  - Limitation: the repointed CLI and GHES device stages are not exercised in this session, which matches HEAD (`scratchpad/ctest-d2-0772-focused.log`).
- **Gates.** `DOCUMENTATION_OK`; `git clang-format` leaves 0 files to change on all 9 paths; `git diff --check` clean; `issue_board.py validate` valid.

### Candidate evidence — accounts-page d2 (BPR-0772 working record)

- **Binding.** Plan `bpr-shell-surfaces-v1`, milestone `accounts-page`; obligations `BPR-0771:*` through `BPR-0774:*` and both exits. `BPR-0771` is done: source `284d1eac`, finish `41353ba8`. This card was promoted in `9d02b36e` and claimed in `f079e831`, on local `main`, not pushed.
- **Starting point.** `server_card` exists from `BPR-0771`, one per configured host profile. The shipped composition configures native HTTPS and GitHub CLI on the same hostname (github.com, and the GHES host). So github.com gets two cards, and A4 fails on the host alone.
- **Design being implemented.**
  - **One card per server.** Profile options are grouped by hostname, native first. The card speaks for the chosen profile when that profile is on this host, and otherwise for the host's native profile.
  - **Header.** The kind line (product and that profile's transport) is the page's only statement of the transport. GHES keeps ", native HTTPS": RFC-BPR-0032 A4 needs the transport once per card, and a GHES host can also offer GitHub CLI.
  - **Account row.** Sign in (the native way in) shows on the current card. It is filled only while the native profile is the presented one, so it is the page's one filled control.
  - **Connect.** Connect shows on a non-current card, or as the retry for a chosen CLI profile.
  - **CLI offer.** RFC-BPR-0015's `GitHub CLI (uses your gh sign-in)` offer is a bordered button that carries the composition's label text. The trust warning names "this server" rather than the host.
  - **Rows.** Rows are 44 px with hairlines between them and a 150 px label column. The Details row carries a quiet summary.
  - **Composition courses.** The `main.cpp` CLI stages iterate the host cards' CLI offers.
- **Progress.**
  - The source is edited, uncommitted, in `server_card.{hpp,cpp}`, `github_presentation_text.hpp`, `workbench_window.{hpp,cpp}` and `src/app/main.cpp`. The composed setup-destination course is in `tests/end_to_end/setup_destination_public_path_tests.cpp`.
  - The app and composed course targets build (`scratchpad/build-d2-0772-app.log`, exit 0).
  - **The rfc-worker returned uncompiled edits** to `tests/adapters_qt/{composed_github_tests,anonymous_public_preview_tests}.cpp`, reporting no suspected source defect (checkpoint `scratchpad/worker-0772-qt-tests.md`):
    - `test_cli_warning_lives_on_its_servers_card`.
    - `expect_one_card_per_server`.
    - The two-card walk in `test_accounts_offer_no_public_preview_activation`.
    - New `test_one_card_per_server_holds_its_ways_in`: membership, native first, offer texts, one warning per server, and exactly one choose-then-connect activation per gesture with every other sink at zero.
    - New `test_accounts_page_says_each_fact_once`: an A4 inventory over 7 states, each with Details closed and open, per card and page-wide, with at most one filled control.
  - **Compile and run: 22 of 22 pass on the first compile**, ctest exit 0 (`scratchpad/build-d2-0772-qt.log`, `scratchpad/ctest-d2-0772-qt.log`). This ran on the formatted source binaries plus the worker's unformatted test files. Passed:
    - `qt_composed_github`, `qt_anonymous_public_preview` and `qt_workbench_window`, each at 100, 125 and 200.
    - `qt_device_authorization` at 100, 125 and 200.
    - `qt_settings_window` at 100, 125, 150 and 200.
    - The GitHub public paths (github.com, UIforETW, GHES), device binding restart, review submission, and setup destination.
  - I adjudicated the worker's diff: the courses test the acceptance directly, and the seven A4 states match the source rules. Its two flagged uncertainties are consistent with the source: a filled Sign in while failed or reading on native, and the stricter transport placement.
  - Gates:
    - Test files formatted (1 changed, 0 remaining).
    - `git diff --check` clean.
    - `DOCUMENTATION_OK` (`scratchpad/doc-gate-d2-0772.log`).
    - `issue_board.py validate` valid.
  - The smoke rerun on the formatted tests passed 6 of 6: `qt_composed_github` and `qt_anonymous_public_preview` at 100, 125 and 200 (`scratchpad/ctest-d2-0772-smoke.log`).
  - **Source committed:** `aee19b26` "Compose one server card per configured server" (9 files, +1279/−213). It is the tree the 22-course run and the smoke rerun tested, apart from test-file formatting. The A4 negative control runs on top of it (`scratchpad/negative-control-0772.log`) and restores the file from HEAD.
  - Next: commit the source slice with explicit paths (`scratchpad/bpr-0772-commit-paths.txt`, 9 paths). Then run an A4 negative control on top: plant the transport in the Details summary, expect only the inventory course to fail, and revert to HEAD. Then finish with `scratchpad/bpr-0772-acceptance.draft.json`.
  - **Focused self-test run** (`scratchpad/ctest-d2-0772-focused.log`, on the unformatted working-tree binaries):
    - Passed: `application_github_public_path_{github_com,uiforetw_74,ghes_3_18}`, `application_device_binding_restart`, `application_review_submission_composition_public_path` and `application_setup_destination_public_path`. The last now asserts, through the shipped composition, that GitHub.com native HTTPS and GitHub CLI share one card, that an added enterprise server appends a second card, and that a further explicit server is a third.
    - Failed: `application_public_path`, `_ghes_cli_unavailable` and `application_device_authorization_ghes_refusal`. Their marker lines match the BPR-0771 HEAD `53de5f35` control apart from timing (`elapsed_ms`, `over_ms`, `stage_ticks`), so the environmental vault failure remains.
    - Limitation: those three courses expire at stage 1, before the repointed CLI stage and the GHES device binding run, so that `main.cpp` code is compiled but not exercised in this session.
  - Formatting of the owned source paths was reapplied with `git clang-format --force` through `bash` with split paths. An earlier zsh invocation passed the path list as one word and falsely reported 0 remaining. The change is whitespace only; the final runs rebuild first.
- **Verification plan.**
  - A Qt grouped fixture: github.com native with CLI, and GHES native with CLI. It should give two cards, correct membership, the offer text, and Connect and offer intents.
  - An A4 per-state visible-text inventory, per card and page-wide, and a check that at most one default button is visible.
  - The repointed anonymous preview course.
  - The setup-destination composed course: github.com native and CLI give one card.
  - Rerun the `qt_composed_github`, `qt_anonymous_public_preview` and `qt_settings_window` Qt courses and the GitHub public-path self-tests. The CLI and device courses fail at HEAD in this session for environmental reasons (see `BPR-0771` Evidence).

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
