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
| Registered private verification application identity | Validated | BPR-0276 | Owner re-confirmed the live settings page against the exact expected registration on 2026-08-24 before any course ran; the admitted capture's registration record binds the private owner-only GitHub App, public client `Iv23lil08fH3933w9MkW`, exact permissions, and owner-only `MikeMarcin/BPR-Test` installation; no client secret exists in any artifact |
| Distributable production application identity | Validated | BPR-0318 | Recorded 2026-08-27, course-proven 2026-08-29: the separate any-account GitHub App with public client `Iv23liitiYN1WSSbag50`, device flow and expiring tokens on, the exact approved permission set, and no client secret; the owner's attended sitting completed the designated read-only course (`RFC0016_PRODUCTION_COURSE_OK`, ten probes) and the complete admission passes `RFC0016_PRODUCTION_IDENTITY_OK` under `--validate-existing` with the enabled macOS Release artifact selecting the recorded client |
| Resolved application kind | Validated | BPR-0275 | Accepted GitHub App decision with exact installation, lifetime, refresh, permissions, and rejected OAuth-App rationale |
| Real sanitized wire contracts | Validated | BPR-0276 | All five courses recaptured 2026-08-24 in one attended sitting under the single repaired runner digest `d00583ab…`; the authorized recovery early poll captured GitHub's `slow_down` with its bounded interval; fixture recorded at `tests/fixtures/github/device-authorization/v1/`, course evidence and attestation at `docs/verification/rfc0016-live-capture-v001/`; complete admission passes `RFC0016_DEVICE_CAPTURE_VALID:attended_human_accepted_capture` against the owner's accepted review `rfc0016-attended-live-capture-v001` |
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

### Owner device-login requirement reconciled with Deferred status — 2026-08-24

When the v006 designated-live lane reached its manual-token prompt, the owner
rejected manual PAT/token entry and reaffirmed that device login is the
required product interaction before live publication; the cancellation is
recorded at `49c89ca` and the consumed v006 authority is non-retryable. That
requirement re-prioritizes this RFC as the active program, and it composes
with the existing authority without changing any of it:

- The RFC front matter stays `Deferred`; that status describes closeout
  eligibility, not implementation permission. The validated available-target
  implementation exception (`rfc0016-prerequisite-exception-v001`) already
  authorizes implementation and hermetic verification once the first three
  eligibility gates complete, and the fourth gate (RFC-BPR-0002
  Auto-Verified via BPR-0035/BPR-0036) remains mandatory for closeout only.
- The requirement grants no new authority: no registration, no traffic, no
  attended-course substitute, and no change to the private/owner-only scope of
  `rfc0016-private-owner-only-2026-08-13`. In particular it does not convert
  BPR-0276's attended five-course recapture into unattended work — the device
  grant needs a person to enter the user code — and it does not touch the
  single authorized recovery early poll's spent-marker discipline.
- The resulting order is the existing dependency graph: BPR-0276's one
  attended session closes gates one and three; BPR-0277/BPR-0278, then
  BPR-0279, BPR-0280, BPR-0281, BPR-0282, and BPR-0318 follow it; BPR-0256
  may prepare a new reviewed packet exercising the device-login path only
  after BPR-0318 is Done, and v006 is never reused or regenerated as
  authority.

Reorientation evidence, 2026-08-24: both foundational authority records
replay `OPERATOR_RESPONSE_VALID` at their committed record identity
`7f74b9f4`, executed from a detached worktree at that commit because the
validator binds its own committed bytes.

### The attended recapture completed and was admitted — 2026-08-24

The owner sat the single attended session the same day. The recovery-poll
authority granted on 2026-08-20 was materialized into its durable state
record, validated against the runner's own checks before launch, and spent
through its distinct `O_EXCL` pre-traffic marker exactly once; GitHub returned
the documented `slow_down` body and the repaired parser admitted it. All five
courses — expiry, denial, token success with all ten read-only probes and
restored user authorization, the disabled-setting response inside a recorded
enabled→disabled→restored attended transition, and the recovery `slow_down` —
were captured under the one repaired runner digest and assembled
deterministically. The owner accepted the digest-bound review batch
`42fd8114a6c2bbeb0cbf6f54` over all 39 identity files with every downstream
claim declared false, and complete admission passes
`RFC0016_DEVICE_CAPTURE_VALID:attended_human_accepted_capture`. BPR-0276 is
Done; BPR-0277 and BPR-0278 are promoted. The third eligibility gate is
closed, so available-target implementation is now authorized under the
recorded exception; the distributable identity (BPR-0318) and RFC-BPR-0002
closeout gates remain open and unchanged.

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
| macOS | **Enabled 2026-08-29 by BPR-0318**: registered as `Iv23liitiYN1WSSbag50`, real flow starts and cancels cleanly | BPR-0282 native/security proof passed 2026-08-25; BPR-0318 identity admitted complete, one-time flip, enabled course `RFC0016_PLATFORM_COURSE_OK` with `RFC0016_DEVICE_STAGE:cancelled`, release-binary admission `RFC0016_PRODUCTION_IDENTITY_OK` |
| Linux | `device_flow_unavailable`, zero requests | BPR-0318 identity plus BPR-0310 target-native Linux closeout |
| Windows x64 | `device_flow_unavailable`, zero requests | BPR-0318 identity, BPR-0036, and BPR-0311 target-native Windows closeout |
| GHES on every platform | `device_flow_unavailable`, zero requests | Future instance-specific registration decision outside RFC-BPR-0016 |

## Final command ledger

These selectors are planned names, not passing tests. BPR-0282 must register
them exactly or update this ledger and BPR-0285 in the same implementation
commit. It did once: `application_device_authorization_registered_course`
joined the focused matrix because the planned set had no row on which "the
grant reaches the existing vault exactly once, and the token exists nowhere
else" could be observed. The three planned application selectors all run
against a build with no registered identity, where nothing enrolls and the
claim is vacuous; the registered course composes the recorded BPR-0276 grant
against the real host, enrollment, and vault, and then reads every file the
application wrote. `scripts/verify_rfc0016_test_inventory.py` refused the
addition until this row named it, which is what that guard is for. Ordinary and final-head result/evidence/commit cells remain pending
until closeout. The three one-time platform-enablement rows instead record their
owning BPR-0318/BPR-0310/BPR-0311 commits as provenance; only the idempotent
immutable final-head platform replay is the closeout result.

| Claim set | Exact command or attended interaction | Proof condition | Result | Durable evidence | Commit |
|---|---|---|---|---|---|
| Eligibility exception | Quote the exact statement in `reviews/rfc0016-prerequisite-exception-v001/operator-packet.md`; run `validate_operator_packet.py --validate-existing` against the adjacent record at its committed record identity | Subject digest and all seven facts match; no adjacent authority inferred | Passed: `OPERATOR_RESPONSE_VALID:rfc0016-prerequisite-exception-v001` | `reviews/rfc0016-prerequisite-exception-v001/operator-response.md` and adjacent `validation-record.json` | Authority `e36c2b4aff66f59fe17d3cb1349e13071543bd53`; record `7f74b9f404d858991ee0a96421e7da3829bcd891` |
| Registration authority | Quote the exact statement in `reviews/rfc0016-github-app-registration-v001/operator-packet.md`; run `validate_operator_packet.py --validate-existing` against the adjacent record at its committed record identity | Exact GitHub App owner/configuration/permissions/capture boundary; no mutation, release, distribution, GHES, or secret-bearing evidence inferred | Passed: `OPERATOR_RESPONSE_VALID:rfc0016-github-app-registration-v001` | `reviews/rfc0016-github-app-registration-v001/operator-response.md` and adjacent `validation-record.json` | Authority `e36c2b4aff66f59fe17d3cb1349e13071543bd53`; record `7f74b9f404d858991ee0a96421e7da3829bcd891` |
| Distributable identity authority | Record the ordinary proceed/defer choice in BPR-0318. If proceeding, present the exact statement in `reviews/rfc0016-production-app-registration-v001/operator-packet.md`; after its record commit run `record_commit="$(git rev-parse HEAD)" && python3 .agents/skills/bpr-attended-review/scripts/validate_operator_packet.py --repository-root . --output docs/verification/reviews/rfc0016-production-app-registration-v001/validation-record.json --validate-existing --record-commit "$record_commit"` | Plain direction alone chooses whether to proceed; the exact external registration and route-bounded traffic authority is independently accepted and replay-valid, with no mutation, release, distribution, GHES, or secret-bearing evidence inferred | Passed 2026-08-27: the owner answered proceed in the attended session, approved the packet with the exact statement bound to decision subject `18b867d3a05321cedcd4570227c732459779bebff23728d354de9411c4e5370b`, and the generated record validates and replays `OPERATOR_RESPONSE_VALID:rfc0016-production-app-registration-v001`. The registration itself and the designated read-only course run next under this recorded authority | BPR-0318 card plus adjacent production packet response/claims/validation files | Record `c82e51c2c933887632fb0bb35db876375a151de9` |
| Capture tooling and evidence admission | `python3 tests/scripts/rfc0016_device_capture_tests.py`; `python3 tests/scripts/rfc0016_device_live_runner_tests.py`; isolated `rfc0016_device_capture_security` and `rfc0016_device_live_runner_security`; independent capability-critical review | Offline sanitizer has no network/write path; attended runner is route-closed and secret-free; complete admission binds exact fixture, five course trees, one repaired runner, setting transition, original and recovery early-poll authority/spend chains, deterministic assembly, and accepted human export; fabricated provenance and Python bool/int aliases fail closed | Focused repair gates passed: 39/39 sanitizer, 40/40 runner, 2/2 isolated CTest; complete-evidence admission passed 2026-08-24: `RFC0016_DEVICE_CAPTURE_VALID:attended_human_accepted_capture` over the recorded fixture, course evidence, authority chains, and accepted review `rfc0016-attended-live-capture-v001`; independent capability-critical closeout review remains BPR-0285's | Initial tooling commits `3b6d470`, `100f578`, `910176c`; parser/recovery repair `69acab3`; final interval-binding repair `386176f`; `docs/verification/fixture-catalog.md`; `docs/verification/rfc0016-live-capture-v001/` | `386176f` for repaired tooling; admission recorded in the BPR-0276 closing commit |
| Registered identity and real capture | Perform BPR-0276's exact bounded attended course under the accepted registration authority; approval for denial also covers the immediate fresh token-success/read-only restoration course | Exact configured public client; real success/pending/one early-poll `slow_down`/denied/expired/disabled; post-denial user authorization restored; synthetic hostile bodies labeled; no secrets | Passed 2026-08-24. The homogeneous repaired-runner recapture completed all five courses in one attended sitting: real expiry, denial, token success with the ten read-only probes and restored user authorization, the disabled response inside a recorded attended settings transition, and the recovery `slow_down` whose bounded interval the repaired parser admitted. `RFC0016_ATTENDED_CAPTURE_BUNDLE_OK` assembly, the owner's accepted digest-bound review, and complete admission all pass | Repository-recorded fixture at `tests/fixtures/github/device-authorization/v1/`, course evidence and authority/spend chains at `docs/verification/rfc0016-live-capture-v001/`, accepted review at `docs/verification/reviews/rfc0016-attended-live-capture-v001/` | Recorded in the BPR-0276 closing commit |
| Focused semantic/host/wire/Qt/product matrix | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev && ctest --test-dir build/dev --output-on-failure -R '^(core_device_authorization|application_device_authorization_host|github_device_authorization_contract|native_https_device_authorization_(contract|fixture)|qt_device_authorization_scale_(100|125|200)|application_device_authorization_(public_path|unavailable|release_contract|registered_course))$'` | Fresh build identity is recorded; inventory requires every exact selector before CTest; all pass and negative cuts fail if boundary, cleanup, authority, display, refresh, or selection is bypassed | Pending | This row plus fresh build identity, inventory output, final CTest summary, and exact fixture paths | Pending |
| Core-only matrix | `cmake --preset core-only --fresh && cmake --build --preset core-only --clean-first && ctest --preset core-only --output-on-failure` | Fresh core builds/tests without Qt/provider/effect dependencies | Pending | This row, fresh build identity, final counts, and failing-if-wrong core selectors | Pending |
| Tests-disabled Release | `cmake --preset app-release --fresh && cmake --build --preset app-release --clean-first` | Fresh registered production selection builds; testing factories/seams are absent | Pending | This row plus fresh build identity, Release executable digest/size, and selection contract | Pending |
| macOS unavailable native course | In BPR-0282, run `python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-unavailable --expected-device-selection unavailable --fresh` | Script creates isolated fresh Debug/Release trees, requires the selector inventory, and runs exact native HTTPS, Keychain, browser, Cocoa, zero-request production selection, runtime/dependency, and hygiene checks without mistaking the private verification identity for production authority | Passed 2026-08-25 on a removed-and-rebuilt build root: `RFC0016_PLATFORM_COURSE_OK` with `RFC0016_COURSE_SELECTORS:15`, `RFC0016_DEVICE_STAGE:unavailable`, `RFC0016_DEVICE_REQUESTS:0`, 297 fresh Debug and 164 fresh Release objects. Four falsification probes each fired: claiming `enabled`, claiming a foreign platform, an artifact carrying BPR-0276's private verification identifier, and an artifact whose platform vault is not this host's. BPR-0285 still owns the idempotent final-head replay | `scripts/verify_rfc0016_platform_course.py` plus the BPR-0282 card's recorded course output | BPR-0282 closing commit |
| Distributable identity and macOS one-time enablement | Run isolated `rfc0016_production_identity_security` and `rfc0016_production_course_security` (BPR-0481 closed four review gaps in the first: the course must name the runner's digest, the probe table is recomputed by equality, the GraphQL document is pinned, and the course tree is bounded); after accepted authority, run `python3 scripts/verify_rfc0016_production_identity.py --registration-observation docs/verification/reviews/rfc0016-production-app-registration-v001/registration-observation.json --course-evidence-root docs/verification/reviews/rfc0016-production-app-registration-v001/read-only-course --require-complete`; rerun the BPR-0282 unavailable course; flip only macOS selection; then rerun `python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-enablement --expected-device-selection enabled --fresh` and the same verifier with `--validate-existing --release-binary <enabled artifact>` | Route-closed identity admission proves the exact public-installation client, approved permissions, designated read-only course, and secret-free evidence — noting that `route-probes.json` is a constant table returned only after all ten real probes passed, so its success fields are proven by the execution path rather than by the file, a BPR-0276 inheritance recorded on BPR-0480 for BPR-0285 to adjudicate; the clean enabled artifact then passes native HTTPS, Keychain, browser, Cocoa, public, Release, runtime/dependency, and hygiene evidence, and carries the recorded production client rather than the private verification one | Identity half passed 2026-08-29: the verifier exists and admits the committed authority, sanitized observation, and secret-free evidence — `RFC0016_PRODUCTION_IDENTITY_OK`, `RFC0016_PRODUCTION_CLIENT:Iv23liitiYN1WSSbag50`, `RFC0016_PRODUCTION_RECORD_COMMIT:c82e51c2c933887632fb0bb35db876375a151de9`, `RFC0016_PRODUCTION_COURSE:identity-only`, under `--validate-existing` as well, and the committed record replays through the checked-in attended-review validator rather than being believed. Mutation-swept whole rather than sampled: every refusal condition in both tools is neutralized in turn and the suite rerun, and the surviving conditions are all `isinstance`/`is_file`/`is_dir` shape guards whose removal changes which diagnostic prints and not whether the run refuses. The counts move with every added test and are recorded, dated, on the BPR-0318 and BPR-0481 cards rather than here, where they had already gone stale twice. `--require-complete` fails closed on the absent `read-only-course/` (proven against an absent root since the live course exists). Completed 2026-08-29: the owner's attended sitting produced the designated read-only course (`RFC0016_PRODUCTION_COURSE_OK`, ten probes, committed secret-free), the complete admission passed, the BPR-0282 unavailable course was rerun clean pre-flip (`RFC0016_PLATFORM_COURSE_OK`, 16 selectors, stage `unavailable`, zero requests), the one-time macOS flip landed with the real credential-free wire and probe cancel, and the fresh enabled course passed at the final head with a genuine live grant — `RFC0016_PLATFORM_COURSE_OK`, `RFC0016_DEVICE_STAGE:cancelled` — plus `--validate-existing --release-binary` admission `RFC0016_PRODUCTION_IDENTITY_OK` against that exact artifact. Three unregistered-era gates met their first registered artifact and were repaired on the way, each recorded on the BPR-0318 card: the release scan now permits exactly the recorded client, the wire header bound accepts Qt's merged-header join byte, and the credential-free binding reports truthfully in shipped builds | `scripts/verify_rfc0016_production_identity.py`, `scripts/run_rfc0016_production_course_attended.py`, their isolated suites, BPR-0318 production packet, identity/course validation under `docs/verification/reviews/rfc0016-production-app-registration-v001/`, one-time flip record, artifacts, and enabled-course output on the BPR-0318 card | Identity half in the BPR-0318 verifier commit; course evidence, flip, gate repairs, and enabled rerun in the BPR-0318 closing commits |
| Refresh lifecycle | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev && ctest --test-dir build/dev --output-on-failure -R '^(application_device_authorization_refresh|github_device_authorization_refresh_contract|application_github_credential_vault)$'` | Fresh build and inventory require all selectors; refresh precedes expiry; vault rotation is atomic; stale/canceled/failed refresh cannot leak or rotate and failure requests re-enrollment | Pending | This row plus fresh build identity and controlled-clock/native-wire/vault evidence | Pending |
| Evidence-selector inventory and shipped-artifact scan | `ctest --preset dev -R '^rfc0016_test_inventory$' --output-on-failure` and, at closeout against a tests-disabled Release build, `python3 scripts/verify_rfc0016_test_inventory.py --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview` | Every evidence selector is registered exactly once with the arguments that make it the right suite; the ledger's own `-R` patterns still name each one; a graph without Qt or the loopback fixture registers none of those rather than passing vacuously; the shipped artifact carries no testing seam, no fixture root, no captured user code, and no token- or client-identifier-shaped material, and a positive control fails an artifact carrying no device-flow route | Passed 2026-08-25: `RFC0016_TEST_INVENTORY_OK` in `dev`, in `core-only`, and against the tests-disabled Release artifact. Five falsification probes each fired: a token-shaped string, a client identifier, the recorded user code, the wrong artifact entirely, and the dev binary with its fixture seam | `scripts/verify_rfc0016_test_inventory.py`; registered as `rfc0016_test_inventory` | BPR-0282 closing commit |
| Sanitizer matrix | `cmake -S . -B build/rfc0016-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBPR_BUILD_QT=OFF -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"` then `ctest --test-dir build/rfc0016-sanitize --no-tests=error --output-on-failure -R '^(core_device_authorization\|application_device_authorization_(host\|refresh)\|application_github_credential_vault)$'` | The core policy, the host, the refresh lifecycle, and the vault run clean under AddressSanitizer and UndefinedBehaviorSanitizer | Passed 2026-08-25: 4/4. An earlier 2/4 was two unbuilt binaries, not two failures — `--no-tests=error` and reading the output caught it | BPR-0282 card's recorded sanitizer output | BPR-0282 closing commit |
| Registered-contract vault course | `ctest --preset dev --output-on-failure -R '^application_device_authorization_registered_course$'` | The recorded grant reaches `enroll_profile` through the composed application; the vault records exactly one store; no file under the application data root contains the enrolled token; the recorded transport received exactly the two submissions the grant needs | Passed 2026-08-25: `DEVICE_FLOW_STAGE:authorized`, `DEVICE_FLOW_VAULT_STORES:1`, `DEVICE_FLOW_DURABLE_COPIES:0`, `DEVICE_FLOW_TRANSPORT_SUBMISSIONS:2`, `DEVICE_FLOW_ENROLLED_OK`. A registered capability exists only in a tests-enabled build with the fixture, so the tests-disabled equivalent remains BPR-0318's and BPR-0285's | `tests/end_to_end/CMakeLists.txt` registration plus the BPR-0282 card | BPR-0282 closing commit |
| Boundary and hygiene | `cmake --build build/dev --target check_boundaries coding-format-check coding-tidy && python3 scripts/verify_documentation.py --root . && git diff --check` | Architecture, forbidden-field/secret scans, format, strict tidy, documentation, and diff gates pass | Pending | This row plus exact gate outputs and scanned artifact inventory | Pending |
| Target-native Linux one-time enablement | In BPR-0310, run `python3 scripts/verify_rfc0016_platform_course.py --platform linux --dev-preset dev --release-preset app-release --build-root build/rfc0016-linux-enablement --expected-device-selection unavailable --fresh`; after it passes, flip only Linux selection in the same commit and rerun with `--expected-device-selection enabled --fresh` | Inventory-validated isolated course runs exact native HTTPS, keyring, browser, Qt, public selection, ELF/runtime/dependency, Release, and hygiene checks before and after the flip | Verified 2026-09-02 on the JaegAW WSL2 station (`bpr-linux`, `linux-lane` `7cb32c62`, merged at `a76bec99`): with the `login` collection read unlocked before and after, all four `linux_secret_service*` fixtures pass; the pre-flip course exits 0 in 171 s (`RFC0016_DEVICE_STAGE:unavailable`, `RFC0016_DEVICE_REQUESTS:0`, artifact 10,991,504 bytes, sha256 `474cfb0d…b644ba`); the flip is one `#if` edit plus the dev-tree gate extended to Linux with a measured `failed` stage (namespace with loopback only, 0.15 s, three runs); the enabled course on a deleted-and-rebuilt root exits 0 in 242 s (`RFC0016_DEVICE_STAGE:cancelled`, `RFC0016_DEVICE_REQUESTS:0`, artifact 10,991,504 bytes, sha256 `d6e21290…4aca6f`); full `dev` matrix 223/223 at the flipped head | BPR-0310 bound host/toolchain/artifact/matrix and flip record | BPR-0310 terminal; the flip record and both course runs are on the card |
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
- A client secret, token, refresh token, or device/user code crosses its
  boundary or enters durable evidence; or a public client identifier, service
  endpoint, interval, or provider type crosses its allocated product boundary.
  The sanitized BPR-0276 evidence may retain only the approved public
  registration/endpoint identities and the bounded `slow_down` interval needed
  to prove the exact active-interval-plus-five transition.
- A widget/provider bypass, alternate credential store, partial unavailable
  mode, unbounded poll, redirect/certificate relaxation, or embedded/listening
  browser design exists.
- A fixture/helper/symbol substitutes for tests-disabled product selection.
- The private owner-only verification identity is embedded in or used to
  justify an enabled generally distributed production artifact.
- BPR-0035/BPR-0036, documentation, simplification, clean final replay, or
  independent capability-critical review remains open.
