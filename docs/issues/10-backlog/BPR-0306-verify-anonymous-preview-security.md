---
id: BPR-0306
title: Verify anonymous preview security and public behavior
status: Backlog
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
