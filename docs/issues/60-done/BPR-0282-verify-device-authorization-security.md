---
schema: "implement-rfc.card.v1"
id: "BPR-0282"
title: "Verify device-authorization security and public behavior"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0281","BPR-0450"]
---

# Verify device-authorization security and public behavior

## Required final state

- Focused core, host, adapter, native HTTPS, Qt, composition, tests-disabled
  Release, sanitizer, runtime, dependency, and boundary matrices pass locally.
- Hostile courses prove interval/backoff/total bounds, cancellation/shutdown,
  malformed/oversized bodies, off-authority addresses, both endpoint redirect
  and certificate failures, browser failure, and zero-request unavailability.
- For a selected GitHub App, controlled-clock/native-HTTPS/vault courses prove
  refresh before expiry, atomic access/refresh rotation, stale/canceled refresh
  rejection, refresh-material cleanup, and typed recoverable re-enrollment on
  refresh failure.
- Source, binary, fixture, log, settings, SQLite, process, and environment scans
  find no client secret, access/refresh token, device code, or user code.
- A registered-contract public course proves one vault enrollment and no
  settings/SQLite copy without contacting or mutating an undesignated target.
- Target-native evidence covers every platform on which device flow is claimed
  available. Untested platforms remain explicitly unavailable or retain an
  open platform-owned card; macOS, cross-compilation, mocks, and hosted
  workflows cannot prove Windows or Linux behavior.
- The complete macOS target-native course runs against production selection
  still unavailable and proves the owner-attended implementation without
  treating the private, owner-only verification app as a distributable client.
  Failure of any row leaves this card nonterminal.
- BPR-0318 owns the later distributable identity, one-time macOS capability-
  selection flip, and clean enabled rerun. BPR-0285 separately reruns the exact
  idempotent final-head platform course after simplification, documentation,
  and every required card are terminal.

## Inherited from done BPR-0450 — 2026-08-25

The refresh wire contract and its production adapter are in place, and the
selector this card's ledger row names — `github_device_authorization_refresh_contract`
— is registered and green with eight loopback scenarios. Two facts this card
must not restate more strongly than they are:

- The renewal wire shapes are `documented_not_recorded`, filed in
  `tests/fixtures/github/device-authorization/documented-v1/` deliberately
  outside BPR-0276's digest-bound recorded tree. No recorded-GitHub claim is
  admissible for refresh, and `capture_rfc0016_device_contracts.py` remains
  structurally unable to emit one.
- The production `device_authorization_transport_port` implementation
  (`github_device_authorization_transport`) ships here rather than in
  [[BPR-0281]], because a partial port could not exist. BPR-0281 keeps the
  composition wiring, the timer adapter, and per-platform selection.

## Verified and closed — 2026-08-25

### Two guards, and what they are for

`scripts/verify_rfc0016_test_inventory.py` exists because a matrix that runs is
not a matrix that covers: `ctest -R` over a regular expression passes just as
happily when a selector was never registered as when it passed. It requires
each of the sixteen evidence selectors exactly once, requires their commands to
still carry the arguments that make them the right suite, anchors on whether
the graph built Qt and the loopback fixture so `core-only` cannot pass
vacuously, and cross-checks the ledger by **compiling that document's own `-R`
patterns** rather than grepping for literal names — grepping would report a
missing selector for every one the ledger collapsed into an alternation, which
is a checker that fails on correct input. It is registered as
`rfc0016_test_inventory` and green in `dev`, in `core-only`, and against the
tests-disabled Release artifact.

Its artifact scan looks for testing seams, both fixture roots, the captured
user code, and token- and client-identifier-shaped material. It never prints a
matched value: reporting a secret to prove a secret leaked puts it in the log,
the CI output, and the evidence file. The service endpoints are deliberately
*not* forbidden — the host owns them and a shipped binary that can run the flow
must contain them, so forbidding them would be a check that fails on correct
code. It carries a positive control instead: an artifact containing no
device-flow route fails, because an absence scan against the wrong file passes
trivially. That control fired on the first draft, which looked for the joined
URL the linker never keeps.

`scripts/verify_rfc0016_platform_course.py` runs one platform's whole course
against isolated fresh Debug and Release trees. It refuses to run for a
platform it is not on, runs the inventory **before** any test, then checks the
artifact: this platform's credential store present and the other two absent, no
embedded browser engine, no client secret, and no BPR-0276 private verification
identity — read out of the committed registration record rather than carried as
a copy. `--expected-device-selection` is stated rather than discovered, because
a course that asks the binary what it selects and then asserts that it selected
that passes on every build; that is what lets the same script run before and
after a one-time enablement flip.

### The row that had no evidence, and the vacuous claim underneath it

The card asks for a registered-contract course proving one vault enrollment and
no settings or SQLite copy. Nothing in the planned selector set could observe
it: all three planned application selectors run against a build with no
registered identity, where nothing enrolls and the claim is **vacuously true**.
That is the RFC's *first* acceptance criterion, so leaving it vacuous was not an
option.

`application_device_authorization_registered_course` composes the recorded
BPR-0276 grant — replayed through the real `parse_device_code_response` and
`parse_device_token_response`, so only the socket is faked and
`native_https_device_authorization_fixture` proves the socket separately —
against the real host, the real enrollment boundary, and the real vault, then
reads every file under the application data root looking for the enrolled
token. `DEVICE_FLOW_STAGE:authorized`, `DEVICE_FLOW_ENROLLMENTS:1`,
`DEVICE_FLOW_VAULT_STORES:1`, `DEVICE_FLOW_DURABLE_COPIES:0`,
`DEVICE_FLOW_TRANSPORT_SUBMISSIONS:2`, `DEVICE_FLOW_ENROLLED_OK`. Adding it
required updating the ledger and [[BPR-0285]]'s anchor atomically, and the
inventory guard refused the selector until the ledger named it.

The tests-disabled equivalent still needs a distributable identity. It stays
[[BPR-0318]]'s to enable and BPR-0285's to replay.

### Two defects the probes found in the evidence itself

A vault store count **cannot** answer "exactly one enrollment". A deliberate
double-enroll probe still reported one store, because the GitHub host collapses
a repeated enrollment for the same profile before the vault sees a second
operation. That collapse is correct, and it is exactly why the count had to
move to the enrollment boundary — the place the criterion is actually about.
The probe fires now.

The counter that made that possible shipped into Release. `nm` found six of its
symbols in the tests-disabled artifact: it had gone into composition ungated
while the recorded transport beside it was gated correctly from the start. It
is behind `BPR_BUILD_TESTING` now, Release is clean, and both seam names joined
the inventory's forbidden list so a regression fails the scan rather than
waiting for someone to run `nm` by hand.

### Evidence

Fresh macOS course from a removed build root:
`RFC0016_PLATFORM_COURSE_OK`, `RFC0016_COURSE_SELECTORS:16`,
`RFC0016_DEVICE_STAGE:unavailable`, `RFC0016_DEVICE_REQUESTS:0`. Sanitizer
matrix 4/4 under AddressSanitizer and UndefinedBehaviorSanitizer — an earlier
2/4 was two unbuilt binaries rather than two failures, which reading the output
rather than the exit code caught. Full `application_*` suite green; repository
secret scan clean, with the only `ghu_`/`ghr_` hits being prefix patterns
inside other secret scanners. Format, `check_boundaries`,
`verify_documentation.py`, `git diff --check`, and `app-release` all clean. Two
composition digest pins drifted by design and are re-pinned here.

Falsification probes, each reverted: the summary parse (twice — CTest omits its
failure clause on a clean run, and my first regex required it); claiming
`enabled` when unavailable; claiming a foreign platform; an artifact carrying
the private verification identity; an artifact whose platform vault is not this
host's; a token-shaped string; a client identifier; the recorded user code; the
wrong artifact entirely; the dev binary's fixture seam; a token written under
the data root; a refused grant; and a double enrollment.

### What this card does not claim

Linux and Windows remain unproven here and keep [[BPR-0310]] and [[BPR-0311]];
macOS cannot speak for them. Every platform is still
`device_flow_unavailable`, so the enabled-selection course has never run
anywhere — [[BPR-0318]] owns the identity and the first flip. RFC-BPR-0002
closeout ([[BPR-0035]]/[[BPR-0036]]) is untouched by this card.
