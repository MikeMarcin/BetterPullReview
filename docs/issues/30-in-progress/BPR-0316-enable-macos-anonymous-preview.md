---
id: BPR-0316
title: Enable anonymous public preview on target-native macOS
status: In Progress
rfc: RFC-BPR-0017
depends_on: [BPR-0306]
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
