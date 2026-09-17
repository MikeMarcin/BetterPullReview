---
schema: "implement-rfc.card.v1"
id: "BPR-0797"
title: "A tests-enabled build must not claim the capabilities it swapped out"
status: "In Progress"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0016"]
depends_on: []
platforms: ["mac"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"owner amendments r4 and r5, 2026-09-16, Settings > Accounts & hosts on the github.com card","acceptance":["one root cause, two reported symptoms"]}
---

# BPR-0797: A tests-enabled build must not claim the capabilities it swapped out

## Outcome

The owner reported two things from one sitting on the github.com card in
Settings → Accounts & hosts.

> "this open browser button did not open the browser."

> "I'm also pretty sure I should be able to use the GH cli flow without going
> through the device auth flow first, but i cant right now"

They have **one root cause**. `better_pull_review_composition` is compiled with
`BPR_BUILD_TESTING=1` whenever `BUILD_TESTING` is on (`src/app/CMakeLists.txt`),
and `BUILD_TESTING` defaults to on because the root `CMakeLists.txt` includes
CTest. That define does not only reveal fixtures; it **replaces two production
capabilities inside the application a person runs**:

1. `src/app/src/github_application.cpp`, the device browser seam: under
   `#if defined(BPR_BUILD_TESTING)` the composed
   `application::device_authorization_browser_port` is
   `recording_device_authorization_browser`, whose whole body is
   `LastAddress.assign(aAddress); ++Opens; return true;`. It never reaches a
   browser and it always reports success.
2. The same file, the GitHub CLI seam: under
   `#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)` the profile label is
   `"GitHub.com — GitHub CLI (uses your gh sign-in)"` and the transport factory
   is `opted_in_cli_transport_factory`; otherwise the label is
   `"GitHub.com — GitHub CLI (unavailable)"` and the factory is
   `native_github_wire_transport_factory`, whose `supports_cli()` is false. The
   GHES pair is gated identically.

So in the build the owner ran, **both** ways in on that card are dead, and one
of them says the opposite. The device panel already owns a truthful sentence
for this situation — `device_authorization_handoff::unavailable` renders "This
build cannot open a browser. Enter the code at the address above on any
device." (`src/adapters/qt/src/device_authorization_panel.cpp`) — but it is
reached only when the composed port is null, which happens only for the
explicit `--suppress-device-browser` path. A recording port that answers `true`
routes to `opened` instead, and the panel faithfully prints "Opened in your
browser." about a browser that was never asked to open.

## Acceptance

- A build that cannot open a browser never tells the reviewer that it opened
  one. The existing `unavailable` sentence is the model; the fix is which
  handoff the composition can honestly produce, not new wording invented here.
- The GitHub CLI way in is either usable in the build a developer runs, or the
  card says why it is not in words that name the reason. `(unavailable)` with
  no reason is not an answer to a reviewer who has just been told a browser
  opened that did not.
- A course asserts the **identity of the composed
  `device_authorization_browser_port` in the configuration the application is
  actually run in**. Every existing course passes today while the real button
  does nothing, because each of them either injects its own opener or asserts
  only widgets and sentences; `device_browser_opens()` has no reader in
  `tests/` at all.
- The safety rule in the existing seam's own comment is preserved: a fixture
  transport reached by mistake writes a line to a log, and a browser opened by
  mistake opens a browser. Nothing here authorizes a test run to launch a
  browser.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run** at
card authoring.

### What d8 established, against the binary the owner actually ran

- `build/dev/build.ninja` carries `-DBPR_BUILD_TESTING=1` for the composition.
- In the reviewed executable, sha256
  `d500ef7a899d2afef1477f54a37b4ca68ad1e3929bca97995d9dbd80a4399f3b`, the
  literal `GitHub CLI (uses your gh sign-in)` is **absent** and
  `GitHub CLI (unavailable)` is **present**. That is a direct observation of
  the shipped binary, not an inference from the source.
- Both gates **predate this pack**: the CLI gate came in with `d9cec406`
  ("wire public application path") and the browser gate with `9eab3e98`. The
  device panel's sentences came in with `6f520e93` ([[BPR-0280]]). This is not
  a regression from [[BPR-0771]], [[BPR-0772]] or [[BPR-0773]]; those cards
  moved the surface into view, they did not break it.
- **No accepted criterion is falsified.** [[BPR-0773]] AC1 and AC2 assert that
  Open browser, the code, the validity and the terminal states *render* in the
  account row; they do not assert that a browser opens. [[BPR-0772]] AC1
  asserts the CLI offer appears "in the composition's words", and in a
  tests-enabled composition those words are "(unavailable)". RFC-BPR-0032's own
  verification row for A2 names "the recording device port" as the direct
  surface. The pack's acceptance stands; this card is new work.
- **The owner's "without going through the device auth flow first" is already
  how the surface works, and there is a second reason they could not see it.**
  The offer's predicate is
  `!flowRow && !cliPresented && !waiting && shown.ConnectAdmitted`
  (`src/adapters/qt/src/server_card.cpp`), and `flowRow` is
  `shown.DeviceSignIn && DevicePanel != nullptr && !signedIn`. Device state only
  ever *suppresses* the offer; it is never a precondition. But the screenshot
  was taken mid sign-in, so `flowRow` was true and the GitHub CLI offer and its
  trust warning were hidden at that exact moment — by [[BPR-0773]]'s design,
  which makes the account row *be* the device sign-in while one runs. Cancelling
  the flow brings the offer back. So the honest answer to the owner has two
  parts: the offer is not gated behind the device flow, and it is nevertheless
  invisible while one is running; and even once visible it says "(unavailable)"
  in the build they were running.
- Also noted and not fixed here: even the shipped opener overstates by one
  notch. `QDesktopServices::openUrl` returning `true` means the launch was
  dispatched, not that a browser rendered, and `github_application.cpp` already
  says in a comment that a browser window "is a browser window a person sees on
  a station, which this process has no way to observe".

## Boundaries

No GitHub or GHES mutation and no live device authorization is authorized by
this card; use fixtures, recordings and the injected seams. No token, device
code or credential reaches a file, a log, a commit or a fixture. Changing which
port the shipped composition installs is a composition-root change: keep it
inside `src/app`, keep the core deciding and the host performing, and do not
widen it into a new preference.
