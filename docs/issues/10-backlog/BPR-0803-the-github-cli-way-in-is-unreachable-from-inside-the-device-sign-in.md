---
schema: "implement-rfc.card.v1"
id: "BPR-0803"
title: "The GitHub CLI way in is unreachable from inside the device sign-in"
status: "Backlog"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0016"]
depends_on: []
platforms: ["mac"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"owner note on attended candidate bpr-0797-device-browser-opens, decision 946daa16, 2026-09-17; allocated on main in d14 because no existing acceptance covers when the CLI offer is reachable","acceptance":["RFC-BPR-0015 account contract","BPR-0773 account row rule"]}
---

# BPR-0803: The GitHub CLI way in is unreachable from inside the device sign-in

## Outcome

The owner accepted [[BPR-0797]]'s browser fix on 2026-09-17 and added one
sentence, verbatim:

> Sign in worked, but I was only able to use CLI *after* I had already done the
> device page sign in.

That sentence is not about the browser, and nothing on [[BPR-0797]] answers it.
It is about **when** the GitHub CLI way in is reachable.

**What is already settled, and is not this card's question.** The GitHub CLI
offer is present and labelled honestly while signed out, before any gesture:
`qt_composed_github_scale_100` asserts `cli_offer_button()` is visible on the
github.com card with the text `GitHub CLI (uses your gh sign-in)` at the point
where `sign_in_button()` is still visible and default — that is, signed out —
and it is green at `e2095a3e`. So the CLI is offered to someone who has not
signed in. [[BPR-0797]] AC2 is not falsified by the owner's note.

**What the note is about.** The offer is deliberately withdrawn for the duration
of the device flow. `server_card::apply_account_row` computes

    offered = !flowRow && !cliPresented && !waiting && shown.ConnectAdmitted

with `flowRow = shown.DeviceSignIn && DevicePanel != nullptr && !signedIn`, so
while the device sign-in occupies the account row and the user is not yet signed
in, the CLI offer and the other ways in are hidden. That is [[BPR-0773]]'s rule,
stated in that function's own comment: "While signing in, the account row is the
device sign-in: the code, the browser and the way out replace who is signed in and
the ways in." Once the sign-in completes, `flowRow` is false and the offer returns.

So a reviewer who starts the device flow and then decides they would rather use
their existing `gh` sign-in has to abandon the flow to find the offer again. The
owner walked exactly that path and reported exactly that order of events.

**The ambiguity, stated rather than resolved.** Two readings of the note survive
the code, and d14 could not separate them from the session record:

1. The offer was hidden while the owner was inside the flow, so they could not
   reach it until the flow ended — which is what the code above does.
2. The candidate told them "Do NOT click that row for this review", they
   complied, and they only exercised the CLI afterwards by choice — in which case
   the note reports their sequence and not a constraint.

Reading 1 is a real product question either way, because the code does withdraw
the offer. Do not close this card by deciding the note meant reading 2.

## Acceptance

- Decide, and write down, whether a reviewer inside the device flow should be
  able to switch to the GitHub CLI way in without first abandoning the flow.
  Either answer is defensible; [[BPR-0773]]'s rule is that the row becomes the
  flow, and a second way in beside a live device code is a real crowding problem.
  What is not defensible is leaving it undecided after the owner asked.
- If the answer is yes, the CLI offer is reachable during the flow without
  cancelling it, the device code and address stay on screen while it is reachable,
  and the RFC-BPR-0015 opt-in warning and its consent gate are still crossed
  before any `gh` process is built — the guard [[BPR-0797]] restored at `a78eefcf`
  is not traded for convenience.
- If the answer is no, [[BPR-0773]]'s rule is stated where a reviewer meets it,
  not only in a source comment: the flow's own way out says that cancelling
  returns the other ways in. A hidden control with no explanation is what produced
  this note.
- The rendered result gets its own attended verdict on the surface, one question
  for this criterion alone. Whether a control is findable at the moment someone
  wants it is not answerable from an offscreen course.
- `qt_composed_github_scale_100`'s existing signed-out assertion still passes
  unchanged. Whatever this card does to the flow, the offer a signed-out reviewer
  sees before any gesture is already accepted behaviour.

## Evidence

Evidence is **not run** at card authoring: nothing here has been implemented, and
no attended question has been asked about it.

Where the existing evidence lives:

- The owner's verdict and note: attended session
  `e966c88f3320638208adf4fcf0651e31` at
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review`, candidate
  `bpr-0797-device-browser-opens`, fingerprint `3b3dff8b…`, decision
  `946daa16-131e-4052-ab42-110e8e87bb75`, bound to application sha256
  `abe15f40…` from source `a78eefcf`.
- The signed-out offer: `tests/adapters_qt/composed_github_tests.cpp:5496`,
  "each server offers GitHub CLI in its own composition's words", and the
  adjacent `sign_in_button()` assertion that fixes the state as signed out.
  Re-run green by d14 at `e2095a3e`: `build/bpr0797-d14-composed-github.log`.
- The withdrawal: `src/adapters/qt/src/server_card.cpp`,
  `server_card::apply_account_row`, and `shown.DeviceSignIn` as computed in
  `workbench_window::update_server_cards`.
- The consent gates that must survive any change here: [[BPR-0797]] AC2, and
  [[BPR-0786]] AC1, which an independent critical review falsified once already
  when the offer was made real without them.

## Boundaries

No GitHub or GHES mutation and no live device authorization is authorized by this
card. No token, device code or credential reaches a file, a log, a commit or a
fixture. The core decides, the host performs, Qt presents: which ways in exist is
the account contract's answer, and what the row shows while a flow runs is
presentation — do not move the decision into the widget. Do not widen this into a
new preference.
