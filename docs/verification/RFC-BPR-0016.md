# RFC-BPR-0016 Verification Ledger

RFC base: `b969554fb45d838b2a4ea11811363bd7e158e47e`

Current RFC status: `Deferred`

Requested order: RFC-BPR-0016, then RFC-BPR-0017, then remaining unattended
macOS-ready work.

No row in this ledger is verified by planning, a linked component, documented
GitHub behavior, a packet without an accepted response, or a historical run.
Final results must be replayed against the immutable closeout head.

## Eligibility ledger

| Gate | State | Owner | Evidence required |
|---|---|---|---|
| Available-target implementation exception | Validated | BPR-0275 | Exact seven-fact response permits implementation/hermetic verification after application registration and real contracts; BPR-0035/BPR-0036 and final closeout remain mandatory |
| Registered private verification application identity | Pending | BPR-0276 | Digest-bound owner authority plus the later plain private/owner-only amendment are recorded; selected application record, public client identifier, exact permissions, and no shipped client secret remain to be proven |
| Distributable production application identity | Pending | BPR-0318 | A separate public-installation identity, exact production client and permissions, designated read-only course, and no shipped client secret are required before any production platform is enabled; plain deferral keeps every platform unavailable and this RFC open |
| Resolved application kind | Validated | BPR-0275 | Accepted GitHub App decision with exact installation, lifetime, refresh, permissions, and rejected OAuth-App rationale |
| Real sanitized wire contracts | Pending | BPR-0276 | Actual selected-service success, pending, one bounded early-poll `slow_down`, denied, expired, and disabled capture; separately labeled synthetic malformed/truncated/duplicate/oversized hostile fixtures; secret-free evidence |
| RFC-BPR-0002 Auto-Verified | Pending | BPR-0035/BPR-0036 | Direct real Windows x64 evidence and terminal RFC-BPR-0002 closeout; macOS, mocks, cross-compilation, and hosted workflows do not substitute |

The accepted
[`rfc0016-prerequisite-exception-v001`](reviews/rfc0016-prerequisite-exception-v001/operator-packet.md)
packet can authorize available-target implementation only. It cannot verify or
remove the fourth final-closeout gate. The independent
[`rfc0016-github-app-registration-v001`](reviews/rfc0016-github-app-registration-v001/operator-packet.md)
packet authorizes only the exact registration and bounded capture it names.
Both byte-preserved responses have adjacent validation records committed by
BPR-0275. The immutable packet files retain their decision-time `pending`
labels; the records and this ledger are the canonical outcomes. Neither
accepted authority proves a registered identity or captured wire contract.
The later plain amendment
[`rfc0016-private-owner-only-2026-08-13`](reviews/rfc0016-github-app-registration-v001/operator-amendment.md)
narrows the verification identity to private/owner-only. It does not widen any
permission or capture authority, and it does not establish the any-account
production identity needed for distribution or RFC closeout.

The originally authorized early capture poll was consumed: the service returned
`slow_down`, but the interrupted exploratory course retained no admissible
complete fixture. The operator later gave plain authority for one replacement.
That replacement was durably spent before traffic and GitHub returned its
documented `slow_down` body, including the new interval, but the fail-closed
three-field parser rejected the response and retained no raw body. The original
and replacement authority/spent records remain immutable provenance. One final
recovery poll may run only after the parser and sanitizer admit the exact
documented interval field, focused adversarial verification passes, and the
operator gives a new plain, narrowly scoped direction. The recovery uses a
distinct pre-traffic spent marker; no further retry is admitted.

The attended run completed real expiry, denial, a fresh token success with all
ten exact read-only route probes and restored user authorization, plus the
disabled-setting response with a verified disable/restore transition. Those
roots bind the pre-repair runner and therefore remain intermediate evidence
only. After the parser repair, all five courses must be recaptured under one
runner digest; mixed-runner assembly remains forbidden. The gate stays Pending
until deterministic five-course assembly, the exact attended-review export,
repository recording, and complete validation all succeed.

## Permission inventory

The current shipped operation catalog supports this proposed minimum GitHub
App registration, subject to exact real-token positive and negative proof:

| Authority | Proposed permission | Current operation |
|---|---|---|
| Repository metadata | Metadata read-only | Repository identity and metadata GET |
| Git object access | Contents read-only | Native smart-HTTP discovery and `git-upload-pack` |
| Check state | Checks read-only | Commit check-runs GET |
| Pull-request observation/publication | Pull requests read-and-write | PR/files/commits/reviews/comments/GraphQL reads and the sole review-comment POST |

No Issues, Actions, Workflows, Commit statuses, Administration, organization,
enterprise, or account permission and no OAuth scope is justified by the
current catalog. Any new route or wider real-token requirement reopens the
attended permission decision.

## Claim matrix

| State | RFC acceptance claim | Owning card | Final direct surface |
|---|---|---|---|
| [ ] | GitHub.com device grant enrolls through the existing vault with no settings or SQLite copy. | BPR-0279, BPR-0281, BPR-0282 | Tests-disabled public composition against recorded contracts plus vault/settings/SQLite inspection |
| [ ] | Core, Qt, and projections expose no token, refresh token, secret device code, client identifier, or device/token service endpoint; the host display projection carries only user code, validated verification URI, and expiry. | BPR-0277, BPR-0280, BPR-0282 | Compile-time boundary/type/symbol inventory with forbidden-field negative controls and exact allowed-display fields |
| [ ] | Pending, denied, expired, rate-limited, and disabled outcomes are typed and retain no transient material or partial profile. | BPR-0278, BPR-0279, BPR-0282 | Real-contract adapter replay plus host cleanup cuts |
| [ ] | Polling honors interval, slow-down, total bound, UI cancellation, and shutdown cleanup. | BPR-0279, BPR-0282 | Controlled-clock/manual-executor host matrix |
| [ ] | Certificate, unknown-authority redirect, and query redirect fail closed on both endpoints before forwarding. | BPR-0278, BPR-0282 | Native HTTPS hostile loopback matrix for device and token routes |
| [ ] | Off-authority verification address is refused with zero browser calls. | BPR-0280, BPR-0282 | Host browser-capability authority cut and Qt projection course |
| [ ] | Browser handoff failure leaves the flow live and later authorization can complete elsewhere. | BPR-0279, BPR-0280, BPR-0282 | Failing-opener application/Qt course followed by authorized terminal |
| [ ] | Manual token enrollment remains functional for every profile. | BPR-0280, BPR-0281, BPR-0282 | Manual-before/device/manual-after public composition courses including GHES |
| [ ] | A selected GitHub App refreshes before expiry and atomically rotates access/refresh material under existing vault authority. | BPR-0279, BPR-0282 | Controlled clock, native refresh wire, vault rotation/restart, stale/canceled result, cleanup, and typed re-enrollment courses |
| [ ] | Unregistered and GHES profiles return `device_flow_unavailable` with zero requests. | BPR-0281, BPR-0282 | Tests-disabled capability-selection counters and hostile profile cases |
| [ ] | Build and verification evidence is local or target-native and contains no secret/transient material. | BPR-0282, BPR-0285 | Clean matrices, Release/runtime/dependency/evidence scans, and no-Actions inventory |

## Verification matrix

Exact selectors are allocated, not claimed to exist before implementation.
BPR-0282 must register and directly run the final names from a clean build.

| Layer | Required direct evidence |
|---|---|
| Core | Replay/property matrix for state, legality, request/revision identity, stale/duplicate observations, terminals, cancellation |
| Application | Controlled clock and executor for interval, slow-down, total bound, shutdown, erasure, refresh, and exactly-one vault handoff |
| GitHub adapter | Exact form requests, bounded parsers, every recorded response, malformed/oversized handling, GraphQL permission probes |
| Native HTTPS | Both endpoints under trust/proxy/redirect/certificate/cancellation/limit hostile courses |
| Qt | Acquisition choice, manual fallback, focus/accessibility, copy/open/cancel, off-authority, failing opener, three-scale rendering |
| Composition | Tests-enabled cut wires and tests-disabled Release selection; registered GitHub.com positive and unregistered/GHES zero-request negatives |
| Security | Source/binary/type/fixture/log/settings/SQLite/process/environment forbidden-material scans with positive canaries |
| Build/runtime | Clean `dev`, `core-only`, sanitizers, tests-disabled Release, format, strict tidy, dependency, runtime, docs, and diff gates |
| Supported platforms | Target-native device-flow evidence for every platform where capability is enabled; otherwise exact unavailable selection and a named open platform card |

### Capability selection by platform

| Platform | Initial production selection after BPR-0281 | Enabling evidence |
|---|---|---|
| macOS | `device_flow_unavailable`, zero requests | BPR-0282 native/security proof plus BPR-0318 distributable identity and enablement |
| Linux | `device_flow_unavailable`, zero requests | BPR-0318 identity plus BPR-0310 target-native Linux closeout |
| Windows x64 | `device_flow_unavailable`, zero requests | BPR-0318 identity, BPR-0036, and BPR-0311 target-native Windows closeout |
| GHES on every platform | `device_flow_unavailable`, zero requests | Future instance-specific registration decision outside RFC-BPR-0016 |

## Final command ledger

These selectors are planned names, not passing tests. BPR-0282 must register
them exactly or update this ledger and BPR-0285 in the same implementation
commit. Ordinary and final-head result/evidence/commit cells remain pending
until closeout. The three one-time platform-enablement rows instead record their
owning BPR-0318/BPR-0310/BPR-0311 commits as provenance; only the idempotent
immutable final-head platform replay is the closeout result.

| Claim set | Exact command or attended interaction | Proof condition | Result | Durable evidence | Commit |
|---|---|---|---|---|---|
| Eligibility exception | Quote the exact statement in `reviews/rfc0016-prerequisite-exception-v001/operator-packet.md`; run `validate_operator_packet.py --validate-existing` against the adjacent record at its committed record identity | Subject digest and all seven facts match; no adjacent authority inferred | Passed: `OPERATOR_RESPONSE_VALID:rfc0016-prerequisite-exception-v001` | `reviews/rfc0016-prerequisite-exception-v001/operator-response.md` and adjacent `validation-record.json` | Authority `e36c2b4aff66f59fe17d3cb1349e13071543bd53`; record `7f74b9f404d858991ee0a96421e7da3829bcd891` |
| Registration authority | Quote the exact statement in `reviews/rfc0016-github-app-registration-v001/operator-packet.md`; run `validate_operator_packet.py --validate-existing` against the adjacent record at its committed record identity | Exact GitHub App owner/configuration/permissions/capture boundary; no mutation, release, distribution, GHES, or secret-bearing evidence inferred | Passed: `OPERATOR_RESPONSE_VALID:rfc0016-github-app-registration-v001` | `reviews/rfc0016-github-app-registration-v001/operator-response.md` and adjacent `validation-record.json` | Authority `e36c2b4aff66f59fe17d3cb1349e13071543bd53`; record `7f74b9f404d858991ee0a96421e7da3829bcd891` |
| Distributable identity authority | Record the ordinary proceed/defer choice in BPR-0318. If proceeding, present the exact statement in `reviews/rfc0016-production-app-registration-v001/operator-packet.md`; after its record commit run `record_commit="$(git rev-parse HEAD)" && python3 .agents/skills/bpr-attended-review/scripts/validate_operator_packet.py --repository-root . --output docs/verification/reviews/rfc0016-production-app-registration-v001/validation-record.json --validate-existing --record-commit "$record_commit"` | Plain direction alone chooses whether to proceed; the exact external registration and route-bounded traffic authority is independently accepted and replay-valid, with no mutation, release, distribution, GHES, or secret-bearing evidence inferred | Pending | BPR-0318 card plus adjacent production packet response/claims/validation files | Pending |
| Capture tooling and evidence admission | `python3 tests/scripts/rfc0016_device_capture_tests.py`; `python3 tests/scripts/rfc0016_device_live_runner_tests.py`; isolated `rfc0016_device_capture_security` and `rfc0016_device_live_runner_security`; independent capability-critical review | Offline sanitizer has no network/write path; attended runner is route-closed and secret-free; complete admission binds exact fixture, five course trees, one repaired runner, setting transition, original and recovery early-poll authority/spend chains, deterministic assembly, and accepted human export; fabricated provenance and Python bool/int aliases fail closed | Focused repair gates passed: 38/38 sanitizer, 40/40 runner, 2/2 isolated CTest; final complete-evidence admission and independent closeout review remain Pending | Initial tooling commits `3b6d470`, `100f578`, `910176c`; parser/recovery repair `69acab3`; `docs/verification/fixture-catalog.md` | `69acab3` for repaired tooling; final evidence Pending |
| Registered identity and real capture | Perform BPR-0276's exact bounded attended course under the accepted registration authority; approval for denial also covers the immediate fresh token-success/read-only restoration course | Exact configured public client; real success/pending/one early-poll `slow_down`/denied/expired/disabled; post-denial user authorization restored; synthetic hostile bodies labeled; no secrets | Pre-repair intermediate roots passed expiry, denial, post-denial success/read-only restoration, and disabled/restore. The authorized replacement `slow_down` response was rejected by the old parser because its documented interval was not admitted. One final recovery authority, a homogeneous repaired-runner five-course recapture, assembly, human verdict, and durable validation remain Pending | Immutable original/replacement authority-spend provenance and owner-private transient course roots; not yet repository closeout evidence | Pending |
| Focused semantic/host/wire/Qt/product matrix | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev && ctest --test-dir build/dev --output-on-failure -R '^(core_device_authorization|application_device_authorization_host|github_device_authorization_contract|native_https_device_authorization_(contract|fixture)|qt_device_authorization_scale_(100|125|200)|application_device_authorization_(public_path|unavailable|release_contract))$'` | Fresh build identity is recorded; inventory requires every exact selector before CTest; all pass and negative cuts fail if boundary, cleanup, authority, display, refresh, or selection is bypassed | Pending | This row plus fresh build identity, inventory output, final CTest summary, and exact fixture paths | Pending |
| Core-only matrix | `cmake --preset core-only --fresh && cmake --build --preset core-only --clean-first && ctest --preset core-only --output-on-failure` | Fresh core builds/tests without Qt/provider/effect dependencies | Pending | This row, fresh build identity, final counts, and failing-if-wrong core selectors | Pending |
| Tests-disabled Release | `cmake --preset app-release --fresh && cmake --build --preset app-release --clean-first` | Fresh registered production selection builds; testing factories/seams are absent | Pending | This row plus fresh build identity, Release executable digest/size, and selection contract | Pending |
| macOS unavailable native course | In BPR-0282, run `python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-unavailable --expected-device-selection unavailable --fresh` | Script creates isolated fresh Debug/Release trees, requires the selector inventory, and runs exact native HTTPS, Keychain, browser, Cocoa, zero-request production selection, runtime/dependency, and hygiene checks without mistaking the private verification identity for production authority | Pending | BPR-0282 bound host/toolchain/artifacts and unavailable-course output | Pending |
| Distributable identity and macOS one-time enablement | After accepted authority, run `python3 scripts/verify_rfc0016_production_identity.py --registration-observation docs/verification/reviews/rfc0016-production-app-registration-v001/registration-observation.json --course-evidence-root docs/verification/reviews/rfc0016-production-app-registration-v001/read-only-course --require-complete`; rerun the BPR-0282 unavailable course; flip only macOS selection; then rerun `python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-enablement --expected-device-selection enabled --fresh` | Route-closed identity admission proves the exact public-installation client, approved permissions, designated read-only course, and secret-free evidence; the clean enabled artifact then passes native HTTPS, Keychain, browser, Cocoa, public, Release, runtime/dependency, and hygiene evidence | Pending | BPR-0318 production packet, identity/course validation, one-time flip record, artifacts, and enabled-course output | Pending |
| Refresh lifecycle | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev && ctest --test-dir build/dev --output-on-failure -R '^(application_device_authorization_refresh|github_device_authorization_refresh_contract|application_github_credential_vault)$'` | Fresh build and inventory require all selectors; refresh precedes expiry; vault rotation is atomic; stale/canceled/failed refresh cannot leak or rotate and failure requests re-enrollment | Pending | This row plus fresh build identity and controlled-clock/native-wire/vault evidence | Pending |
| Boundary and hygiene | `cmake --build build/dev --target check_boundaries coding-format-check coding-tidy && python3 scripts/verify_documentation.py --root . && git diff --check` | Architecture, forbidden-field/secret scans, format, strict tidy, documentation, and diff gates pass | Pending | This row plus exact gate outputs and scanned artifact inventory | Pending |
| Target-native Linux one-time enablement | In BPR-0310, run `python3 scripts/verify_rfc0016_platform_course.py --platform linux --dev-preset dev --release-preset app-release --build-root build/rfc0016-linux-enablement --expected-device-selection unavailable --fresh`; after it passes, flip only Linux selection in the same commit and rerun with `--expected-device-selection enabled --fresh` | Inventory-validated isolated course runs exact native HTTPS, keyring, browser, Qt, public selection, ELF/runtime/dependency, Release, and hygiene checks before and after the flip | Pending | BPR-0310 bound host/toolchain/artifact/matrix and flip record | Pending |
| Target-native Windows x64 one-time enablement | In BPR-0311 on real Windows 10/11 x64, run `py -3 scripts/verify_rfc0016_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0016-windows-enablement --expected-device-selection unavailable --fresh`; after it passes, flip only Windows selection in the same commit and rerun with `--expected-device-selection enabled --fresh` | Inventory-validated isolated course runs exact native HTTPS, Credential Manager, browser, Qt, public selection, PE/runtime/dependency, tests-disabled Release, and hygiene checks before and after the flip | Pending | BPR-0036/BPR-0311 bound Windows host/toolchain/artifact/matrix and flip record | Pending |
| Immutable final-head platform replay | After BPR-0318, on each enabled target run `scripts/verify_rfc0016_platform_course.py` with that row's interpreter, platform, presets, a new `build/rfc0016-<platform>-closeout` root, `--expected-device-selection enabled --fresh`, and no selection mutation | The already-enabled immutable head uses the recorded distributable identity and passes isolated fresh Debug/Release selector inventory, native vault/browser/transport, public selection, runtime/dependency, and hygiene checks; this is replayable and idempotent | Pending | BPR-0285 per-platform bound host/toolchain/artifact/final-course records | Pending |
| Final complete local matrix | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev && ctest --preset dev --output-on-failure` | Fresh inventory requires every RFC selector and the complete configured macOS matrix passes at immutable closeout head | Pending | This row plus fresh build identity, final count/duration, and commit | Pending |
| Capability-critical closeout | Independent `rfc-critical-reviewer` review of immutable final head, artifacts, packets, commands, and claims | All five required review sections are present; Blocking findings and Verification failures contain no unresolved item after the primary reproduces the review commands; high-risk residuals are explicitly adjudicated | Pending | BPR-0285 final five-section review record and primary reproduction record | Pending |

## Documentation impact ledger

| Surface | Owner | Required disposition |
|---|---|---|
| `docs/architecture/product-contract.md` | BPR-0284 | Acquisition choices, manual fallback, user-visible terminals, GHES rule |
| `docs/architecture/system-architecture.md` | BPR-0284 | Core/host/GitHub/Qt/browser/composition ownership and typed flow |
| `docs/architecture/github-host-implementation.md` | BPR-0284 | Selected permissions, exact endpoints, polling, refresh, vault join point |
| `docs/architecture/review-round-trip.md` | BPR-0284 | Credential acquisition no-impact or exact changed authority |
| `docs/security/foundation-security.md` | BPR-0284 | Public client, no secret, transient material, redirects, browser/listener prohibitions |
| `docs/development/build-and-test.md` | BPR-0284 | Registration selection, fixture provenance, local/Release/negative gates |
| Operator authentication guidance | BPR-0284 | Installation, device course, manual fallback, diagnostics, re-enrollment |
| Agent guidance | BPR-0284 | Update only if the completed work establishes a reusable stable rule |

## Delegation and execution record

- A read-only semantic lane inventoried the shipped REST, GraphQL, and native
  smart-HTTP operations and derived the proposed minimum permission set. It
  made no file edits.
- A separate read-only planning lane audits the issue graph and dependency
  semantics; a verification-surface lane audits existing reusable contracts and
  gaps. Neither owns implementation files.
- The runtime advertised ten child slots but admitted only two concurrent
  children. A third direct spawn and a descendant utility spawn both returned
  `agent thread limit reached`; a new lane started immediately when one of the
  two completed. The run therefore keeps both effective slots occupied and
  records serial fallback rather than claiming unavailable parallel evidence.
- The primary agent alone owns tracker transitions, attended responses,
  integration, commits, final verification, and RFC status.

## Closeout blockers

- Any eligibility row remains pending, or a packet is treated as approval
  without the exact accepted response.
- A client secret, token, refresh token, device/user code, endpoint, interval,
  or provider type crosses its allocated boundary or enters durable evidence.
- A widget/provider bypass, alternate credential store, partial unavailable
  mode, unbounded poll, redirect/certificate relaxation, or embedded/listening
  browser design exists.
- A fixture/helper/symbol substitutes for tests-disabled product selection.
- The private owner-only verification identity is embedded in or used to
  justify an enabled generally distributed production artifact.
- BPR-0035/BPR-0036, documentation, simplification, clean final replay, or
  independent capability-critical review remains open.
