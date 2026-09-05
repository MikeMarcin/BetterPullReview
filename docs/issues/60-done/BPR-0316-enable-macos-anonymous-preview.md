---
schema: "implement-rfc.card.v1"
id: "BPR-0316"
title: "Enable anonymous public preview on target-native macOS"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0306"]
---

# Enable anonymous public preview on target-native macOS

## Required final state

- The exact target-native candidate course passes while production selection is
  unavailable, then this card flips only macOS public-preview selection in the
  same commit and reruns the complete isolated course against the enabled
  tests-disabled Release artifact.
- The course covers native HTTPS, zero Keychain/header, REST comments, rate/404,
  public Git smart HTTP, cache/epoch/diff, system browser/sign-in handoff, Cocoa
  presentation, runtime/dependencies, fixture absence, and hygiene.
- The one-time flip is provenance; BPR-0309 owns idempotent immutable final-head
  replay of already-enabled macOS after simplification/documentation.

## Authorized and claimed — 2026-09-02

[[BPR-0306]] is terminal. The product owner authorized the native candidate
proof this card requires — in full: "authorized" — against the same public
pull request the RFC-BPR-0017 probe captures came from,
`octocat/Hello-World#1`. Reading that pull request through the application's
own anonymous provider issues one REST request per eligible operation —
nine in all, each unauthenticated, each spending one unit of the shared
hourly core budget — and no GraphQL, viewer, or mutation request; that
count is stated here so the authorization is read as the read it is. No
credential, header, or vault is involved. Claimed straight into
`30-in-progress/` by the integrator; the unattended half — a tests-enabled
live-candidate flag over the production factories and the probe verdict for
a shipped enabled selection — runs as a worker slice, then the integrator
performs the authorized read once, records it, and lands the macOS flip with
the enabled course in the same commit, as the required final state orders.

## Part one landed — 2026-09-03

Integrated at `969dc1a4` with the card open. Nothing here sends a request;
the authorized read and the flip follow.

**The live-candidate flag.** Under `BPR_BUILD_TESTING`,
`--public-preview-live-candidate <owner> <repository> <number>` forces the
public-preview selection available for the run and composes the
**production** factories — `make_native_https_anonymous_read_transport` for
the anonymous provider and `make_native_https_anonymous_smart_transport`
for the Git path, system proxy discovery and system trust, no fixture, and
no `_for_test` factory, which is unreachable from composition by
construction because `BPR_NATIVE_HTTPS_TESTING` is private to the adapter
target — then activates public preview on the GitHub.com profile and opens
the named pull request through the anonymous provider. It prints the
enabled course's markers plus `PUBLIC_PREVIEW_LIVE_CANDIDATE_PRODUCTION_FACTORIES`,
`PUBLIC_PREVIEW_WIRE_REQUESTS` (counted in a wrapper around the shipped
transport, never parsed from logs), `PUBLIC_PREVIEW_WIRE_CAPABILITY_PROBES`,
`PUBLIC_PREVIEW_WIRE_CREDENTIALED_REFUSALS`, `PUBLIC_PREVIEW_BUDGET`
(limit/remaining/reset from the session's core rate limits),
`PUBLIC_PREVIEW_SNAPSHOT_MODE`, `PUBLIC_PREVIEW_CANDIDATE:<owner>/<repo>#<n>
<head-sha>`, and `PUBLIC_PREVIEW_LIVE_CANDIDATE_OK|FAILED` — no bodies and
no comment text, because this output goes on a card. It refuses before
composition without `--self-test`, with a fixture, a GHES host, a course
flag, or a device flag, or without a run budget. The anonymous factory
refuses both credentialed entries and counts the attempt, so no authorized
transport can exist on the path; there is no vault scan for a stored
credential, which is stated rather than claimed.

**A dry run proves the wiring without a network.**
`--public-preview-live-candidate-dry-run` composes the same graph and stops
before the read; `application_anonymous_public_preview_live_candidate_wiring`
asserts `PRODUCTION_FACTORIES:true`, `WIRE_REQUESTS:0`, and the terminal
marker under network isolation. The worker's mutation routed the anonymous
provider to the test transport factory instead: the row failed on
`PRODUCTION_FACTORIES:false`; restored, it passes.

**The probe verdict for an enabled shipped selection.** `--public-preview-probe`
now splits its no-course branch: not composed → `STAGE:unavailable`,
`REQUESTS:0`, `PUBLIC_PREVIEW_UNAVAILABLE_OK`; composed → signed out, no
refusal, `REQUESTS:0`, `HEADER_EFFECTS:0`, `VAULT_EFFECTS:0`,
`SMART_ACQUISITIONS:0`, at least one publication → `PUBLIC_PREVIEW_ENABLED_OK`.
The fixture course still requires its reads. A new tests-enabled
`--public-preview-selection-enabled-probe` forces the selection with no
course, and `application_anonymous_public_preview_enabled_probe` pins its
markers; the platform course's `enabled` expectation is therefore
satisfiable by the shipped probe once macOS flips.

Both new rows are pinned uncontained and allocated in the inventory with
their flags. The composition conflicted with [[BPR-0515]]'s override at
integration — both adopt an option outside the fixture branch — and was
merged keeping both; the pins were recomputed on the merged tree in two
passes.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The dev graph builds clean on the merged tree | `cmake --build --preset dev` | exit 0, zero `error:` or `warning:` lines | this commit |
| Every device-authorization and public-preview row including the two new ones, the composed-host audit, both inventories and their contracts, the manifest gate, and both composition contracts | `ctest --preset dev -R '^(application_device_authorization_(unavailable\|release_contract\|public_path\|registered_course\|browser_handoff\|browser_absent\|panel_course\|panel_absent)\|application_anonymous_public_preview_(public_path\|unavailable\|release_contract\|ghes_refusal\|enabled_probe\|live_candidate_wiring)\|composed_application_hosts\|rfc0016_test_inventory\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|rfc0017_platform_course_launch_contract\|test_network_isolation_manifest\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 22/22 passed | this commit |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the touched files | exit 0, no output | this commit |
| The shipped image knows no live-candidate flag and still answers unavailable | `cmake --build --preset app-release`; the inventory's `--all --release-binary` scan; `strings`; the shipped probe | `RFC0017_TEST_INVENTORY_OK`; 0 occurrences; `COMPOSED:false`, `STAGE:unavailable`, `REQUESTS:0`, `PUBLIC_PREVIEW_UNAVAILABLE_OK` | this commit |

## The authorized read, first attempt — 2026-09-03

Performed once by the integrator on the macOS host, from the tests-enabled
dev artifact at `7e0776b6`, offscreen:

```text
better_pull_review --self-test --public-preview-live-candidate octocat Hello-World 1 --automated-run-budget-ms 30000
```

What it printed, in full for the public-preview markers:

```text
PUBLIC_PREVIEW_COMPOSED:true
PUBLIC_PREVIEW_STAGE:enabled
PUBLIC_PREVIEW_REQUESTS:1
PUBLIC_PREVIEW_HEADER_EFFECTS:0
PUBLIC_PREVIEW_VAULT_EFFECTS:0
PUBLIC_PREVIEW_SIGN_IN_REQUESTS:0
PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:0
PUBLIC_PREVIEW_WRITE_REFUSALS:0
PUBLIC_PREVIEW_LIVE_CANDIDATE_PRODUCTION_FACTORIES:true
PUBLIC_PREVIEW_WIRE_REQUESTS:24
PUBLIC_PREVIEW_WIRE_CAPABILITY_PROBES:0
PUBLIC_PREVIEW_WIRE_CREDENTIALED_REFUSALS:0
PUBLIC_PREVIEW_BUDGET:0/0/0
PUBLIC_PREVIEW_SNAPSHOT_MODE:authenticated
PUBLIC_PREVIEW_CANDIDATE:octocat/Hello-World#1
PUBLIC_PREVIEW_LIVE_CANDIDATE_FAILED
PUBLIC_PATH_BUDGET:expired=1 budget_ms=30000 elapsed_ms=30014
PUBLIC_PATH_STATUS:Loading the pull request.
```

**What it establishes.** The production factories were composed
(`PRODUCTION_FACTORIES:true`), the session was activated signed out
(`STAGE:enabled`), no credentialed transport was built and no header or
vault effect occurred, and the shipped anonymous transport issued
twenty-four REST requests to the live service with no `Authorization` —
the wire-level facts this card's candidate proof exists for.

**What it did not establish.** No snapshot was accepted: the run's budget
expired while the pull request was still loading, the snapshot mode is the
default, no head is reported, and the session's rate-limit projection was
never populated (`BUDGET:0/0/0`), which means no provider read completed.
Twenty-four wire requests for one provider read is not the nine the
operation set implies; the only mechanism in the tree that repeats requests
is the host's retry of a `network` or `timeout` failure, up to three
attempts, so the reading consistent with the counts is three attempts of
eight requests each, one operation failing each time in a way the markers
do not name. Nothing in this output says which, and the flag prints no
per-operation outcome — an omission of the flag, not of the transport.

**Cost.** The owner authorized one read; this attempt spent twenty-four
unauthenticated core requests of the shared hourly budget of sixty, none of
them a GraphQL, viewer, or mutation request. No second attempt is made
without a fresh authorization, and the flag first gains what a second
attempt needs to be diagnostic: a single provider attempt so it costs at
most nine requests, and a per-operation outcome marker naming the operation
and its status or failure kind, with no bodies.

## Part two landed — 2026-09-03

Integrated at `df889980`; the pins recomputed on the merged tree. The
live-candidate course is now diagnostic and bounded, so a second authorized
read costs at most nine requests and names what fails.

**One attempt on the wire.** The counting transport refuses every request
after a composition's first `github_wire_error`: it answers `cancelled`
without forwarding, counting, or recording, so the host's three-attempt
retry still runs and costs nothing outside the process, and
`PUBLIC_PREVIEW_WIRE_REQUESTS` means *reached the wire*. An in-process
self-check drives the real counting transport over a stub that always
answers `network` and asserts one forward, one record, and a second
request answered `cancelled`; it prints `PUBLIC_PREVIEW_WIRE_SINGLE_ATTEMPT`
on every live run and gates the verdict. The worker's mutation deleted the
short-circuit: the wiring row failed on `SINGLE_ATTEMPT:false`; restored.

**One line per forwarded request.** `PUBLIC_PREVIEW_WIRE_OPERATION:<operation>:<page>:<outcome>:<elapsed_ms>`,
in order, before `WIRE_REQUESTS`; the outcome is `http_<status>`, or
`error_<enumerator>` with `.retry_after=<n>` and `.rate=<limit>/<remaining>/<reset>`
when present, or `cancelled` — no body, no URL, no other header.
`PUBLIC_PREVIEW_LIVE_ELAPSED_MS` prints before the terminal marker, and the
records are read from the transport at print time rather than from the
tick snapshot, so an expired run still reports them: a dry run with a
1 ms budget printed the whole block and then `_FAILED`. The first attempt's
24 requests would have printed as 24 such lines.

The merge conflicted with the C4456 rename in the same verdict block; the
worker's one added conjunct was applied to the renamed variable, and a
guard confirmed no reference to the outer variable remains inside the live
block. `application_anonymous_public_preview_live_candidate_wiring` now
asserts the new markers with zero requests.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The merged graph builds clean and the pins bind it | `cmake --build --preset dev`; the round-trip contract, one repin | exit 0; green | this commit |
| Every public-preview row including the wiring row, both dev-tree device rows, the composed-host audit, the inventory and manifest gates, and both composition contracts | `ctest --preset dev -R '^(application_anonymous_public_preview_(live_candidate_wiring\|enabled_probe\|public_path\|unavailable\|release_contract\|ghes_refusal)\|application_device_authorization_(unavailable\|release_contract)\|test_network_isolation_manifest\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|composed_application_hosts\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` | 14/14 passed | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` on `main.cpp` after the conflict resolution; the worker's three files before it | exit 0, no output | this commit |

## Diagnostic live read — 2026-09-04

The owner asked this task to implement the remaining work and perform the
attended work available on this Mac. After rebuilding at `e6d47155` with the
BPR-0556 SDK compatibility repair, the integrator ran the same named public
target once with the same 30,000 ms run budget. This was an unauthenticated
read; no publication or credential was involved.

The result disproves this card's claimed nine-request upper bound. One provider
attempt still paginates. The transport forwarded 29 requests: repository,
pull request, files, and commits each returned HTTP 200; reviews returned
HTTP 200 for pages 1 through 14; review comments page 1 returned HTTP 200;
issue comments pages 1 through 9 returned HTTP 200; issue comments page 10
was cancelled when the run expired. Elapsed time was 30,016 ms.
`WIRE_SINGLE_ATTEMPT:true` therefore proves retry suppression only. It does
not bound successful pagination or authorize a claim that nine operations
cost nine HTTP requests. The earlier inference that the first 24 requests
represented three retries is not established and must not be reused.

The final facts were `WIRE_REQUESTS:29`, `WIRE_CAPABILITY_PROBES:0`,
`WIRE_CREDENTIALED_REFUSALS:0`, `HEADER_EFFECTS:0`, `VAULT_EFFECTS:0`,
`BUDGET:0/0/0`, no accepted head, and `PUBLIC_PREVIEW_LIVE_CANDIDATE_FAILED`.
The budget and snapshot mode remain uninitialized presentation values, not
observations of the remote quota or an authenticated session.

Next: enforce the nine-forwarded-request limit across all pages/transports
and prove it against an always-successful paginated stub under isolation.
No further live read is part of that repair. The public target has already
proved too large for this bounded course; an attended successor must name a
small target or explicitly bind a different request budget before another
read. macOS selection remains unavailable until a candidate proof passes.

## Enforced cap and successful small-target read — 2026-09-04

The counting transport now reserves one of nine shared request slots before
forwarding, including successful pagination and replacement transports. The
atomic compare/exchange leaves the observed count at nine when a tenth
request is refused. The isolated self-check exercises both a typed failure
followed by retry and nine successful pages followed by two refused attempts
through distinct wrappers. Increasing the cap to ten made
`application_anonymous_public_preview_live_candidate_wiring` fail on
`PUBLIC_PREVIEW_WIRE_SINGLE_ATTEMPT:false`; restoring nine passed.
This is a live-candidate bound, not a new production paging limit.

The owner explicitly selected the smaller existing disposable public target:
"Use MikeMarcin/BPR-Test#1". One read of that target through the restored
candidate passed in 4,104 ms. Each of the nine operations returned HTTP 200
on page 1: repository, pull request, files, commits, reviews, review comments,
issue comments, checks, and search. The terminal facts were:

```text
PUBLIC_PATH_OK
PUBLIC_PREVIEW_WIRE_REQUESTS:9
PUBLIC_PREVIEW_WIRE_CAPABILITY_PROBES:0
PUBLIC_PREVIEW_WIRE_CREDENTIALED_REFUSALS:0
PUBLIC_PREVIEW_WIRE_SINGLE_ATTEMPT:true
PUBLIC_PREVIEW_HEADER_EFFECTS:0
PUBLIC_PREVIEW_VAULT_EFFECTS:0
PUBLIC_PREVIEW_BUDGET:60/46/1788568328
PUBLIC_PREVIEW_SNAPSHOT_MODE:public_preview
PUBLIC_PREVIEW_CANDIDATE:MikeMarcin/BPR-Test#1 17990ba0c2fb50338141f35c0717f0f99639c7ba
PUBLIC_PREVIEW_LIVE_CANDIDATE_OK
```

The 14-selector matrix recorded above passed 13/14 in 170.98 seconds; its
sole failure was the pre-existing manifest conflict repaired by [[BPR-0557]].
The repaired manifest then passed directly; all other inputs are unchanged
and their passing evidence is retained, including both composition contracts
and the real public fixture path. The touched C++ formatting and diff checks
pass. The source manifests were recomputed after inspection, and the
code-simplifier pass found no further change required. This implementation
commit records the source used for the successful live read.

Next: run the current-source isolated macOS course with selection unavailable,
flip only macOS, and run its enabled tests-disabled Release course. No further
public-service read is needed for that work. Transient logs under
`/tmp/bpr-0316-*` were consumed and are removed after this record.

## macOS enablement and selected artifact — 2026-09-04

The unavailable candidate at `9fd5cf1e` passed the current native course:

```sh
python3 scripts/verify_rfc0017_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/bpr0316-macos-20260904 --expected-preview-selection unavailable --fresh
```

Result: `RFC0017_PLATFORM_COURSE_OK`, 19 selectors, stage unavailable, zero
activation requests, 168 seconds. The intervening policy-record commits changed
no product source. This slice changes only macOS's shipped capability to enabled;
Linux and Windows remain unavailable.

The first enabled course found a real probe defect: the plain non-fixture
`--public-preview-probe --ghes-host ghe.example.invalid` activated GitHub.com
instead of the requested enterprise profile. Its marker incorrectly reported
enabled with zero requests. The probe now carries the requested enterprise
selection through activation and judges policy refusal only when composed;
unavailable platforms retain the unavailable verdict. The independent CTest
expectations name macOS enabled, macOS GHES refused, and other platforms
unavailable. The historical unavailable selector now passes (1/1, 0.53 s).

The complete enabled course then passed with the same command and
`--expected-preview-selection enabled`, using an incremental rebuild of the
isolated trees after the correction. No unchanged clean build was repeated.
Result: `RFC0017_PLATFORM_COURSE_OK`, 19/19 selectors, stage enabled, zero
activation requests, 47 seconds. The tests-disabled Release artifact at
`build/bpr0316-macos-20260904/release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`
also passed the GHES refusal, fixture/test-seam absence, composed-host inventory,
runtime/dependency, and platform-surface gates within that command. macOS was
26.6.2 arm64 with Qt 6.10.3. This proves the selected Mac surface, not native
Windows or Linux enablement. No further live-service read was performed.

Final selected checks: `ctest --test-dir build/bpr0316-macos-20260904/debug
-R '^(application_review_round_trip_composition_contract|application_composition_contract|test_network_isolation_manifest)$'
--output-on-failure` passed 3/3 in 166.78 s. Scoped strict tidy on `main.cpp`
and `github_application.cpp` passed, as did clang-format, `git diff --check`,
`verify_documentation.py`, and all 54 documentation contract tests. Both source
pins were recomputed after inspecting the complete diff.

The isolated build root is transferred directly to BPR-0307's simplifier
verification consumer, then must be removed. The temporary BPR-0316 logs and
BPR-0541 response files are removed after their facts are recorded. No separate
worktree was created. This commit contains the macOS flip and enabled evidence.
