---
schema: "implement-rfc.card.v1"
id: "BPR-0797"
title: "A tests-enabled build must not claim the capabilities it swapped out"
status: "Done"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0016"]
depends_on: []
platforms: ["mac"]
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"owner amendments r4 and r5, 2026-09-16, Settings > Accounts & hosts on the github.com card","acceptance":["one root cause, two reported symptoms"]}
acceptance: [{"criterion":"AC1 - A build that cannot open a browser never tells the reviewer that it opened one; the fix is which handoff the composition can honestly produce, not new wording","outcome":"pass","revision":"a78eefcf","evidence":{"kind":"command","detail":"Two things had to change and both did. Which handoff the composition can produce: a run outside the automated test boundary - the configuration a person's own build runs in - binds bpr::app::system_browser_opener, so the handoff it reports is a real dispatch rather than a recorder's `++Opens; return true`. Demonstrated on binary sha256 abe15f40ba8575e69d84e74686e385b77a688ddd14130635c3a867f6d895d577 at zero pending build actions, by running the SAME argv against the SAME binary with only one ENVIRONMENT_MODIFICATION entry different: marker set -> COMPOSITION_TEST_BOUNDARY:inside / COMPOSITION_BROWSER:recording; marker erased -> outside / system (build/bpr0797-d13-probe3-inside.log, build/bpr0797-d13-probe3-outside.log). That is its own negative control: the same binary yields the other answer when the one input changes. And what the panel may claim: device_authorization_handoff::opened no longer renders \"Opened in your browser.\" - that string is absent from the binary in UTF-8, UTF-16LE and UTF-16BE, and survives in the source only inside explanatory comments and one negative assertion. It renders \"Your browser was asked to open the page. If it did not, enter the code at the address above on any device.\", which claims the dispatch and not the window, and keeps the same way out the other three states offer. The existing unavailable sentence is untouched and is still the construction cut's only answer. Pinned by the new qt course test_an_accepted_handoff_does_not_claim_a_browser_opened at three scales (qt_device_authorization_scale_100/125/200 green), which asserts the absence of the old claim, the presence of \"asked to open\" and \"any device\", that the button stays enabled, and that the code and address are still on screen. LIMITATION: this establishes that the build no longer overclaims and that it binds the real opener. It does NOT establish that a browser window appears; see AC5."}},{"criterion":"AC2 - The GitHub CLI way in is either usable in the build a developer runs, or the card says why not in words that name the reason","outcome":"pass","revision":"a78eefcf","evidence":{"kind":"command","detail":"Usable, and usable means with its consent gates intact - which took two commits. b1f89533 made the offer real: the label and the transport factory now follow cli_read_offered_here() rather than `__APPLE__ && !BPR_BUILD_TESTING`, so the same binary reports COMPOSITION_CLI_READ:available and the label 'GitHub.com - GitHub CLI (uses your gh sign-in)' outside the boundary, and unavailable with the '(unavailable)' label inside it. ComposedCliReadAvailable is read off the factory the run actually holds, so it cannot disagree with the label. THEN INDEPENDENT CRITICAL REVIEW FOUND THAT b1f89533 HAD MADE THE OFFER REAL WITHOUT ITS CONSENT GATES, and it was right: the read opt-in warning in dispatch_intent and confirm_cli_publication were both still gated on BPR_BUILD_TESTING, and confirm_cli_publication's tests-enabled branch is `return true`. Confirmed on the 5f13c390 binary in three encodings: 'uses your gh sign-in' present while 'Use configured GitHub CLI', 'Confirm GitHub CLI publication' and 'Each publication requires a separate write confirmation' were all absent. That falsified BPR-0786 AC1 for the default BUILD_TESTING=ON build. Fixed at a78eefcf by moving both gates to the same run-time question. Before and after on the same fixture-less command outside the boundary: 5f13c390 printed CLI_OPT_IN_REQUIRED_FAILED with CLI_OPT_IN_GUARD_COUNT:0; abe15f40 prints CLI_OPT_IN_REQUIRED_OK with CLI_OPT_IN_GUARD_COUNT:1 (build/bpr0797-d13-cli-outside.log). Both dialog titles are now in the binary. gh_cli_transport, github_cli_process_runner, application_review_submission_contract, application_review_submission_application_host and application_review_submission_composition_public_path all pass. LIMITATION: the gh process launch itself is NOT demonstrated by d13 or its reviewer - this station's credential store is locked for the session, so every fixture-less --self-test row stops at 'The operating system credential store is unavailable.' What is demonstrated is that the consent guard refuses before a transport is built; the hop to findExecutable(\"gh\") is read from code. D14 UPDATE, ON THE OWNER'S OWN REPORT AND A RE-RUN. The owner's note on the attended candidate reads, verbatim: \"Sign in worked, but I was only able to use CLI *after* I had already done the device page sign in.\" That does not falsify this criterion and it is not dismissed. The offer is present and honestly labelled to somebody who has not signed in: qt_composed_github_scale_100 asserts cli_offer_button() visible on the github.com card with the text 'GitHub CLI (uses your gh sign-in)' at the point where sign_in_button() is still visible and default - that is, signed out, before any gesture - and it is green at e2095a3e (tests/adapters_qt/composed_github_tests.cpp:5496; build/bpr0797-d14-composed-github.log). What the note is about is when the offer is reachable, not whether it exists: server_card::apply_account_row computes offered = !flowRow && !cliPresented && !waiting && shown.ConnectAdmitted, and flowRow is true exactly while the device flow holds the account row and the user is not yet signed in, so the offer is withdrawn for the duration of the flow and returns when the sign-in completes. That is BPR-0773's rule, stated in that function's own comment. The owner walked that path and reported that order of events. Whether the offer should be reachable from inside the flow is a product question no existing acceptance answers, and it is carded as BPR-0803, which also records the competing reading - that the candidate told them not to click the row, so they used it later by choice - because the session cannot separate the two. THE LIMITATION ABOVE IS ALSO CORRECTED: the gh process launch is still not demonstrated by any agent, but the reason has changed. Re-run at e2095a3e, application_public_path_ghes_cli_unavailable now stops at PROFILE_ALREADY_OPEN, because the owner's reviewed application still holds the profile (pid 88792, the same bundle). The run never reaches a credential store, so d13's locked keychain is neither confirmed nor refuted today (build/bpr0797-d14-ghes-verbose.log). D14 did not stop the owner's application to obtain a green. The owner's remark that they used the CLI is the first indication from a person that the path functions; it is unstructured, no agent observed a spawn, and this criterion does not rest on it."}},{"criterion":"AC3 - A course asserts the identity of the composed device_authorization_browser_port in the configuration the application is actually run in","outcome":"pass","revision":"a78eefcf","evidence":{"kind":"command","detail":"application_composition_capabilities_inside_boundary and application_composition_capabilities_product_run both green. They assert the composed identity by name - COMPOSITION_BROWSER:recording and COMPOSITION_BROWSER:system - and the second is the configuration the application is actually run in, which is the half no existing course covered: device_browser_opens() had no reader in tests/ at all. Read out of CTest's own manifest, the two rows register byte-identical commands against binary abe15f40ba8575e69d84e74686e385b77a688ddd14130635c3a867f6d895d577 and differ in exactly one property entry, the product-run row's trailing BPR_AUTOMATED_TEST_BOUNDARY=unset: appended after the seam's set:1. Neither row opens anything: the probe reports and shuts down at the start stage, selects no device path so no verification address exists, runs no CLI course so it needs no platform vault, and carries --network-denied against a closed loopback port under sandbox-exec. The composed identity is also now reported in github_application_status (DeviceAuthorizationBrowser, ComposedCliReadAvailable) rather than inferred from defines. application_device_authorization_browser_handoff, _absent and _address and composed_application_hosts all still pass unchanged."}},{"criterion":"AC4 - The safety rule in the existing seam's own comment is preserved: nothing here authorizes a test run to launch a browser","outcome":"pass","revision":"a78eefcf","evidence":{"kind":"command","detail":"Preserved, and now audited rather than asserted. The marker is injected by bpr_add_network_isolated_test, the single registration seam every isolated test on all three platforms goes through, and a selected fixture counts as inside on its own so a fixture course started by hand outside CTest cannot hand a recorded address to the real opener. ENVIRONMENT_MODIFICATION rather than ENVIRONMENT, because a row's own set_tests_properties(... ENVIRONMENT ...) replaces the property after the seam returns; an independent reviewer demonstrated the CTest semantics on a purpose-built five-row CMake 4.4.3 project - set:1 survives a later row-level ENVIRONMENT write, set:1 then unset: erases, unset: then set:1 does not - and verified per row that every application_device_authorization_* course carries the marker alongside its own offscreen variable. verify_test_network_isolation.py reads the marker back out of the manifest for every configured row, names the product-run row as the sole permitted erasure and requires the two entries in that order. Its 46 positive controls pass, six of them naming the boundary. The reviewer could not find any registered row that reaches the src/app composition outside the boundary except the deliberate product-run row, and confirmed that row cannot reach a browser or gh with a logging fake gh first on PATH. TWO NAMED GAPS, recorded rather than glossed: gh_cli_git_smart_transport is a raw add_test at tests/github_contract/CMakeLists.txt:1200 that escapes the seam - the gate reports it, and it carries zero bpr::app:: symbols so it cannot compose the application; and the marker is read only in src/app/main.cpp, so tests/end_to_end/github_application_liveness_tests.cpp:55, which composes github_application in process with no fixture, is outside the boundary whatever its row says. It presses nothing. Both are follow-up work, not this card's acceptance."}},{"criterion":"AC5 (added by d13) - The owner presses Open browser on a build that binds the real opener and reports whether a browser actually appeared, and whether the sentence the panel then shows is true of what they saw","outcome":"pass","revision":"a78eefcf","evidence":{"kind":"observation","detail":"OBSERVED BY THE OWNER, AND THE HALF THAT WAS NOT IS NAMED RATHER THAN ABSORBED. Attended session e966c88f3320638208adf4fcf0651e31 at /Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review, candidate bpr-0797-device-browser-opens, fingerprint 3b3dff8b7b78f2864b4e5c07a4b04a3b9ea11daea7b678d9bf74b2a08ca40977, bound to application sha256 abe15f40ba8575e69d84e74686e385b77a688ddd14130635c3a867f6d895d577 from a78eefcf at zero pending build actions. Decision 946daa16-131e-4052-ab42-110e8e87bb75, saved 2026-09-17T02:33:38Z, actor human, outcome select, session sequence 111. FIRST HALF - PASS BY OBSERVATION. The owner ticked exactly one of six options: 'A browser window opened at the GitHub device page.' The candidate also offered 'I did not start the device sign-in, so I cannot answer the browser question' and said in words that declining was a legitimate answer which would leave this criterion unanswered rather than passed; that option was on the page and was not taken. So the window is a reported observation and not a click-through, which is the only way this half could ever close, because QDesktopServices::openUrl cannot see the window it asks for. SECOND HALF - NOT TICKED, AND RESTING ON AN AMBIGUOUS REMARK PLUS AC1. The owner ticked neither 'The sentence under the button was true of what I saw' nor 'The sentence under the button was wrong or confusing', and not 'The code and the address stayed on screen'. The candidate asked for the outcome and the wording as one decision; the session holds only the outcome. After the sitting the owner said, verbatim, 'The wording was fine.', relayed uninterpreted by the coordinator. It is probably the missing wording answer - the candidate had just asked for exactly that judgment and no other wording was under review - but not certainly, because the remark followed a conversation about the coordinator's own phrasing and nothing written down fixes its referent. It is recorded as owner input on the two sentences, not as the session's verdict. The wording half therefore rests on that remark together with AC1's course, which pins the exact text ('Your browser was asked to open the page. If it did not, enter the code at the address above on any device.') and the absence of the old claim at three scales. No tick was ever made for it and this record does not pretend one was. BOUNDARY, RECORDED HONESTLY. The owner completed the device sign-in although the candidate asked them to press Cancel: their note says 'Sign in worked.' That was their own act on their own account. No agent performed or attempted any device authorization, and no device code, user code, token or credential is recorded on this card, in any log, in any fixture or in any commit. Two consequences for a successor: this station's credential state is no longer what d13 described, and the reviewed application was still running as pid 88792 during d14's checks. The note's separate remark about the GitHub CLI is adjudicated under AC2 and carried as BPR-0803. PROVENANCE OF THE CANDIDATE, since the session's count moved from 19 to 20 during this sitting: the 20th is this candidate itself, created by d13 at 2026-09-17T01:56:35Z at revision 'b1f89533 / app sha256 5f13c390...' and replaced at 02:10:13Z with 'b1f89533 + a78eefcf / app sha256 abe15f40...' after the consent-gate correction changed the binary. The first revision sits in the session history with phase waiting and decision null: never prepared, never shown, no verdict. One id, two revisions, one verdict."}}]
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
- **Added by d13, because the four bullets above are all satisfiable without it
  and none of them is the owner's sentence.** The owner presses Open browser on
  a build that binds the real opener and reports whether a browser actually
  appeared, and whether the sentence the panel then shows is true of what they
  saw. `QDesktopServices::openUrl` returning true means the launch was
  dispatched; the window it asks for is a window on somebody's screen, which
  `github_application.cpp` says in its own comment this process cannot observe.
  No course can close this, and the card must not be Done while it is open.

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

### What d13 fixed, and what it verified for itself

The fix is `b1f89533` plus its correction `a78eefcf`, on top of the claim `f1cf9d0d`. The boundary is a
run-time fact now: `bpr_add_network_isolated_test` injects
`BPR_AUTOMATED_TEST_BOUNDARY=set:1` into every row it registers, a selected
fixture counts as inside on its own, and the composed browser identity
(`system|recording|none`) and the CLI label and transport factory all follow
that one question. `ENVIRONMENT_MODIFICATION` rather than `ENVIRONMENT`,
because a row's own `set_tests_properties(... ENVIRONMENT ...)` replaces the
property outright after the seam returns and would strip the marker from every
`application_device_authorization_*` course -- the rows that press the button.

The evidence below was produced at **zero pending build actions**
(`ninja -n` reports "no work to do" for the whole `build/dev` graph, not just
the app target), against the Mac app binary sha256
`abe15f40ba8575e69d84e74686e385b77a688ddd14130635c3a867f6d895d577`, the
final one. (`5f13c390…` was `b1f89533`'s binary and is superseded; where a
measurement below names it, it names it deliberately.)

**The discriminating observation.**
`application_composition_capabilities_inside_boundary` and
`application_composition_capabilities_product_run` register the *same* command
against the *same* binary. Their only difference, read back out of CTest's own
manifest, is that the second appends `BPR_AUTOMATED_TEST_BOUNDARY=unset:` after
the seam's `set:1`. Run directly, one binary, one argv, one environment entry
apart (`build/bpr0797-d13-probe3-inside.log`,
`build/bpr0797-d13-probe3-outside.log`):

| marker | boundary | browser | CLI read | first CLI label |
|---|---|---|---|---|
| `set:1` | `inside` | `recording` | `unavailable` | `GitHub.com — GitHub CLI (unavailable)` |
| erased | `outside` | `system` | `available` | `GitHub.com — GitHub CLI (uses your gh sign-in)` |

The second row is the configuration a person's own build runs in. This is its
own negative control: the same binary produces the other answer when the one
input changes, so a mis-detected boundary could not pass both rows.

**Courses.** 15 rows in `build/bpr0797-d13-decisive-ctest3.log`, 14 green: `qt_device_authorization_scale_{100,125,200}`
(carrying the new `test_an_accepted_handoff_does_not_claim_a_browser_opened`),
`application_composition_capabilities_{inside_boundary,product_run}`,
`application_device_authorization_browser_{handoff,absent,address}`,
`composed_application_hosts`, plus the rows the critical review made relevant:
`gh_cli_transport`, `github_cli_process_runner`,
`application_review_submission_contract`,
`application_review_submission_application_host` and
`application_review_submission_composition_public_path`. The isolation
verifier's own positive controls pass 46/46
(`build/bpr0797-d13-isolation-selftests-final.log`, six of them naming the boundary).
`python3 scripts/verify_documentation.py --root .` prints `DOCUMENTATION_OK`
(`build/bpr0797-d13-doc-gate-final.log`). The one row that fails is
`application_public_path_ghes_cli_unavailable`, on "The operating system
credential store is unavailable." - this station's keychain is locked for the
session, and that row has failed identically, same regex and same ~8.4 s, in this
session's logs since long before this card (as test #257, before the two new rows
shifted the numbering).

**The binary, in both encodings.** `Opened in your browser` occurs 0 times as
UTF-8, UTF-16LE or UTF-16BE. Both CLI labels are present, because the choice is
made at run time. `BPR_AUTOMATED_TEST_BOUNDARY`, `Your browser was asked to
open the page` and `A browser could not be opened` are each present once;
`opted_in_cli_transport_factory` and `system_browser_opener` are both linked.
No token, device code or credential is in the diff, the logs or the binary. The
one `WDJB-MJHT` in the staged diff is RFC 8628's published example user code,
already present 8 times in that same test file at `68aa7ab1`.

**`test_network_isolation_manifest` is red, and was red before this card.** To
separate the two, d13 ran `68aa7ab1`'s own copy of the verifier against this
same manifest with the exact registered argv
(`build/bpr0797-d13-head-verifier.log`): 6 failures. The current verifier
reports 7. The six are pre-existing -- three from the untouchable
`.claude/worktrees/bpr-dif-integration-rfc-3c2904` checkout, and three from
`gh_cli_git_smart_transport`. The seventh is new, and it is a true finding of
the new check rather than a new defect: that row is registered by a raw
`add_test` at `tests/github_contract/CMakeLists.txt:1200` (a file this card
does not touch), labelled `bpr_local_only_fixture`, and it escapes
`bpr_add_network_isolated_test` altogether -- which is why it already failed
three other checks. It cannot open a browser: it links
`BetterPullReview::AdapterGitHub`, `BetterPullReview::Application` and
`Qt6::Core`, never the `src/app` composition, so it never constructs
`github_application` and never binds an opener or a transport factory. **The
design's safety argument therefore has one known exception today, and that
exception is harmless for the reason just given, not by audit.** Bringing that
row under the seam is follow-up work, not this card's.

**`coding-format-check` is red, and this card made it one line worse before
d13 corrected it.** At `68aa7ab1` `src/app/main.cpp` already failed, on lines
3051-3053, 3159, 5246 and 5274. The slice as delivered failed on 928-930, 3241,
5392, 5420 and 7649: it removed 3051-3053 by rewriting them, but the new
declarations `CapabilityProbe`, `InsideAutomatedTestBoundary` and
`config.InsideAutomatedTestBoundary` joined clang-format alignment runs and
moved the required column for four *unchanged* neighbouring lines. "None of the
violations are in changed lines" was true and beside the point. d13 ran
`clang-format --lines=925:945 --lines=7645:7670` on `main.cpp` alone, changing
those four lines, and `main.cpp` is now back to exactly its three pre-existing
violations (3241, 5392, 5420 -- the same source lines as 3159, 5246, 5274,
renumbered). The gate as a whole stays red on 16 files this card never touches,
`src/adapters/qt/include/bpr/qt/workbench_window.hpp` alone accounting for 110.

### What independent critical review found, and what it cost

d13 asked an independent critical reviewer one question about `b1f89533`: find
an execution path that defeats the new safety property. It could not — every
registered row that reaches the `src/app` composition is inside the boundary,
and it demonstrated the CTest semantics the design leans on by building a
five-row CMake 4.4.3 project of its own: `ENVIRONMENT_MODIFICATION set:1`
survives a later row-level `ENVIRONMENT` write, `set:1,unset:` erases, and
`unset:,set:1` does not. It also confirmed independently that
`gh_cli_git_smart_transport` carries **zero** `bpr::app::` symbols, so the row
that escapes the seam cannot compose the application.

**But it found a blocking regression on the changed surface, and it was right.**
`b1f89533` moved the CLI transport factory to the run-time question and left two
consent gates behind on `BPR_BUILD_TESTING`:

- `src/app/src/github_application.cpp`, `dispatch_intent` — the read opt-in
  warning, the one that says the active `gh` account can change outside this app
  and that "Connecting for reads does not authorize publishing".
- the same file, `confirm_cli_publication` — whose tests-enabled branch is
  `return true`, meaning no per-publication write confirmation at all.

Both were harmless for as long as the same define also withheld the factory: a
session that could not *read* over `gh` could never reach a *write* over it
either. After `b1f89533` they were not. d13 confirmed it by reading both sites
and by scanning the `5f13c390` binary in UTF-8, UTF-16LE and UTF-16BE: `uses
your gh sign-in` present, `Use configured GitHub CLI` **absent**, `Confirm
GitHub CLI publication` **absent**, `Each publication requires a separate write
confirmation` **absent**. The build that offered the real `gh` row could raise
neither of its consent dialogs.

**That falsified [[BPR-0786]] AC1** — "read opt-in alone cannot publish; each
CLI-backed write gets a fresh confirmation naming host, observed login,
repository, PR and operation" — in the default `BUILD_TESTING=ON` build, which
is the build a developer runs and the build this card's attended candidate is
bound to. It is recorded here rather than on [[BPR-0786]], which is Done and
whose historical acceptance is not rewritten.

**Fixed at `a78eefcf`**, by the same transformation the rest of the card
applies: both gates now ask `cli_read_offered_here()` inside
`#if defined(__APPLE__)`. A run inside the boundary composes a factory whose
`supports_cli()` is false, cannot hold a CLI session, never reaches either gate
and is never handed a modal dialog it has nobody to answer with; its
`return true` stays its answer. A tests-disabled build is unchanged in every
branch. d13 checked systematically that those were the only two
`!defined(BPR_BUILD_TESTING)` gates left anywhere under `src/app/src/`.

Before and after, same command, fixture-less and outside the boundary, which is
the configuration a person's own build runs in:

| binary | verdict | guard count |
|---|---|---|
| `5f13c390` (pre-fix, measured by the reviewer) | `CLI_OPT_IN_REQUIRED_FAILED` | `0` |
| `abe15f40` (post-fix, measured by d13) | `CLI_OPT_IN_REQUIRED_OK` | `1` |

A side effect worth naming: `b1f89533` had already changed the `main.cpp`
harness to poll for that dialog whenever the run offers the read, so until
`a78eefcf` it was watching for something the build could not show, and
`cli_opt_in_presentation_passed` could not pass. It can now.

**One further gap the review surfaced, not fixed here.** The environment marker
is read only in `src/app/main.cpp`, so the "single registration seam" argument
covers the **application binary**. An in-process composition root that calls
`github_application::start` directly with no fixture is outside the boundary
whatever its row's environment says:
`tests/end_to_end/github_application_liveness_tests.cpp:55` is one, and
`application_poller_liveness` therefore composes the real opener. It presses
nothing, so nothing opens today. Every other in-process composition goes through
`github_application_fixture_access::start(fixture, …)` and is inside by
`aFixture.has_value()`. Recorded as a known limitation; it and the escaped
`gh_cli_git_smart_transport` row are the two places the audit does not reach.

**The reviewer's own limitation, stated because it bounds the above.** It could
not demonstrate the `gh` spawn itself: this station's credential store is locked
for the session, so the connect stopped before `make_cli` in both its control
and its outside run, and its logging fake `gh` was never invoked. The hop from
`github_provider.cpp:1341` to `make_cli` to `findExecutable("gh")` is read from
code, not executed. The consent-gate defect and its fix are demonstrated; the
process launch beyond them is not.

### The tests-disabled Release, which the card had claimed and not checked

Run because the correction at `a78eefcf` restructured preprocessor branches, so
the configuration nobody had compiled was exactly the one whose compilation the
change could break. `cmake --build --preset app-release`
(`BUILD_TESTING:BOOL=OFF`): **clean.** No `: error:` line, no `: warning:` line,
and a follow-up invocation reports `ninja: no work to do`
(`build/bpr0797-d13-app-release-build.log`). Release binary sha256
`4d36e20713bc268d7ab715d2a189626d2baeb3f6d1cfdb18a149b37b0ad4db03`.

**The release-absence claim is now checked rather than asserted.**
`BPR_AUTOMATED_TEST_BOUNDARY` is absent from the Release in UTF-8 and UTF-16,
as are `composition-capability-probe` and `--network-denied`. `Opened in your
browser` is absent. Both consent dialogs are present, which is the point of
`a78eefcf`, and so is `This build cannot open a browser` — the construction
cut's sentence is untouched. `rfc0017_test_inventory`,
`rfc0017_release_absence_negative_control`, `rfc0016_test_inventory`,
`rfc0017_test_inventory_contract` and `rfc0017_public_preview_decision_contract`
all pass, 5/5 (`build/bpr0797-d13-rfc0017.log`).

**One consequence worth naming, because this card's own diagnosis depended on
the thing it removes.** Two literals now survive in the Release that the `#if`
used to delete: `COMPOSITION_BROWSER:` (dead report text — `CapabilityProbe` is
compile-time `false` there) and `GitHub CLI (unavailable)` (the untaken arm of a
run-time ternary). Neither is reachable and no gate objects. But **d8 diagnosed
this very defect by running `strings` on the shipped binary** and reasoning from
which of the two CLI labels was present. That technique no longer works on
anything built after `b1f89533`: both labels are in every binary now, because the
choice moved to run time, and their presence says nothing about which one a row
shows. A future reader repeating d8's check would be misled. The replacement for
a tests-enabled build is `application_composition_capabilities_product_run`,
which reports `COMPOSITION_CLI_LABEL` from the composition itself; for a Release
there is no probe, and the honest answer is to read `cli_read_offered` or run the
application.

### Decisions

1. **Scope stays on this card.** BPR-0797 is a composition-root change and is
   not inside RFC-BPR-0032, RFC-BPR-0033 or RFC-BPR-0034's criteria. It
   falsifies no accepted criterion: d8's reading still holds, and the two new
   rows add a claim nothing previously asserted rather than repairing one.
   RFC-BPR-0032 A2's verification row names "the recording device port" as its
   direct surface, and that is still exactly what a row inside the boundary
   composes.
2. **A tests-enabled build outside the boundary now binds the real opener.**
   That is the point, and it is what makes the audit load-bearing rather than
   decorative. The safety asymmetry is unchanged in words and in force: a
   fixture transport reached by mistake writes a line to a log, a browser
   opened by mistake opens a browser, and the marker is injected by the one
   seam every isolated row on every platform goes through so that no test
   author has to remember it.
3. **Finished `human-pending`, not `done`.** Every acceptance bullet on this
   card is met by the evidence above. The owner's actual symptom is not: whether
   a browser window appears on their Mac is a window on somebody's screen, which
   `github_application.cpp` says in its own comment this process cannot observe,
   and `QDesktopServices::openUrl` returning true means only that the launch was
   dispatched. Closing this card on suite evidence would repeat the error
   [[BPR-0779]] B1 recorded, where the macOS half was false at the moment the
   owner read it. The attended candidate `bpr-0797-device-browser-opens` is
   prepared against sha256 `abe15f40…` and asks that one question.

### Remaining limitations

- Whether a real Mac draws a browser window is a target-native observation. NOT
  RUN; it is the attended candidate's question.
- A device fixture course run by hand still cannot open a browser, because a
  selected fixture counts as inside the boundary on its own. Deliberate.
- `gh_cli_git_smart_transport` escapes the registration seam (above). Follow-up.
- The marker is read only in `src/app/main.cpp`, so the seam argument covers the
  application **binary**. `tests/end_to_end/github_application_liveness_tests.cpp:55`
  composes `github_application` in process with no fixture, so
  `application_poller_liveness` is outside the boundary whatever its row's
  environment says. It presses nothing, so nothing opens today. Follow-up, with the
  row above.
- The `gh` process launch itself is NOT demonstrated, by d13 or by its reviewer.
  This station's credential store is locked for the session, so every fixture-less
  `--self-test` row stops at "The operating system credential store is
  unavailable." - which is also why `application_public_path_ghes_cli_unavailable`
  fails here, as it has in this session's logs since long before this card. What is
  demonstrated is that the consent gate now refuses before a transport is built;
  the hop from there to `findExecutable("gh")` is read from code.
- `coding-format-check` remains red on 16 files this card does not touch. `main.cpp`
  is back to exactly its three pre-existing violations.
- The tests-disabled `app-release` build is now RUN and clean, and the
  release-absence claim is checked; see the section above. `coding-tidy` is still
  NOT RUN (`BPR_ENABLE_CODING_TARGETS` is `OFF` in the `app-release` cache, and it
  was not run in `dev` either).
- `--network-denied` remains on both capability probe rows, and neither selects
  a device path, so no verification address exists in either run to hand
  anywhere.

### The attended verdict, d14 - what the owner answered and what they did not

- **The session record.** `e966c88f3320638208adf4fcf0651e31`, sequence 111,
  handed back `2026-09-17T02:33:43Z`. Candidate `bpr-0797-device-browser-opens`,
  fingerprint `3b3dff8b…`, bound to application sha256 `abe15f40…` from
  `a78eefcf`. Decision `946daa16-131e-4052-ab42-110e8e87bb75`, saved
  `2026-09-17T02:33:38Z`, actor `human`, outcome `select`.
- **Which candidate this was, since the count moved.** The session went from 19
  candidates to 20 during this sitting, and the 20th is this candidate itself, not
  a new question. d13 created it at `2026-09-17T01:56:35Z` (event 105) at revision
  `b1f89533 / app sha256 5f13c390…`, then replaced it at `02:10:13Z` (event 106)
  with `b1f89533 + a78eefcf / app sha256 abe15f40…` once the consent-gate
  correction changed the binary. The first revision is in the session's `history`
  with `phase: waiting` and `decision: null`: never prepared, never shown, no
  verdict. One id, two revisions, one verdict.
- **What the owner ticked.** Exactly one of the six options: "A browser window
  opened at the GitHub device page." The note reads, verbatim: "Sign in worked,
  but I was only able to use CLI *after* I had already done the device page sign
  in."
- **The decline option was on the page and was not taken.** "I did not start the
  device sign-in, so I cannot answer the browser question" was one of the six, and
  the candidate said in words that rejecting was a legitimate answer that would
  leave this criterion unanswered. The owner instead reported a window. **AC5's
  first half is therefore closed by observation, not by a click-through** - which
  is the only way it could ever have closed, since `QDesktopServices::openUrl`
  cannot see the window it asks for.
- **What the owner did not tick.** Neither wording line: not "The sentence under
  the button was true of what I saw", and not "The sentence under the button was
  wrong or confusing". Nor "The code and the address stayed on screen". The
  candidate asked for the outcome **and** the wording as one decision; the session
  holds only the outcome. On the session record alone, the wording half of AC5 is
  unanswered.
- **One out-of-session remark, recorded with its ambiguity rather than cleaned
  up.** After the sitting the owner said, verbatim: "The wording was fine." It was
  relayed to d14 by the coordinator, uninterpreted. It is **probably** the missing
  wording answer: the candidate had just asked for exactly that judgment, and no
  other wording was under review. It is **not certainly** that: the remark
  followed a conversation about the coordinator's own phrasing, so its referent is
  not fixed by anything written down. It is recorded here as owner input on the
  two sentences, not as the session's verdict, and the wording half of AC5 rests on
  it together with AC1's course pinning the sentence's exact text - not on a tick
  that was never made.
- **The owner completed the sign-in the candidate asked them to cancel.** "Sign in
  worked." That was their own act on their own account, and it is theirs to make.
  No agent performed or attempted any device authorization, and no device code,
  user code, token or credential is recorded on this card, in any log, in any
  fixture or in any commit. Two consequences a successor must know rather than
  discover: this station's credential state is no longer what d13 described, and
  the reviewed application was still running as pid 88792 during d14's checks.
- **The note's CLI sentence does not falsify AC2, and is not dismissed either.**
  The offer is present and honestly labelled to someone who has not signed in:
  `qt_composed_github_scale_100` asserts `cli_offer_button()` visible with the text
  `GitHub CLI (uses your gh sign-in)` at the point where `sign_in_button()` is
  still visible and default, and it is green at `e2095a3e`
  (`build/bpr0797-d14-composed-github.log`). What the note is about is that
  `server_card::apply_account_row` withdraws the offer while the device flow holds
  the row - `offered = !flowRow && !cliPresented && !waiting &&
  shown.ConnectAdmitted`, with `flowRow` true exactly while the flow is shown and
  the user is not yet signed in - which is [[BPR-0773]]'s rule, in that function's
  own comment. So the owner walked a path where the offer really was not there,
  and it came back when the sign-in finished. Whether that is right is a product
  question no existing acceptance answers: [[BPR-0803]]. The competing reading -
  that the candidate told them not to click the row, so they simply used it later -
  is recorded there too, because the session cannot separate the two.
- **A reported-open row has a different reason today, and the old reason is no
  longer verified.** `application_public_path_ghes_cli_unavailable` still fails,
  but its output is now `PROFILE_ALREADY_OPEN`: the owner's reviewed application
  holds the profile. The run never reaches a credential store, so d13's locked
  keychain is neither confirmed nor refuted today. d14 did not stop the owner's
  application to obtain a green, and a successor should not either without asking.
- **One inherited sentence corrected.** d13 recorded `coding-format-check` as red
  "on files this card does not touch". `src/app/main.cpp` is both flagged and
  changed by `b1f89533`. The accurate claim is about lines, not files: all three
  flagged lines in that file (`3241`, `5392`, `5420`) blame to `0ec1136a` and
  `284d1eac`, neither of which is this card's work.

## Current frontier - closed, bpr-shell-surfaces-20260915 d14 / rfc-integrator-d14 / mandate r8

- **Disposition.** Done, on the attended verdict recorded immediately above. Source
  revision `a78eefcf`; application sha256
  `abe15f40ba8575e69d84e74686e385b77a688ddd14130635c3a867f6d895d577` at zero
  pending build actions, re-confirmed by d14 (`ninja -C build/dev -n`: no work to
  do, at `e2095a3e`).
- **Usable behaviour, now.** A build made with this project's CMake default binds
  the real browser opener and offers the real GitHub CLI read with both of its
  consent gates present. A run inside the automated test boundary binds the
  recorder and composes a factory that cannot serve a CLI read. The boundary is one
  run-time fact, injected by the one registration seam every isolated test on every
  platform goes through, and audited out of CTest's own manifest. **And the owner
  has now seen a browser window open from the button that did nothing.**
- **Why this is Done and not still Parked.** d13 parked it deliberately, because
  closing it on suite evidence would have repeated the error [[BPR-0779]] B1
  recorded, where the macOS half was false at the moment the owner read it. That
  reason is spent: the owner pressed the button on `abe15f40…` and reported the
  window. The residual is the wording half, and it is named above rather than
  absorbed - it rests on an ambiguous out-of-session remark plus AC1's course, and
  a successor is entitled to know that no tick was ever made for it.
- **Unresolved obligations, carried forward with their true status.**
  1. `coding-tidy` - **NOT RUN**, by d13 and not by d14 either. The tests-disabled
     `app-release` build is run and clean, and `BPR_AUTOMATED_TEST_BOUNDARY` is
     confirmed absent from the Release.
  2. `test_network_isolation_manifest` - **RED**, seven findings at `e2095a3e`
     (`build/bpr0797-d14-recheck.log`). Four name `gh_cli_git_smart_transport`
     (marker, scrubs, label, runner) and are [[BPR-0802]]. Three - `raw add_test
     ownership`, `designated live source registration ... calls=2`, and
     `WINDOWS_UNCONTAINED appears 190 times ... expected between one and 105` - are
     the gate scanning `.claude/worktrees/bpr-dif-integration-rfc-3c2904`, a second
     checkout inside this one that this mandate may not touch. It is the same
     checkout that stops `architecture_boundaries`.
  3. `coding-format-check` - **RED**, 228 findings across 18 files
     (`build/bpr0797-d14-format-check.log`). None of them is a line this card
     wrote; see the correction above.
  4. `application_public_path_ghes_cli_unavailable` - **RED**, today for
     `PROFILE_ALREADY_OPEN` rather than a locked credential store
     (`build/bpr0797-d14-ghes-verbose.log`).
  5. The `gh` process launch itself is **still not demonstrated** by any agent. The
     owner's note says they used the CLI, which is the first indication from a
     person that the path functions, but it is an unstructured remark and no
     course covers the spawn. What is demonstrated is that the consent guard
     refuses before a transport is built.
  6. Seven sandbox-blocked courses on [[BPR-0775]] remain blocked.
  7. `strings` on a binary can no longer tell which GitHub CLI label a row shows,
     because both arms of a run-time ternary are present in every build. That is
     the technique d8 used to diagnose this card. A successor auditing a Release
     should not reach for it.
- **Pack state at closure.** Every other card in `bpr-shell-surfaces-v1` is Done:
  [[BPR-0770]] through [[BPR-0783]], [[BPR-0789]], [[BPR-0790]], [[BPR-0791]] and
  [[BPR-0796]]. The authored `verification-closeout` milestone's work is
  [[BPR-0779]], [[BPR-0780]], [[BPR-0781]], [[BPR-0782]] and [[BPR-0791]], all
  Done, and the plan's `unplanned_work` is empty. Deferred to Backlog on purpose:
  [[BPR-0798]], [[BPR-0799]], [[BPR-0800]], [[BPR-0801]], [[BPR-0802]] and
  [[BPR-0803]].
- **What formal closure still waits on, unforced.** RFC-BPR-0032, RFC-BPR-0033 and
  RFC-BPR-0034 are all `Proposed`, and the terminal status is `Auto-Verified`.
  RFC-BPR-0032 depends on RFC-BPR-0016 (`Auto-Verified`), RFC-BPR-0018
  (`Proposed`) and RFC-BPR-0028 (`Proposed`); RFC-BPR-0033 on RFC-BPR-0024
  (`Deferred`); RFC-BPR-0034 on RFC-BPR-0006 (`Auto-Verified`) and RFC-BPR-0018
  (`Proposed`). Three dependencies are therefore not terminal, so none of the three
  can be closed in canonical order yet. That ordering is the remaining work on this
  pack, and no status here is advanced to make the board look finished.
- **Next bounded objective.** Nothing in this pack's authorized scope. The pack is
  complete apart from the strictly-ordered RFC closure above, which is gated on
  RFC-BPR-0018, RFC-BPR-0028 and RFC-BPR-0024 rather than on anything in it.
- **Where the evidence lives.** `build/bpr0797-d13-*.log` and
  `build/bpr0797-d14-*.log` in this checkout (`/build/` is git-ignored), the
  attended session at
  `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/attended-review`, and d10's
  evidence at `/Users/jaeger/.mask/bpr-shell-surfaces-d7-20260915/d10-evidence/`.

## Boundaries

No GitHub or GHES mutation and no live device authorization is authorized by
this card; use fixtures, recordings and the injected seams. No token, device
code or credential reaches a file, a log, a commit or a fixture. Changing which
port the shipped composition installs is a composition-root change: keep it
inside `src/app`, keep the core deciding and the host performing, and do not
widen it into a new preference.
