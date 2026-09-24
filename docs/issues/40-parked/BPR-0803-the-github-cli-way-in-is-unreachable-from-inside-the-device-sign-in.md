---
schema: "implement-rfc.card.v1"
id: "BPR-0803"
title: "The GitHub CLI way in is unreachable from inside the device sign-in"
status: "Parked"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0016"]
depends_on: []
platforms: ["mac"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"owner note on attended candidate bpr-0797-device-browser-opens, decision 946daa16, 2026-09-17; allocated on main in d14 because no existing acceptance covers when the CLI offer is reachable","acceptance":["RFC-BPR-0015 account contract","BPR-0773 account row rule"],"owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-0803-cli-during-device-flow-v001","decision":"dbae079d-f8b8-40c4-b78a-2bafe383031d","selected":"Keep the code on screen beside the CLI choice until the reviewer ends one of them"}}
parked_on: "human-review"
acceptance: [{"criterion":"The decision is recorded","outcome":"pass","revision":"9577a516","evidence":{"kind":"observation","detail":"Owner decision dbae079d (attended session cbd48b91, 2026-09-24): keep the code on screen beside the CLI choice until the reviewer ends one of them; recorded in the card's owner-decision section."}},{"criterion":"The code stays in view: pressing Use GitHub CLI while a device flow awaits authorization keeps the code and address on screen beside the CLI choice, and the flow never keeps running unseen","outcome":"pass","revision":"9577a516","evidence":{"kind":"command","detail":"update_sign_in_surface keeps the card holding a signing-in device flow visible in the CLI-only dialog; the discovered account's offer stands beside the code; an accepted account session while the flow still asks for its code presses the flow's own Cancel. qt_composed_github_scale_100/125/200 (test_the_code_stays_beside_the_github_cli_choice) and application_device_flow_cli_choice pass; each change undone fails its row."}},{"criterion":"Both endings are exercised: a course completes the device authorization while the CLI choice is on screen, and another takes the CLI way in while the flow is running; each shows which account the profile ends signed in to and no second flow left behind","outcome":"pass","revision":"9577a516","evidence":{"kind":"command","detail":"application_device_flow_cli_choice (22 s): one account connects at once and two accounts with the CLI chosen both end signed in on the GitHub CLI profile as octo-reviewer, with the device flow ended and nothing enrolled when the browser finishes 7 s later; two accounts with the browser first ends signed in on the native profile, sign-in closed, no read opt-in, one device grant stored. The Qt row covers both endings at three scales."}},{"criterion":"The consent gates stay: the RFC-BPR-0015 read opt-in and its consent gate are crossed before any gh read, and the discovery inventory that Use GitHub CLI runs first is BPR-0909's model","outcome":"pass","revision":"9577a516","evidence":{"kind":"command","detail":"In the composed course, choosing GitHub CLI reads nothing and signs nothing in by itself; the read opt-in is answered exactly once before the CLI session in both CLI endings and never in the browser-first ending; Use GitHub CLI runs one discovery first. BPR-1023's admission check stands before every Connect."}},{"criterion":"The signed-out offer is unchanged: qt_composed_github_scale_100's existing signed-out offer assertion still passes","outcome":"pass","revision":"9577a516","evidence":{"kind":"command","detail":"qt_composed_github_scale_100 passes with its signed-out offer assertion untouched; the 39-row regression selection passes."}},{"criterion":"The rendered result gets its own attended verdict, one question on this criterion alone","outcome":"pending","revision":"9577a516","evidence":{"kind":"artifact","detail":"Prepared, not asked: one question on docs/verification/BPR-0803-code-beside-cli-choice/code-beside-cli-choice.png beside code-behind-read-opt-in-at-once.png; the exact wording is in the card's owner-ask section.","path":"docs/issues/40-parked/BPR-0803-the-github-cli-way-in-is-unreachable-from-inside-the-device-sign-in.md#owner-ask--parked-on-human-review-one-rendered-question"}}]
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

## Stale premise, re-observed 2026-09-22 (RFC-BPR-0015 r3 closeout)

This card was written on 2026-09-17, before [[BPR-0909]] (2026-09-18) made
GitHub CLI a discovered way in and [[BPR-0915]] moved Home and pasted-link
sign-in into one shared sign-in dialog. Its code citations still hold for the
per-card offer: `server_card::apply_account_row` still withdraws the card's own
CLI offer while `flowRow` is true, and `qt_composed_github_scale_100`'s
signed-out offer assertion still passes before the suite reaches its unrelated
[[BPR-0940]] failure. But a card no longer holds a precomposed CLI offer at all
in the shipped graph, and the page-level **Use GitHub CLI** entry is outside
the card row, so the title's "unreachable" is no longer true.

A temporary offscreen probe (not committed) composed `workbench_window` with
the `device_profiles()` fixture set and a GitHub CLI discovery dispatch, as
the macOS product composition wires it, then opened Settings → Accounts &
hosts, pressed the GitHub.com card's Sign in and applied an
`awaiting_authorization` device presentation. Observed at `9f06d5b1` plus the
RFC-0015 r3 working tree:

- The card's Sign in opens the shared sign-in dialog; the code and address are
  in the card's account row and the device panel's own Cancel is hidden there
  (the dialog's Cancel is the way out).
- During the flow, Use GitHub CLI is visible and enabled below the cards.
- Pressing it dispatches one CLI discovery and **no** device-flow cancel
  gesture, and switches the dialog to CLI-only, which hides the GitHub.com card
  and with it the live code and address. The device flow is left running,
  unseen.

So the question the owner's note raised has moved. The CLI is reachable
during the flow without abandoning it; what is undecided is what happens to
the flow when the reviewer takes that way in. Leaving it polling behind a
hidden card means a browser authorization completed moments later could sign
the native profile in while the CLI inventory or its read opt-in is on
screen. That interaction has not been exercised; nothing here shows it
misbehaves, and nothing shows it is safe.

Revised decision to make (still one attended question once built):

- Either pressing Use GitHub CLI during a device flow cancels that flow first
  (the code goes away because the flow ended), or the code stays on screen
  beside the CLI choice until the reviewer ends one of them. Either way the
  flow must not keep running where the reviewer cannot see it.
- The RFC-BPR-0015 read opt-in is still crossed before any read; the discovery
  inventory that Use GitHub CLI runs first is the owner-accepted BPR-0909
  model and is not traded away.
- The earlier acceptance bullets that assumed the offer was hidden during the
  flow are superseded by the two bullets above; the signed-out offer
  assertion requirement stands.

This card does not gate RFC-BPR-0015's six criteria: the opt-in, trust
warning and read/publish boundary it relies on are unchanged. The unexercised
flow-and-CLI interaction is named in the RFC-0015 independent review scope on
[[BPR-0788]].

**2026-09-23.** RFC-BPR-0015 is Auto-Verified. The product owner approved
BPR-0788's pending review, but that approval neither exercised nor accepted
this interaction. This card stays Backlog as a follow-up to the closed RFC,
and its acceptance above is unchanged.

## Owner decision, 2026-09-24: keep the code on screen beside the CLI choice (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0803-cli-during-device-flow-v001`,
revision `9f06d5b1` (re-observed 2026-09-22), fingerprint `f2843e83…`, decision
`dbae079d-f8b8-40c4-b78a-2bafe383031d`, sequence 572, 2026-09-24T18:55:40Z,
`actor: human`: **select**, verbatim:

> Keep the code on screen beside the CLI choice until the reviewer ends one of
> them

The note is empty. This answers the revised decision under "Stale premise"
above, and the alternative (cancel the device flow first) is not chosen. The
card leaves Backlog for **Ready**. The acceptance that now applies, replacing
the bullets that assumed the offer was hidden during the flow:

- **The decision is recorded** (this section).
- **The code stays in view.** Pressing Use GitHub CLI while a device flow is
  awaiting authorization keeps the device code and address on screen beside
  the CLI choice. The switch to the CLI must not hide the GitHub.com card or
  the code, as it does at `9f06d5b1`. The flow keeps running where the
  reviewer can see it until the reviewer ends one of the two: cancelling the
  flow, finishing it in the browser, or finishing the CLI way in. Whichever
  ends first, the other is ended or withdrawn visibly, never left running
  unseen.
- **Both endings are exercised.** A course completes the device authorization
  while the CLI choice is on screen, and another takes the CLI way in while the
  flow is running. Each shows which account the profile ends signed in to, and
  shows no second flow left behind.
- **The consent gates stay.** The RFC-BPR-0015 read opt-in and its consent
  gate are crossed before any `gh` read, and the discovery inventory that Use
  GitHub CLI runs first is [[BPR-0909]]'s accepted model.
- **The signed-out offer is unchanged.** `qt_composed_github_scale_100`'s
  existing signed-out offer assertion still passes.
- **The rendered result gets its own attended verdict**, one question on this
  criterion alone.

Platform: macOS, where the GitHub CLI way in is composed.

## Built, 2026-09-24 (Mac batch 7): the code stays beside the GitHub CLI choice

**What changes** (`src/adapters/qt`, presentation only; which ways in exist
is still the account contract's answer):

- **The card holding a running device flow stays in view.** In
  `workbench_window::update_sign_in_surface`, choosing GitHub CLI (Use GitHub
  CLI, or a card's GitHub CLI offer) used to switch the shared sign-in dialog
  to CLI-only. That hid every card without a discovered GitHub CLI account,
  including the GitHub.com card with the live code. Now the card that holds
  the device sign-in while it is signing in stays visible. It shows its code,
  its address and Open browser. It enters the CLI presentation only if the
  inventory found an account on its host.
- **The GitHub CLI choice stands beside the code.** In
  `server_card::apply_account_row`, the GitHub CLI offer used to give way to a
  running flow's row. Where sign-in asked for GitHub CLI and the host has a
  discovered account, it now stays on the same card beside the code, so the
  reviewer ends one or the other.
- **Whichever ends first ends the other, visibly.**
  - The GitHub CLI way in finishing first: `workbench_window::publish`
    presses the flow's own Cancel when an accepted account session arrives
    while a device flow is still asking for its code. The core decides
    whether that cancel is legal. The flow's own session arrives only after
    it has authorized, so it is never cancelled by this.
  - The device authorization finishing first: the existing rule closes
    sign-in when the session is accepted, which withdraws the CLI choice.
  - Cancelling: the dialog's Cancel ends both, as before.
- **The consent gates stay.** Use GitHub CLI still runs [[BPR-0909]]'s
  discovery inventory first. Choosing an account, or a single account
  connecting at once, still crosses the RFC-BPR-0015 read opt-in before any
  `gh` read. [[BPR-1023]]'s admission check still stands in front of every
  Connect, the GitHub CLI offer included.

**Courses.**

- `qt_composed_github_scale_100/125/200`, new
  `test_the_code_stays_beside_the_github_cli_choice`, in two runs:
  - *CLI first:* Sign in begins one flow whose code is in the dialog. Use
    GitHub CLI runs one discovery and keeps the card, code and address in
    view. The discovered account's offer then stands on the same card.
    Choosing it activates exactly profile 32 while the code stays. The GitHub
    CLI session arriving cancels the flow once; sign-in closes as finished
    (no reviewer cancel); GitHub.com ends signed in on profile 32 as
    `octo-reviewer`.
  - *Device first:* the flow authorizing and its session arriving cancel
    nothing and activate nothing; sign-in closes; GitHub.com ends signed in
    on profile 31.
- New `application_device_flow_cli_choice`: the real composition over the
  recorded GitHub.com fixture, whose device poll now answers
  `authorization_pending` until the course lets the browser finish (fixture
  option `DeviceAuthorizationPending`). Three runs:
  1. *One discovered account:* Use GitHub CLI connects it at once. The read
     opt-in is asked once, with the code still in view behind it. GitHub.com
     ends signed in on the GitHub CLI profile (2) as `octo-reviewer`. The
     device flow is ended. Letting the browser finish 7 s later enrolls
     nothing (no vault store) and the CLI session stays.
  2. *Two discovered accounts, CLI chosen:* the offer is beside the code, and
     choosing it reads nothing and signs nothing in by itself. Taking it asks
     the opt-in once, with the code behind it, and ends as run 1.
  3. *Two discovered accounts, browser first:* the device flow authorizes
     while the choice is on screen. GitHub.com ends signed in on its native
     profile (1), sign-in closes, no read opt-in is asked, and exactly one
     device grant is stored.
- **Negative controls.** Each change undone alone fails its row, and each
  passes restored:

  | Change undone | Row that fails | Assertion |
  |---|---|---|
  | The card with the flow kept visible | Qt row | "Use GitHub CLI during the flow … keeps the GitHub.com card, its code and its address in view" |
  | The offer beside the flow | Qt row | "the GitHub CLI choice stands beside the running code, on the same card" |
  | The cancel on another session | Qt row | "once the GitHub CLI way in has signed in, the device flow is ended through its own Cancel, once" |
  | The cancel on another session | Composed course | "the device flow still waiting beside it is ended" |

  The first change is not separable in the composed course: the fixture's
  inventory always finds github.com, and the card stays for that reason.
- **The signed-out offer is unchanged.** `qt_composed_github_scale_100`'s
  existing signed-out assertion passes untouched.
- Regression selection, 39 of 39 pass: `qt_composed_github`,
  `qt_anonymous_public_preview`, `qt_workbench_window`,
  `qt_device_authorization`, `qt_settings_window`, `qt_home_list` and
  `qt_surface_capture_course` at every scale; the device-authorization panel
  courses; the setup-destination, CLI, device-binding, two-account and
  public-preview public paths; and the new course.

**Rendered** (composed application, offscreen, `BPR_DEVICE_CLI_CHOICE_CAPTURE_DIR`):
[code-beside-cli-choice.png](../../verification/BPR-0803-code-beside-cli-choice/code-beside-cli-choice.png)
(the offer beside the code, two accounts),
[code-behind-read-opt-in-at-once.png](../../verification/BPR-0803-code-beside-cli-choice/code-behind-read-opt-in-at-once.png)
(the dialog behind the read opt-in, one account),
[code-behind-read-opt-in-chosen.png](../../verification/BPR-0803-code-beside-cli-choice/code-behind-read-opt-in-chosen.png)
and
[code-beside-cli-choice-then-browser.png](../../verification/BPR-0803-code-beside-cli-choice/code-beside-cli-choice-then-browser.png).

## Owner ask — parked on human-review (one rendered question)

Prepared for a later session, not asked. One question on the last criterion
alone:

**The question.** While a device sign-in shows its code, pressing Use GitHub
CLI keeps the code, the address and Open browser on GitHub.com's card. The
account GitHub CLI found appears as a button on the same card, above the code
(`code-beside-cli-choice.png`). Whichever you finish first ends the other:
the GitHub CLI sign-in cancels the waiting code, and finishing in the browser
closes sign-in. **Is this where you want to find the GitHub CLI choice while
a code is on screen?**

Bind the candidate to the build that contains this change, and show the
capture beside `code-behind-read-opt-in-at-once.png`, the one-account case,
where the choice is the read opt-in itself.

clang-format 23.1.1 passes every touched file. The project tidy script
reports nothing on `server_card.cpp` or the two courses. Its one finding is
the untouched `home_view_state.hpp:35` they include.

## Linux at `45191097` (bpr-main-linux, 2026-09-24): the Qt rows pass, and the composed course is now registered only on macOS

**The portable half passes on Linux. The composed course could not, and is
now registered only where the GitHub CLI way in is composed.** The card is
macOS; its Qt change is portable, so this station ran it.

The run was in the station's main checkout, with a clean tree, at
`45191097`, which contains `9577a516`. WSL2 Ubuntu 26.04, GCC 15.2, Qt
6.11.2, `build/dev` (Debug, `-Werror`), rebuilt at `9ee4ee50` with no
`warning:` line. `45191097` changes nothing under `src/`, `tests/` or
`cmake/` from it. `ctest --test-dir build/dev -j1 --output-on-failure`,
offscreen, with `DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS`
unset and fresh absolute `HOME` and XDG roots.

| Row | Result |
| --- | --- |
| `qt_composed_github_scale_100` | pass, 3.25 s |
| `qt_composed_github_scale_125` | pass, 3.48 s |
| `qt_composed_github_scale_200` | pass, 4.66 s |
| `application_device_flow_cli_choice` | **fail**, 15.34 s: `FAILED: one discovered account connects at once and signs in` |

The three Qt rows include `test_the_code_stays_beside_the_github_cli_choice`
and the untouched signed-out offer assertion.

**Why the composed course failed.** Use GitHub CLI's discovery in the
composition is `#if defined(__APPLE__)` code (`github_application.cpp`, the
discovery dispatch). On Linux and Windows no GitHub CLI session can arrive,
so the course's first ending waited out its 15 s bound. The setup course
gates its GitHub CLI courses with `#if defined(__APPLE__)` for the same
reason, but this row was registered on every platform. On Windows it was also
registered `WINDOWS_UNCONTAINED` without being in the pinned uncontained set
of `scripts/verify_test_network_isolation.py`, which that check refuses.

The failing run left its course root, `/tmp/device_flow_cli_choice-nsdREo`,
because `expect` ends with `std::exit` and the course's `QTemporaryDir` is
never destroyed. That is [[BPR-1121]]'s pattern. The root held only this
run's application data. Its listing and its one-line `diagnostics.log` were
saved, and it was then removed by its exact name.

**Correction, `89907c1f`.** `application_device_flow_cli_choice` is
registered only on macOS, beside a comment that says why. The executable
still builds everywhere, so its source stays portable. The
`WINDOWS_UNCONTAINED` keyword went with it. On macOS that keyword changes
nothing, so the macOS registration and the evidence above are unchanged.
After the reconfigure on Linux, `ctest -N -R '^application_device_flow_cli_choice$'`
lists no test, the three Qt rows are unchanged, and
`test_network_isolation_manifest` passes. The Windows station's next
reconfigure drops the row there too.

The acceptance record above is unchanged, and the card stays parked on its
one rendered question. The logs are station records under
`build/logs/bpr-main-linux-20260924/batch7-linux/` and are not committed:
`bpr-0803-rows.log`, `bpr-0803-leftover-listing.txt`,
`bpr-0803-leftover-diagnostics.log`, `build-dev-bpr0803-registration.log`
and `bpr-0803-registration-manifest.log`.
