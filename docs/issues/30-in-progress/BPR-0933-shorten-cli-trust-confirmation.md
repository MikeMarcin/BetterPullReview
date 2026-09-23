---
schema: "implement-rfc.card.v1"
id: "BPR-0933"
title: "Shorten the GitHub CLI trust confirmation"
status: "In Progress"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P2"
---

# Shorten the GitHub CLI trust confirmation

The owner requested this follow-up in attended decision 432 on 2026-09-20:
the CLI warning dialog is too long and visually unwieldy. The decision applies
to candidate `rfc0013-v004-9c670d88-71117a5c`, source `9c670d88`.

## Acceptance

- Present a concise, readable trust decision at ordinary and narrow macOS
  window sizes; put supporting detail behind an accessible disclosure.
- Preserve the explicit configured executable/account trust, residual account
  switch and configuration risks, read versus publish authority, and an obvious
  Cancel path. This is presentation work, not expanded CLI authority.
- Verify the real sign-in/CLI opt-in composition, keyboard/accessibility flow,
  and rendered dialog without changing credentials or global CLI state.

## Scope

This records the requested follow-up only. It is separate from the RFC-0013
selected-diff repair and does not gate that milestone's acceptance.

## Implementation (RFC-BPR-0015 r3 closeout round, 2026-09-22)

The read opt-in was one bold paragraph: a static `QMessageBox::warning` with
every residual risk in its main text, which macOS draws as an `NSAlert`
headline. `bpr::qt::make_github_cli_read_consent` in
`src/adapters/qt/src/github_cli_consent.cpp` now lays the same decision out as:

- a plain-text headline question naming the exact host and the installed `gh`;
- a short consequence paragraph: the account `gh` is signed in to can change
  outside the app, the reviewer trusts this Mac's `gh` executable,
  configuration and certificates, and reading does not authorize publishing;
- every residual risk the long paragraph named (account selector and race,
  redirects, socket routing, CA certificates, child processes, environment
  overrides, refused migration, the owned temporary store, the shell credential
  helper, `gh` 2.100.0 or 2.101.0 with Apple Git 2.50.1, 32 MiB per file, the
  supervised store limit, token handling and the separate per-write
  confirmation) behind the box's own Show Details disclosure.

Connect is the only accepting button; Cancel is the default and the escape
button. The composition asks through `qt::ask_github_cli_read_consent` inside
the unchanged `#if defined(__APPLE__)` / `cli_read_offered_here()` gate. A
detailed text keeps Qt from using a native `NSAlert` (which has no disclosure),
so the box is Qt's own message box on macOS too.

The card-row warning beside the offer keeps its sentence list. Its stale Git
object claim ("requires gh 2.100.0") now says "gh 2.100.0 or 2.101.0",
matching the allowlist eecdda2a widened. The per-publication confirmation is
unchanged; it was not the dialog the owner named.

### Composition reseal review

`application_review_round_trip_composition_contract` pins
`github_application.cpp`. The only change in its scope is the read opt-in call:
the inline `QMessageBox::warning(...)` is replaced by
`qt::ask_github_cli_read_consent(Window.get(), title, host)` plus its include.
The call stays inside `#if defined(__APPLE__)` and
`if (Profile->cli_read_offered_here())`; a declined answer returns before the
identity generation, refresh-authority release and activation dispatch exactly
as `consent != QMessageBox::Yes` did. No publication, mutation, transport,
factory or boundary code changed, and the mutation-branch pin is unchanged
(`a7e2081e…`). The full-file and manifest pins are resealed to `827fad21…` and
`0ed1f246…` in the implementation commit.

`composition_contract_tests.py` now names the question each consent is asked
through (`QMessageBox::warning` for publication,
`qt::ask_github_cli_read_consent` for reading) and adds a hostile control in
which the read answer is supplied for the reviewer (`true || …`); it is
detected, as is the retitled-warning control.

### Verification so far

- `qt_composed_github_scale_{100,125,200}`: the new
  `test_cli_read_consent_is_a_short_decision` passes at all three scales
  (headline, ≤70-word decision, every residual-risk phrase behind Show Details,
  Connect/Cancel roles, accessible keyboard-reachable disclosure, closed until
  asked, Escape and Return both decline). Renders:
  `build/dev/tests/adapters_qt/composed-github-{100,125,200}-cli-read-consent-{closed,details}.png`.
  The suite then stops at the pre-existing BPR-0940 assertion
  `with GitHub CLI chosen the card speaks for it…`, which fails identically at
  the unchanged baseline; it is not counted as passing.
- `application_setup_destination_public_path` passes with the new question: the
  tests-enabled discovery courses answer it through its Yes (Connect) button.
- `application_review_round_trip_composition_contract` passes after the reseal.
- The composition contract's read-question gate matches the new call, and both
  hostile controls are refused. That test method still reports the
  pre-existing `cli_read_offered_here` definition drift from BPR-0909, owned by
  the RFC-0015 closeout, and unrelated vault-location drift.
- Pending: the rendered native macOS dialog and its keyboard/accessibility
  course in a tests-disabled build, at ordinary and narrow window sizes.
