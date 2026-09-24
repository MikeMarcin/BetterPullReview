---
schema: "implement-rfc.card.v1"
id: "BPR-1023"
title: "Reconcile the anonymous-preview Accounts course with shared sign-in's Connect, and decide whether a refused activation stops a device-capable card's Connect"
status: "Ready"
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
