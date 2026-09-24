---
schema: "implement-rfc.card.v1"
id: "BPR-1023"
title: "Reconcile the anonymous-preview Accounts course with shared sign-in's Connect, and decide whether a refused activation stops a device-capable card's Connect"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0940 integration pass, Mac integrator, 2026-09-23","found_at":"f839e26a","related_cards":["BPR-0940","BPR-0915","BPR-0772","BPR-0597"],"suite":"qt_anonymous_public_preview_scale_100/125/200","owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-1023-refused-connect-v001","decision":"ad887ef9-fcdf-40f1-8d47-de059fe82203","disposition":"delegated to the implementer with two stated preferences"}}
---

# BPR-1023: Reconcile the anonymous-preview Accounts course with shared sign-in's Connect

## Observation

`qt_anonymous_public_preview_scale_100`, `_125` and `_200` stop in
`test_accounts_offer_no_public_preview_activation`
(`tests/adapters_qt/anonymous_public_preview_tests.cpp`) at

`GitHub.com's Connect, while it is not chosen, activates exactly its native HTTPS profile`

[[BPR-0597]] recorded this as untracked and pre-existing. It is the same drift
[[BPR-0940]] reconciled in the composed GitHub suite: since [[BPR-0915]]
(`cbd50b74`), a server card's Connect on a host with a device flow chooses
that host's native profile and begins device sign-in in the shared sign-in
dialog (`workbench_window::add_server_card`, the `.Connect` gesture, through
`begin_selected_sign_in`). It no longer dispatches the activation intent that
the [[BPR-0772]] course expects, and the course's closing claim that "neither
Connect nor the GitHub CLI offer emits device sign-in" no longer holds.

A diagnostic run at `f839e26a` with the course's `expect` made non-fatal
(not committed) observed, in order:

- GitHub.com's Connect while not chosen: 0 activation intents, 1 device begin.
- Five later assertions of the same course fail for the same reason (the card
  is in the dialog's setup presentation, and Connect was the device retry).
- **With the activation command refused** (`activate_host_profile` disabled,
  `invalid_profile`), a click on GitHub.com's hidden Connect dispatched no
  activation intent but **began a second device sign-in** (device begins
  1 → 2). The enterprise card's Connect and the GitHub CLI offer dispatched
  nothing.

## Decision needed

The course's last row asks that a card's ways in obey the host activation
command state. The Connect button is hidden while activation is refused, so
a reviewer cannot press it; but its handler, reached programmatically, begins
device sign-in because `begin_selected_sign_in` clicks the device panel's
begin button, which the device flow's own commands govern. Decide one of:

1. A device-capable card's Connect is refused whenever its Connect is not
   admitted (the handler returns before choosing or beginning), matching
   the enterprise card and the GitHub CLI offer; or
2. Device begin is the core's separate command and correctly governs this
   path; the course's last row then asserts the device command's state
   rather than the activation command's.

Either way, reconcile the course to the shipped [[BPR-0915]] entry path
(Connect on GitHub.com begins device sign-in in the shared dialog) without
weakening the exact-profile checks.

## Acceptance

- AC1: The decision above is recorded with its reason, and the product or the
  course changes accordingly, with a failing-before control. The owner
  delegated the decision to the implementer on 2026-09-24 (below), so the
  reason must answer his two stated preferences.
- AC2: `test_accounts_offer_no_public_preview_activation` follows the shared
  sign-in entry path, keeps its exact-profile and no-public-preview checks, and
  passes at all three scale registrations.

## Boundaries

Fixture and Qt-level only; no live GitHub, credential or device-flow request.

## Owner decision, 2026-09-24: the implementer decides (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-1023-refused-connect-v001`, revision
`f839e26a`, fingerprint `7cd1b578…`, decision
`ad887ef9-fcdf-40f1-8d47-de059fe82203`, sequence 568, 2026-09-24T18:52:18Z,
`actor: human`. The page required a selection, and the owner selected **both**
options:

> 1. Refuse it: a device-capable card's Connect does nothing while its Connect
> is not admitted, like the enterprise card and the GitHub CLI offer
>
> 2. Allow it: device begin is the core's separate command and governs this
> path; the course asserts the device command's state instead

Note, verbatim:

> Defer decision to the implementer, prefer a flow that makes it essy to reach
> the CLI as this is likely the most common adoption path and prefer a flow
> that is the least surprising between different areas of the app.
>
> Also attended review feedback: for a multiple choice I should alwasy be able
> to write-in. Right now I have to select something.

**What this means for the card.** The two selections are not a choice between
the options; the note says so. The decision is **delegated to the implementer
who takes this card**, bounded by two preferences:

1. the flow makes the GitHub CLI easy to reach, because it is likely the most
   common way people adopt the product;
2. the flow is the least surprising between different areas of the app.

The card leaves Backlog for **Ready**: nothing in it waits for the owner now.
AC1 records the implementer's choice and its reason against both preferences.

**The integrator's reading, for whoever takes it (not a decision).**
Preference 2 points to option 1. Under it, every way in on a card obeys the
same admission, so a refused activation stops Connect, device begin and the
GitHub CLI offer alike. Today the enterprise card and the CLI offer already
behave that way. As the card describes the two options, preference 1 does not
separate them: the CLI offer dispatches nothing under a refused activation in
either one. It constrains the reconciled course instead. The course has to
keep proving that the GitHub CLI way in is reachable wherever it is admitted,
and must not trade that away to make the refusal row pass. If the implementer
finds that one option does change how reachable the CLI is, preference 1
decides, and AC1 says how.

**The attended-review remark** (a multiple-choice question should always
allow a write-in answer) is about the review tool, not this product. The
shared `attended-review` skill is MASK-managed and is not edited here; the
remark was passed to the coordinator on 2026-09-24 as tool feedback.

## Decision and resolution, 2026-09-24 (Mac batch 7)

**Decision: option 1. A device-capable card's Connect is refused whenever its
Connect is not admitted.** The handler returns before it chooses a host,
opens the sign-in dialog, or begins anything. The enterprise card's Connect
and the GitHub CLI offer already worked this way.

**Why, against the owner's two preferences.**

1. *Least surprising between areas of the app.* Under option 1, each control
   answers to the command it is named and shown for:
   - Every **Connect** (the enterprise card's, GitHub.com's, and the GitHub
     CLI offer, which is a Connect to the CLI profile) answers to the core's
     host-activation command. The card already shows or hides all three by
     that command (`ConnectAdmitted`).
   - Every **Sign in**, **Begin** or **Get a code** answers to the device
     command. The card shows Sign in by `SignInAdmitted`, the device begin's
     state. Home and the shared sign-in dialog begin through that same device
     panel, so they are unchanged.

   Under option 2, the one word Connect on one page would answer to two
   commands, depending on the card. A hidden Connect would still act, and the
   same Connect would do different things on the page and in code. Option 1
   makes what a control does match whether it is shown.
2. *Easy to reach the GitHub CLI.* Neither option changes how reachable the
   CLI is. Under both, the GitHub CLI offer shows whenever host activation is
   admitted, beside Connect or Sign in, and activates exactly the CLI profile.
   The reconciled course keeps proving that at each point where it is
   admitted: with GitHub.com chosen and presenting native HTTPS, and again
   after the enterprise host is chosen. Option 1 also stops a refused CLI offer
   from half-acting, when reached anyway. Before, it still opened the sign-in
   dialog and presented CLI choices that could not be taken, a partial
   response more surprising than none. The integrator's reading on this card
   is borne out: preference 1 does not separate the options, so preference 2
   decides.

**What changes.** `workbench_window::add_server_card`'s Connect gesture now
asks `github_session_panel::connect_enabled()` first, the same admission the
card's `ConnectAdmitted` reads, and does nothing without it. Nothing a
reviewer can see changes: every such Connect is already hidden while
activation is refused. In the shipped core, `github_commands` admits
activation always, so the refusal is reachable only when a command projection
refuses it, as this course sends one.

**The course, reconciled to [[BPR-0915]]'s entry path**
(`test_accounts_offer_no_public_preview_activation`):
- GitHub.com's Connect, while not chosen, now chooses exactly its native
  HTTPS profile (`selected_profile` 1, the card current on profile 1). It
  begins one device sign-in and dispatches no activation, in the shared
  sign-in dialog for GitHub.com alone. The course closes the dialog, as
  `qt_composed_github` does, and goes on in Settings.
- The exact-profile and no-public-preview checks are kept. The enterprise
  Connect activates exactly profile 7 (twice), the GitHub CLI offer activates
  exactly profile 9 with the authenticated mode, and the page carries nothing
  for reading without signing in.
- The device-begin count is exact throughout: one, from GitHub.com's Connect.
  The GitHub CLI offer and the enterprise Connect begin none.
- The last row: under the refused activation command, GitHub.com's Connect,
  its GitHub CLI offer and the enterprise Connect are hidden. Clicked anyway,
  none dispatches an intent, begins a second device sign-in, or opens the
  sign-in dialog, and the enterprise card stays chosen.

**Verification** (macOS 26.6.2 arm64, `build/dev`):
- `qt_anonymous_public_preview_scale_100`, `_125` and `_200` pass. They
  failed before at "GitHub.com's Connect, while it is not chosen, activates
  exactly its native HTTPS profile".
- **Failing-before control (AC1).** With the new admission check removed from
  the Connect gesture, the reconciled course fails at "every way in on a card,
  device sign-in included, obeys the host activation command state": the
  device begins go from 1 to 2. Restored, it passes.
- Also passing, 26 of 26: `qt_composed_github_scale_*`,
  `qt_workbench_window_scale_*`, `qt_device_authorization_scale_*`,
  `qt_settings_window*`, `qt_home_list_scale_*`,
  `qt_surface_capture_course_scale_*`, `application_device_authorization_panel_course`
  and `_absent`, and `github_cli_process_runner`. The setup, CLI and
  public-preview courses pass 21 of 21, including
  `application_setup_destination_public_path`,
  `application_anonymous_public_preview_public_path` and
  `application_profile_two_account_isolation_public_path`.
- clang-format 23.1.1 passes both touched files.
- The project tidy script reports nothing on the two touched translation
  units. Its three findings are in untouched headers they include
  (`dif_runtime.hpp:1`, `home_view_state.hpp:35`).
