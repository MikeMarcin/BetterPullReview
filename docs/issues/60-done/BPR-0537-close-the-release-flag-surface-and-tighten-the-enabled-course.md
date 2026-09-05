---
schema: "implement-rfc.card.v1"
id: "BPR-0537"
title: "Close the Release flag surface and tighten the enabled course"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0515"]
---

# Close the Release flag surface and tighten the enabled course

## Why

Three residuals the independent review of `28fa0741` ([[BPR-0285]],
2026-09-03) named and the integrator adjudicated as work rather than
acceptance:

- `--device-authorization-registered-course` is parsed unconditionally in
  `src/app/main.cpp`; in a Release it suppresses the probe's cancel, so
  `--self-test --device-authorization-probe --device-authorization-registered-course`
  polls GitHub until the run budget expires. It must be gated like
  `--device-authorization-browser-probe`.
- RFC-BPR-0016's inventory forbids none of `--network-denied`,
  `--device-authorization-browser-absent`, `--device-authorization-panel-absent`,
  `--device-authorization-registered-course`, `--public-preview-*`, or
  `--test-application-data-root`; only the RFC-0017 inventory forbids the
  first, and the platform course runs only the RFC-0016 scan. All are
  structurally absent today; the scan must say so.
- The platform course's `enabled` expectation accepts any stage but
  `unavailable`, so an offline station passes without proving the flow
  reached GitHub; and `system_browser_opener::open`'s address validation
  has no unit test, its only evidence being the three owner observations.

## Required final state

- Every tests-enabled flag is parsed only under `BPR_BUILD_TESTING`, and the
  RFC-0016 inventory's forbidden set names every such flag string; the
  positive control still fires on the tests-enabled binary.
- The platform course's `enabled` expectation requires `cancelled` — the
  stage only a minted device code reaches — and the replay row records the
  one bounded request that implies, as accepted traffic.
- A unit test drives `system_browser_opener`'s validation: an absolute
  `https` address with a host and no userinfo is the only shape it
  forwards, and nothing else reaches the launch service.

## Lane — 2026-09-03

Claimed straight into `30-in-progress/` by the integrator under the
transition exception, its one dependency terminal. Split as the board's
rule allows: the script half — the RFC-0016 inventory's forbidden set and
the platform course's `cancelled` expectation with their contracts — is the
integrator's and lands first; the flag gating in `main.cpp` and the opener
validation test follow [[BPR-0535]] so the two slices do not edit the same
composition file at once.

## Part one landed — 2026-09-03

The script half, at `a31aee2d` and this commit. `scripts/verify_rfc0016_test_inventory.py`
now forbids by string the ten tests-enabled flags that are already absent
from a Release — `--network-denied`, the browser and panel `-absent` and
`-probe` flags, the four `--public-preview-*` flags, `--test-application-data-root`,
and `--github-fixture` — proven both ways on the integrator host: the
release image at the closeout head still passes (`RFC0016_TEST_INVENTORY_OK`)
and the tests-enabled image fails on eleven strings. The two course flags a
Release still parses, `--device-authorization-registered-course` and
`--device-authorization-panel-course`, were confirmed present in the
release image by `strings` and join the list when the second half gates
them.

`scripts/verify_rfc0016_platform_course.py`'s `enabled` expectation now
requires `DEVICE_FLOW_STAGE:cancelled` — the stage only a minted device code
reaches before the run budget cancels it — and keeps `DEVICE_FLOW_REQUESTS:0`
as the refusing-transport negative control, refusing `failed` and
`unavailable` by name (`device-selection-not-reached:<stage>`). The
contract gained four cases for it. Every enabled course this RFC has
recorded — macOS, Linux, and Windows, before and after BPR-0515 — landed on
`cancelled`, so no recorded run changes verdict; an offline station now
cannot pass the enabled course, which is the point.

## Part two landed — 2026-09-03

The flag half, at `7e8fbb9d` and this commit. `--device-authorization-registered-course`
and `--device-authorization-panel-course` now take the browser probe's
shape in `src/app/main.cpp`: parsed under `BPR_BUILD_TESTING`, `false`
otherwise. Nothing else moved; both fed only the capability's `Registered`
and `PanelCourse` fields and the tests-only refusal, and the fixture
option was already inside the gate. A fresh `app-release` image carries
neither string (`strings -a`: 0 and 0, against 1 each in the dev image),
so the two joined the RFC-0016 inventory's forbidden set and the comment
that promised it is gone.

`system_browser_opener` splits its strict parse into
`validated_browser_address(std::string_view) -> std::optional<QUrl>`: the
object it validates is the object `open` forwards, so no second parse can
disagree with the first. Under `BPR_BUILD_TESTING` only, `open` finishes
through a function-pointer slot that `detail::substitute_browser_launch`
swaps and restores; a Release compiles the direct call and, checked with
`nm -C`, carries neither symbol. The class stays `final` and the
composed-host constructor anchor is untouched. The new selector
`application_device_authorization_browser_address` proves the accepted
address (`https://github.com/login/device`) launches exactly once with the
same string, and nine refused shapes — `http://`, scheme-less, `user@`,
`user:secret@`, an empty host, empty, `javascript:`, `file:`, a space in
the host — reach no launch at all. Dropping the userinfo refusal fails it
with six failures, restored byte-exact.

The inventory allocates the selector outside the graph-independent set
(it links Qt) and the ledger's inventory row names it; only the production
manifest pin drifted on the slice branch, and both pins were recomputed on
the merged tree (`full `d2ce570fb1e0…``, `manifest `686c74c55a36…``).

| Claim | Command | Result | Commit |
| --- | --- | --- | --- |
| The slice's graph builds clean | `cmake --build --preset dev` in the slice worktree, then on the merged tree | exit 0 both | this commit |
| Every device-authorization row including the new opener selector, the composed-host audit, the inventory, the manifest gate, and both composition contracts | `ctest --preset dev -R '^(application_device_authorization_(host\|refresh\|ghes_refusal\|unavailable\|release_contract\|public_path\|registered_course\|browser_handoff\|browser_absent\|panel_course\|panel_absent\|browser_address)\|composed_application_hosts\|rfc0016_test_inventory\|test_network_isolation_manifest\|application_review_round_trip_composition_contract\|application_composition_contract)$' --output-on-failure` on the merged tree | 17/17 passed in 185 s | this commit |
| The Release carries neither course flag and passes the widened scan | `cmake --preset app-release && cmake --build --preset app-release`, then `strings -a` and `python3 scripts/verify_rfc0016_test_inventory.py --release-binary <image> --repository-root .` | 0 and 0 occurrences; `RFC0016_TEST_INVENTORY_OK` (worker, slice tree) and 0 and 0 occurrences, `RFC0016_TEST_INVENTORY_OK` (merged tree) | this commit |
| The refusals are load-bearing | Worker dropped `\|\| !address.userInfo().isEmpty()`, rebuilt, ran the selector; restored | `failures=6` including `a refused address must reach no launch`; restored, passes | 7e8fbb9d |
| The architecture boundaries hold | `cmake --build build/dev --target check_boundaries` | exit 0 | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` over the four touched files | exit 0, no output | 7e8fbb9d |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |

Untested here: the Linux and Windows release images (the macOS image is the
one scanned), which the RFC-0016 final-head replays cover by running the
platform course's own inventory scan on each station.
