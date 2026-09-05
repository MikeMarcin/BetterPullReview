---
id: BPR-0306
title: Verify anonymous preview security and public behavior
status: Done
rfc: RFC-BPR-0017
depends_on: [BPR-0305]
---

# Verify anonymous preview security and public behavior

## Required final state

- Core, provider/read-contract, native HTTPS, installed Git, SQLite migration/
  restart, host, Qt, public composition, tests-disabled Release, sanitizer,
  runtime, dependency, and boundary matrices pass from clean state.
- Positive canaries prove zero Authorization header, vault lookup, credential
  enumeration/enrollment, mutation, GraphQL/viewer request, GHES request, and
  private-content disclosure.
- Target-native macOS evidence covers the production native-HTTPS path and a
  real rendered public-preview candidate course while production selection is
  still unavailable; offscreen evidence is layout-only.
- This card extends `scripts/verify_rfc0017_test_inventory.py` to the complete
  RFC selector set and creates the exact isolated
  `scripts/verify_rfc0017_sanitizer_course.py` and
  `scripts/verify_rfc0017_platform_course.py` interfaces seeded by the RFC
  verification ledger. Each script fails on missing, extra, or duplicate
  required selectors before execution.
- BPR-0314/BPR-0315 own named Linux/Windows unavailable-path proofs, and
  BPR-0316 owns macOS enablement after this card is terminal.

## Part one landed — 2026-09-02

Integrated at `cf3ab49c` while the card sits in backlog behind
[[BPR-0305]]'s second half: the two script interfaces the ledger seeds now
exist with their hermetic contracts, and the sanitizer course has run on
the integrator host. The macOS platform course runs once
`application_anonymous_public_preview_public_path` is registered.

**`scripts/verify_rfc0017_platform_course.py`** takes the ledger's exact
interface — `--platform`, `--dev-preset`, `--release-preset`,
`--build-root`, `--expected-preview-selection unavailable|enabled`,
`--fresh` — and reuses RFC-BPR-0016's build, launch-environment, runtime,
dependency, symbol, and composed-host steps by import rather than by copy.
Its selector set is derived, not listed twice: one `ctest -N` listing of
the dev tree, the inventory's `selector_expected` over it to find the
selectors that graph owes (on Windows the two wire-block selectors are owed
zero and are neither required nor expected), plus two course-only
selectors owed on every platform, `application_anonymous_public_preview_public_path`
and `composed_application_hosts`. The inventory is invoked with exactly the
owed names as `--required`; the registration audit refuses
`course-selector-missing`, `-duplicate`, and `-extra` (an RFC-0017 family
name registered but not owed), and `course-exception-stale` for a reasoned
exception that became a course selector. After the selectors it runs the
release-absence scan and the production composed-host audit against the
shipped artifact, the runtime and dependency gates under the launch
environment, the embedded-engine surface check, then the shipped probe:
`--self-test --public-preview-probe` must print `COMPOSED:false`,
`STAGE:unavailable`, `REQUESTS:0`, `HEADER_EFFECTS:0`, `VAULT_EFFECTS:0`,
and `PUBLIC_PREVIEW_UNAVAILABLE_OK` for `unavailable`, and the same probe
with `--ghes-host` must refuse with zero requests. Markers: `RFC0017_PLATFORM`,
`RFC0017_SELECTION`, `RFC0017_COURSE_SELECTORS`, `RFC0017_PREVIEW_STAGE`,
`RFC0017_PREVIEW_REQUESTS`, `RFC0017_RELEASE_ARTIFACT`,
`RFC0017_ELAPSED_SECONDS`, `RFC0017_PLATFORM_COURSE_OK`.

**`scripts/verify_rfc0017_sanitizer_course.py`** takes `--build-root` and
`--fresh`, configures an isolated ASan/UBSan Debug tree with
`BPR_BUILD_QT=OFF` exactly as RFC-BPR-0016's hand-run precedent did, derives
the selectors that Qt-free graph owes from the same inventory rule,
refuses missing, duplicate, extra, or empty sets, runs exactly them, and
fails on any `ERROR: AddressSanitizer` or `runtime error:` before reading
CTest's exit status.

**Recorded for [[BPR-0316]].** The composition's probe prints
`PUBLIC_PREVIEW_ENABLED_OK` only under the tests-enabled enabled course,
whose verdict requires at least one read; a flipped shipped selection
activates with zero requests and would take the `UNAVAILABLE_FAILED`
branch. The course's `enabled` expectation is activation with zero
requests, so the macOS flip owes either a probe-verdict change in
`src/app/main.cpp` or a course change, decided there.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The two contracts and their siblings, the inventory, and the manifest gate | `ctest --preset dev -R '^(rfc0017_platform_course_launch_contract\|rfc0017_sanitizer_course_contract\|rfc0016_platform_course_launch_contract\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|test_network_isolation_manifest)$' --output-on-failure` | 6/6 passed | this commit |
| The platform course's own refusal table | `python3 tests/scripts/rfc0017_platform_course_tests.py` | **47 tests, OK** | this commit |
| The sanitizer course's own refusal table | `python3 tests/scripts/rfc0017_sanitizer_course_tests.py` | **21 tests, OK** | this commit |
| The sanitizer course on the integrator host | `python3 scripts/verify_rfc0017_sanitizer_course.py --build-root build/rfc0017-sanitize --fresh` | exit 0 in 26 s: `RFC0017_SANITIZER_SELECTORS:4` (`core_anonymous_public_read`, `application_anonymous_read_host`, `application_anonymous_diff_authority`, `sqlite_anonymous_epoch_authority`), `RFC0017_SANITIZER_COURSE_OK`; `-fsanitize=address,undefined` confirmed in the tree's cache | this commit |
| The platform course dry run against the dev tree | the registration audit alone | 15 owed, 17 course selectors; the sole refusal `course-selector-missing:macos:application_anonymous_public_preview_public_path`, which BPR-0305 part two supplies | this commit |

## Lane and close — 2026-09-02

[[BPR-0305]] is terminal, so this card is claimed and closed by the
integrator from backlog in one step: its two scripts landed under part one,
and the remaining item — the macOS platform course on fresh trees — has now
run on the integrator host.

**The macOS course.** `python3 scripts/verify_rfc0017_platform_course.py
--platform macos --dev-preset dev --release-preset app-release --build-root
build/rfc0017-macos-course --expected-preview-selection unavailable
--fresh` on a build root created fresh: `RFC0017_PLATFORM:macos`,
`RFC0017_SELECTION:unavailable`, `RFC0017_COURSE_SELECTORS:17`,
`RFC0017_PREVIEW_STAGE:unavailable`, `RFC0017_PREVIEW_REQUESTS:0`,
`RFC0017_ELAPSED_SECONDS:154`, `RFC0017_PLATFORM_COURSE_OK`. Inside it:
the inventory required exactly the selectors this graph owes; every course
selector was registered once and ran green; the release-absence scan and
the production composed-host audit passed against the shipped artifact;
`verify_binary_dependencies.py` and `verify_runtime_surface.py` passed under
the launch environment the inventory declares; no embedded browser engine
is in the image; the shipped probe answered `COMPOSED:false`,
`STAGE:unavailable`, `REQUESTS:0`, `HEADER_EFFECTS:0`, `VAULT_EFFECTS:0`,
`PUBLIC_PREVIEW_UNAVAILABLE_OK`, and the GHES probe refused with zero
requests. The artifact: `build/rfc0017-macos-course/release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview`, 10,608,944 bytes, sha256 `3147dd83dffa50e10ca883e28ad58be065136c969074460330ae176aed46fe86`.

**What the canaries prove, and where.** Zero `Authorization` on the wire —
`native_https_anonymous_read_fixture`, the three anonymous smart scenarios in
`native_git_smart_transport_fixture`, and the fixture role that refuses any
such header under `application_anonymous_public_preview_public_path`; zero
vault lookup, enrollment, or credential enumeration — the counters in
`application_anonymous_read_host` and `application_anonymous_diff_authority`
and the `VAULT_EFFECTS:0` marker; mutation, GraphQL, and viewer requests
refused before construction — `core_anonymous_public_read`,
`native_https_anonymous_read_contract`, `github_anonymous_read_contract`,
and `WRITE_REFUSALS:2` on the composed course; GHES with zero requests —
the factory refusal, the host refusal, and
`application_anonymous_public_preview_ghes_refusal`; private-content
non-disclosure — the existence-neutral `permission` mapping proven in
`github_paged_read` against the promoted 404 body.

**Target-native macOS evidence, and its limit.** The course above is
target-native for the shipped artifact's selection, runtime, dependencies,
symbols, and probe, and the fixture-enabled composed course renders the
signed-out pull request offscreen. A real rendered candidate against the
live service with the production native-HTTPS path is a network step that
needs designated authority; it is [[BPR-0316]]'s native candidate proof
before the macOS flip, with the owner's authorization, not something this
card performs unattended. The Linux and Windows unavailable-path courses
are [[BPR-0314]]'s and [[BPR-0315]]'s on their stations.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The macOS platform course on fresh trees, selection unavailable | the command above | exit 0 in 154 s; the markers above | this commit |
| The sanitizer course | `python3 scripts/verify_rfc0017_sanitizer_course.py --build-root build/rfc0017-sanitize --fresh` | recorded under *Part one landed*: 4 selectors, `RFC0017_SANITIZER_COURSE_OK`; the built test binary links the AddressSanitizer runtime | `b0b0b377` |
| The two contracts and their siblings with the inventory owning the public path | `ctest --preset dev -R '^(rfc0017_platform_course_launch_contract\|rfc0017_sanitizer_course_contract\|rfc0017_test_inventory\|rfc0017_test_inventory_contract\|test_network_isolation_manifest)$'` | 5/5 passed; the platform course contract 47 tests OK | this commit |
| The tree's documentation and issue-graph contracts hold | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |

## Correction — 2026-09-03

[[BPR-0315]]'s station found `rfc0017_platform_course_launch_contract` red
on Windows: the inventory's `selector_expected` reads the real
`sys.platform`, and the contract's macOS and Linux cases had no inverse of
the `as_windows` helper, so on a Windows host they inherited the host's
answer and the wire selectors read as extras. The course itself was never
wrong — it refuses a platform it is not on — only the contract that proves
the audit. The module now pins `sys.platform` to the wire block in
`setUpModule` and restores it after, so every case is judged from inside
the block on any host; the two new BPR-0316 rows are allocated in the
inventory and the manifest at the same time.
