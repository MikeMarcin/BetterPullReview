# RFC-BPR-0016 Verification Ledger

RFC base: `b969554fb45d838b2a4ea11811363bd7e158e47e`

Current RFC status: `Auto-Verified`

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
| RFC-BPR-0002 Auto-Verified | Verified — RFC-BPR-0002 closed Auto-Verified 2026-08-28 at `76d18e60` on the Windows x64 target-native matrix (BPR-0035 and BPR-0036 terminal); corrected 2026-09-03 under BPR-0285's review, V1 | BPR-0035/BPR-0036 | Direct real Windows x64 evidence and terminal RFC-BPR-0002 closeout; macOS, mocks, cross-compilation, and hosted workflows do not substitute |

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
| [x] | GitHub.com device grant enrolls through the existing vault with no settings or SQLite copy. | BPR-0279, BPR-0281, BPR-0282 | Tests-disabled public composition against recorded contracts plus vault/settings/SQLite inspection |
| [x] | Core, Qt, and projections expose no token, refresh token, secret device code, client identifier, or device/token service endpoint; the host display projection carries only user code, validated verification URI, and expiry. | BPR-0277, BPR-0280, BPR-0282 | Compile-time boundary/type/symbol inventory with forbidden-field negative controls and exact allowed-display fields |
| [x] | Pending, denied, expired, rate-limited, and disabled outcomes are typed and retain no transient material or partial profile. | BPR-0278, BPR-0279, BPR-0282 | Real-contract adapter replay plus host cleanup cuts |
| [x] | Polling honors interval, slow-down, total bound, UI cancellation, and shutdown cleanup. | BPR-0279, BPR-0282 | Controlled-clock/manual-executor host matrix |
| [x] | Certificate, unknown-authority redirect, and query redirect fail closed on both endpoints before forwarding. | BPR-0278, BPR-0282 | Native HTTPS hostile loopback matrix for device and token routes |
| [x] | Off-authority verification address is refused with zero browser calls. | BPR-0280, BPR-0282, BPR-0516 | Host browser-capability authority cut and Qt projection course; the suites bind the recording opener, and the composed system opener is proven by linkage, by the composed-host anchor, and by the owner's three observations on BPR-0516 (BPR-0285 review, V5) |
| [x] | Browser handoff failure leaves the flow live and later authorization can complete elsewhere. | BPR-0279, BPR-0280, BPR-0282, BPR-0516 | Failing-opener application/Qt course followed by authorized terminal, plus the composed handoff and construction-cut courses — all against the recording opener; the composed opener's own validation test is BPR-0537's (BPR-0285 review, V5) |
| [x] | Manual token enrollment remains functional for every profile. | BPR-0280, BPR-0281, BPR-0282 | Manual-before/device/manual-after public composition courses including GHES |
| [x] | A selected GitHub App refreshes before expiry and atomically rotates access/refresh material under existing vault authority. | BPR-0279, BPR-0282 | Controlled clock, native refresh wire, vault rotation/restart, stale/canceled result, cleanup, and typed re-enrollment courses |
| [x] | Unregistered and GHES profiles return `device_flow_unavailable` with zero requests. | BPR-0281, BPR-0282 | Tests-disabled capability-selection counters and hostile profile cases |
| [x] | Build and verification evidence is local or target-native and contains no secret/transient material. | BPR-0282, BPR-0285 | Clean matrices, Release/runtime/dependency/evidence scans, and no-Actions inventory |

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
| Composition | Tests-enabled cut wires and tests-disabled Release selection; registered GitHub.com positive and unregistered/GHES zero-request negatives; the system-browser opener composed, anchored in the composed-host audit, and cut out to prove `unavailable` (BPR-0516); the device panel mounted in the shipped window, anchored in the same audit, driven from its own accessible control, and cut out to prove the flow stays reachable with the marker false (BPR-0526) |
| Security | Source/binary/type/fixture/log/settings/SQLite/process/environment forbidden-material scans with positive canaries |
| Build/runtime | Clean `dev`, `core-only`, sanitizers, tests-disabled Release, format, strict tidy, dependency, runtime, docs, and diff gates |
| Supported platforms | Target-native device-flow evidence for every platform where capability is enabled; otherwise exact unavailable selection and a named open platform card |

### Capability selection by platform

| Platform | Initial production selection after BPR-0281 | Enabling evidence |
|---|---|---|
| macOS | **Enabled 2026-08-29 by BPR-0318**: registered as `Iv23liitiYN1WSSbag50`, real flow starts and cancels cleanly | BPR-0282 native/security proof passed 2026-08-25; BPR-0318 identity admitted complete, one-time flip, enabled course `RFC0016_PLATFORM_COURSE_OK` with `RFC0016_DEVICE_STAGE:cancelled`, release-binary admission `RFC0016_PRODUCTION_IDENTITY_OK` |
| Linux | Enabled since BPR-0310 (2026-09-02): the registered GitHub.com selection with the recorded distributable identity; corrected 2026-09-03 under BPR-0285's review, V2 | BPR-0318 identity plus BPR-0310 target-native Linux closeout |
| Windows x64 | Enabled since BPR-0311 (2026-09-02): the registered GitHub.com selection with the recorded distributable identity; corrected 2026-09-03 under BPR-0285's review, V2 | BPR-0318 identity, BPR-0036, and BPR-0311 target-native Windows closeout |
| GHES on every platform | `device_flow_unavailable`, zero requests — true in the composed product since BPR-0535 (2026-09-03): the capability names the profile it was composed for and the host refuses a foreign begin before any transport call; the review of `28fa0741` had found the mounted panel could begin the GitHub.com flow for a GHES profile (BPR-0285, B1) | Future instance-specific registration decision outside RFC-BPR-0016; BPR-0535 for the refusal, proven by `application_device_authorization_ghes_refusal` |

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
| Refresh lifecycle | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev && ctest --test-dir build/dev --output-on-failure -R '^(application_device_authorization_refresh\|application_device_authorization_refresh_course\|application_device_authorization_orphan_course\|github_device_authorization_refresh_contract\|application_github_credential_vault\|composed_application_hosts)$'` | Fresh build and inventory require all selectors; refresh precedes expiry; vault rotation is atomic; stale/canceled/failed refresh cannot leak or rotate and failure requests re-enrollment; and since BPR-0536 the lifecycle is composed: the session's vault authority is bound to the device host on acceptance, the refresh observation reaches the panel, cancel forgets an outstanding store, renewal is armed for the session that enrolled the grant in this run (no session survives a restart today — BPR-0541 persists the binding), and since BPR-0540 the refresh slot shares the base credential's erasure path — every exact delete of the credential (orderly shutdown, sign-out, a superseding enrollment) deletes the refresh slot under the same authority through the same port, issued from that delete's own terminal rather than beside it because a platform vault refuses two operations under one context (BPR-0545), counted into quiescence, with any error reported as a failed cleanup — except a rotation superseding the session it just renewed, where the erasure is held only while the incoming session may still name the same authority key and runs the moment it does not (BPR-0547, BPR-0549); and the resume answers only for a credential whose provenance is a device grant, never for slot presence; the composed-host audit requires the binding in both profiles; `application_device_authorization_refresh_course` rotates **twice** through the real graph over the fixture's refresh slot — two rather than one because a single rotation succeeds whether or not the supersession keeps its material, so only the second shows the chain surviving — and pins `DEVICE_FLOW_ROTATIONS:2`, `DEVICE_FLOW_REFRESH_STATE:scheduled`, `DEVICE_FLOW_VAULT_STORES:3`, `DEVICE_FLOW_REFRESH_STORES:3`, `DEVICE_FLOW_REFRESH_DELETES:1`, `DEVICE_FLOW_REFRESH_STORED_TOKEN:e2e8c030e3a107e9` (the second rotation's own material) and `DEVICE_FLOW_REFRESH_RETAINED:false` after its orderly shutdown; and `application_device_authorization_orphan_course` pre-seeds the slot, connects with a pasted credential, and pins `DEVICE_FLOW_REFRESH_RESUMES:0`, `DEVICE_FLOW_ROTATIONS:0`, `DEVICE_FLOW_VAULT_STORES:1`, and `DEVICE_FLOW_STORED_IS_PASTED:true` | Passed 2026-09-04 at the closeout head `0d19c990` on all three platforms: `application_device_authorization_refresh_course` prints `DEVICE_FLOW_ROTATIONS:2`, `REFRESH_STATE:scheduled`, `VAULT_STORES:3`, `REFRESH_STORES:3`, `REFRESH_DELETES:1`, `REFRESH_STORED_TOKEN:e2e8c030e3a107e9` (the second rotation's own material), `REFRESH_RETAINED:false`, `CLEANUP_FAILED:false`, `REFRESH_OK` — 7.90 s on Linux, 8.72 s on Windows, both far inside the row's 25 s bound and stable to within 230 ms across four heads. Two rotations rather than one because a single rotation succeeds whether or not the supersession keeps its material; only the second shows the chain surviving, and forcing the erasure back on turns the row red at rotation two; the refresh, vault, contract, GitHub host, and audit rows pass (31/31 in the closing reproduction); deleting the composition's `bind_refresh_authority` call turns the course red (`ROTATIONS:0`). The resume branch (`REFRESH_RESUMES`) is reachable today only through the orphaned-slot case the second review named (B1); BPR-0540 gates it on provenance and BPR-0541 makes it reachable by a persisted binding | This row, the BPR-0536 card's three parts with their mutations, and the composed course's markers | BPR-0536 closing commit |
| Evidence-selector inventory and shipped-artifact scan | `ctest --preset dev -R '^rfc0016_test_inventory$' --output-on-failure` and, at closeout against a tests-disabled Release build, `python3 scripts/verify_rfc0016_test_inventory.py --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview` | Every evidence selector is registered exactly once with the arguments that make it the right suite; the ledger's own `-R` patterns still name each one; a graph without Qt or the loopback fixture registers none of those rather than passing vacuously; the shipped artifact carries no testing seam, no fixture root, no captured user code, and no token- or client-identifier-shaped material, and a positive control fails an artifact carrying no device-flow route Since BPR-0516 the inventory also allocates `application_device_authorization_browser_handoff` and `application_device_authorization_browser_absent`, the composed system-browser opener and its construction cut, both required to be absent from the tests-disabled artifact. Since BPR-0526 it also allocates `application_device_authorization_panel_course` and `application_device_authorization_panel_absent`, the mounted sign-in panel and its cut; since BPR-0535 `application_device_authorization_ghes_refusal`, the GHES profile's refusal to begin a GitHub.com flow; and since BPR-0537 `application_device_authorization_browser_address`, the opener's validation observed without a launch, with the two course flags now in the forbidden set. | Passed 2026-08-25: `RFC0016_TEST_INVENTORY_OK` in `dev`, in `core-only`, and against the tests-disabled Release artifact. Five falsification probes each fired: a token-shaped string, a client identifier, the recorded user code, the wrong artifact entirely, and the dev binary with its fixture seam | `scripts/verify_rfc0016_test_inventory.py`; registered as `rfc0016_test_inventory` | BPR-0282 closing commit |
| Sanitizer matrix | `cmake -S . -B build/rfc0016-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBPR_BUILD_QT=OFF -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"` then `ctest --test-dir build/rfc0016-sanitize --no-tests=error --output-on-failure -R '^(core_device_authorization\|application_device_authorization_(host\|refresh)\|application_github_credential_vault)$'` | The core policy, the host, the refresh lifecycle, and the vault run clean under AddressSanitizer and UndefinedBehaviorSanitizer | Passed 2026-08-25: 4/4. An earlier 2/4 was two unbuilt binaries, not two failures — `--no-tests=error` and reading the output caught it | BPR-0282 card's recorded sanitizer output | BPR-0282 closing commit |
| Registered-contract vault course | `ctest --preset dev --output-on-failure -R '^application_device_authorization_registered_course$'` | The recorded grant reaches `enroll_profile` through the composed application and is the one store the vault records — proven by `DEVICE_FLOW_STORED_IS_GRANT:true` beside `DEVICE_FLOW_VAULT_STORED_TOKEN:<sha256 prefix>` of the stored bytes, pinned to the grant's recorded token, not by the count alone; no file under the application data root contains the enrolled token; the recorded transport received exactly the two submissions the grant needs | Passed 2026-08-25 as `DEVICE_FLOW_STAGE:authorized`, `DEVICE_FLOW_VAULT_STORES:1`, `DEVICE_FLOW_DURABLE_COPIES:0`, `DEVICE_FLOW_TRANSPORT_SUBMISSIONS:2`, `DEVICE_FLOW_ENROLLED_OK` — but until BPR-0539 (2026-09-03) that one store was the fixture's own connect credential and the grant's enrollment aborted on a delete the fixture refused; since BPR-0539 the course does not connect before the grant, the fixture deletes by authority, and the row passes with `DEVICE_FLOW_VAULT_STORED_TOKEN:f5505a4b89c07e21` and `DEVICE_FLOW_STORED_IS_GRANT:true` and fails when the grant's enrollment is skipped. A registered capability exists only in a tests-enabled build with the fixture, so the tests-disabled equivalent remains BPR-0318's and BPR-0285's | `tests/end_to_end/CMakeLists.txt` registration plus the BPR-0282 card | BPR-0282 closing commit |
| Boundary and hygiene | `cmake --build build/dev --target check_boundaries coding-format-check coding-tidy && python3 scripts/verify_documentation.py --root . && git diff --check` | Architecture, forbidden-field/secret scans, format, strict tidy, documentation, and diff gates pass | Pending | This row plus exact gate outputs and scanned artifact inventory | Pending |
| Target-native Linux one-time enablement | In BPR-0310, run `python3 scripts/verify_rfc0016_platform_course.py --platform linux --dev-preset dev --release-preset app-release --build-root build/rfc0016-linux-enablement --expected-device-selection unavailable --fresh`; after it passes, flip only Linux selection in the same commit and rerun with `--expected-device-selection enabled --fresh` | Inventory-validated isolated course runs exact native HTTPS, keyring, browser, Qt, public selection, ELF/runtime/dependency, Release, and hygiene checks before and after the flip | Verified 2026-09-02 on the JaegAW WSL2 station (`bpr-linux`, `linux-lane` `7cb32c62`, merged at `a76bec99`): with the `login` collection read unlocked before and after, all four `linux_secret_service*` fixtures pass; the pre-flip course exits 0 in 171 s (`RFC0016_DEVICE_STAGE:unavailable`, `RFC0016_DEVICE_REQUESTS:0`, artifact 10,991,504 bytes, sha256 `474cfb0d…b644ba`); the flip is one `#if` edit plus the dev-tree gate extended to Linux with a measured `failed` stage (namespace with loopback only, 0.15 s, three runs); the enabled course on a deleted-and-rebuilt root exits 0 in 242 s (`RFC0016_DEVICE_STAGE:cancelled`, `RFC0016_DEVICE_REQUESTS:0`, artifact 10,991,504 bytes, sha256 `d6e21290…4aca6f`); full `dev` matrix 223/223 at the flipped head | BPR-0310 bound host/toolchain/artifact/matrix and flip record | BPR-0310 terminal; the flip record and both course runs are on the card |
| Target-native Windows x64 one-time enablement | In BPR-0311 on real Windows 10/11 x64, run `py -3 scripts/verify_rfc0016_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0016-windows-enablement --expected-device-selection unavailable --fresh`; after it passes, flip only Windows selection in the same commit and rerun with `--expected-device-selection enabled --fresh` | Inventory-validated isolated course runs exact native HTTPS, Credential Manager, browser, Qt, public selection, PE/runtime/dependency, tests-disabled Release, and hygiene checks before and after the flip | Verified 2026-09-02 on the JaegAW native Windows station (`bpr-win`, `windows-lane` `4627b6e1`, merged at `6fd16043`) under the supported procedure (BPR-0493: Smart App Control off, Visual Studio developer environment, build root deleted whole) with the course's own launch environment (BPR-0511) and MSVC building the whole tree (BPR-0512): the pre-flip course at `6fa6c08e` exits 0 in 941 s (`RFC0016_COURSE_SELECTORS:14`, `RFC0016_DEVICE_STAGE:unavailable`, `RFC0016_DEVICE_REQUESTS:0`, artifact 7,543,296 bytes, sha256 `23be95d7…fa5f4f`); the flip is one `#if` edit plus the dev-tree gate's measured `cancelled` (the uncontained test reached GitHub once per run — BPR-0515 adjudicated that posture on 2026-09-03 with the owner's choice of a tests-enabled `--network-denied` override: both dev-tree device rows now expect `failed` with zero requests on every platform, and the Windows station measured zero outbound connections and no `github.com` resolution across 294 samples of two runs; the tests-disabled Release carries no trace of the flag); the enabled course on a deleted-and-rebuilt root exits 0 in 931 s (`RFC0016_DEVICE_STAGE:cancelled`, `RFC0016_DEVICE_REQUESTS:0`, artifact 7,545,856 bytes, sha256 `cb225667…b18cd0`), and against that artifact `RFC0016_PRODUCTION_IDENTITY_OK` (the recorded distributable client present, the private BPR-0276 client absent) and `RFC0016_TEST_INVENTORY_OK` | BPR-0036/BPR-0311 bound Windows host/toolchain/artifact/matrix and flip record | BPR-0311 terminal; the flip record and both course runs are on the card |
| Immutable final-head platform replay | After BPR-0318, on each enabled target run `scripts/verify_rfc0016_platform_course.py` with that row's interpreter, platform, presets, a new `build/rfc0016-<platform>-closeout` root, `--expected-device-selection enabled --fresh`, and no selection mutation | The already-enabled immutable head uses the recorded distributable identity and passes isolated fresh Debug/Release selector inventory, native vault/browser/transport, public selection, runtime/dependency, and hygiene checks; this is replayable and idempotent | Verified 2026-09-04 at `0d19c990` on all three platforms, each from a build root deleted whole and with no selection mutated: macOS on the integrator host (exit 0, 572 s), Linux on `bpr-linux` (288 s, login collection unlocked and recorded first), Windows x64 on `bpr-win` (1171 s). Each printed `RFC0016_PLATFORM_COURSE_OK` with `SELECTION:enabled`, `COURSE_SELECTORS:25` derived from the inventory, and `DEVICE_STAGE:cancelled` — the stage that is reachable only after a device code was minted, so each replay mints exactly one and cancels it inside the run budget, unattended; `DEVICE_REQUESTS:0` beside it is the unregistered refusing transport's counter, a negative control rather than a request count. Each artifact then admitted `RFC0016_PRODUCTION_IDENTITY_OK` for client `Iv23liitiYN1WSSbag50`, `RFC0016_TEST_INVENTORY_OK`, and `COMPOSED_HOSTS_OK: required=30 forbidden=7`. Artifacts: macOS 10,770,080 bytes sha256 `97e66f91…`; Linux 11,651,608 bytes sha256 `bbd42d85…`; Windows 7,815,680 bytes sha256 `16433dfa…` | The BPR-0285 card's three replay sections, with host, toolchain, and full marker blocks | BPR-0285 closing commit |
| Final complete local matrix | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0016_test_inventory.py --build-dir build/dev && ctest --preset dev --output-on-failure` | Fresh inventory requires every RFC selector and the complete configured macOS matrix passes at immutable closeout head | Pending | This row plus fresh build identity, final count/duration, and commit | Pending |
| Capability-critical closeout | Independent `rfc-critical-reviewer` review of immutable final head, artifacts, packets, commands, and claims | All five required review sections are present; Blocking findings and Verification failures contain no unresolved item after the primary reproduces the review commands; high-risk residuals are explicitly adjudicated | Pending | BPR-0285 final five-section review record and primary reproduction record | Pending |

## Documentation impact ledger

| Surface | Owner | Required disposition | Delivered 2026-09-02 |
|---|---|---|---|
| `docs/architecture/product-contract.md` | BPR-0284 | Acquisition choices, manual fallback, user-visible terminals, GHES rule | `## Credential acquisition`: the two acquisition paths into one vault, the manual path staying first-class, the seven reviewer-visible terminals, the failed handoff that is not one of them, and the GHES rule as a structural route refusal with its ledger row still open |
| `docs/architecture/system-architecture.md` | BPR-0284 | Core/host/GitHub/Qt/browser/composition ownership and typed flow | `## RFC-BPR-0016 device authorization allocation` plus a Feature-allocation row: the typed flow end to end, and per-region ownership for core, host, GitHub adapter, native HTTPS, Qt, the composition-owned browser opener, and identity/platform selection |
| `docs/architecture/github-host-implementation.md` | BPR-0284 | Selected permissions, exact endpoints, polling, refresh, vault join point | `### RFC-BPR-0016 device authorization`: the registered identity and its permission table, the two `/login` routes and their admission rule, the exact polling bounds, the refresh lifecycle and its second vault slot, the enrollment join point, and GHES/unregistered selection |
| `docs/architecture/review-round-trip.md` | BPR-0284 | Credential acquisition no-impact or exact changed authority | `## Credential acquisition has no impact here`: an evidence-backed no-impact region — publication authority, drafts, anchoring, safe targets, retries, and reconciliation never consult how enrollment happened |
| `docs/architecture/security-threat-model.md` | BPR-0284 | Authority boundaries, prohibited designs, fixture/production split, test network posture | `## RFC-BPR-0016 device authorization boundary`: what the device host may reach and what it structurally cannot, the closed presentation record, the twice-checked verification authority, the still-prohibited designs, the `BPR_BUILD_TESTING` browser seam, and the per-platform automated-test network posture |
| `docs/security/foundation-security.md` | BPR-0284 | Public client, no secret, transient material, redirects, browser/listener prohibitions | `## RFC-BPR-0016 device authorization boundary`: public client with no secret, transient material confined to the host, inherited redirect/certificate rules, the system-browser opener with no engine or listener, and why the service endpoints are deliberately not forbidden material |
| `docs/development/build-and-test.md` | BPR-0284 | Registration selection, fixture provenance, local/Release/negative gates | `### Platform selection and what each platform's gate expects`, `### Running a target-native course`, and `### What RFC-BPR-0016 does not deliver yet`, beside the existing guard sections: per-platform isolation and expectation, the uncontained Windows gate and BPR-0515, the two named Windows exclusions and BPR-0525, the Linux and Windows course commands, and the three open observations |
| Operator authentication guidance | BPR-0284 | Installation, device course, manual fallback, diagnostics, re-enrollment | `docs/development/build-and-test.md` `### Signing in with a device code`: the registered GitHub App and its any-account installation implication, the compiled permission set, manual fallback, terminals and diagnostics, refresh and re-enrollment, handoff failure, GHES, and the refusals not to work around; target-native procedure by reference to `docs/development/remote-stations.md` |
| Sign-in presentation | BPR-0284, BPR-0526 | The Qt surface a reviewer sees, and what composition actually mounts | `docs/ux/workbench-specification.md` `## Signing in with a device code`: the panel's fields, its four actions, command legality taken from the core's projection, handoff reporting, terminal clearing, and, since BPR-0526, where the workbench mounts it — the review header column under the session chrome, bound to the profile the host picker names — with the projection port fanned out to the self-test sink and the mounted panel together |
| Agent guidance | BPR-0284 | Update only if the completed work establishes a reusable stable rule | No change. This slice established no reusable cross-slice rule; every boundary it documents is RFC-BPR-0016's own and is already enforced by `VerifyArchitectureBoundaries.cmake`, `verify_composed_hosts.py`, and `verify_rfc0016_test_inventory.py` rather than by guidance |

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

## Simplification

BPR-0283's focused pass over the shipped device-authorization implementation.
Six behaviour-preserving removals, one behaviour *correction* that is the
point of the first of them, and the reasoning for what was deliberately left
alone. Nothing here collapses a security check, an erasure, a cancellation, or
a bounded-state rule into a helper; those are the parts a reader has to be
able to audit line by line, and they are untouched.

**Removed: Qt's second copy of command legality.** The core already decides
which of begin and cancel is legal and why the other is not, and publishes it
as `device_authorization_transition_output::Commands`. The host dropped that
field on the floor and `device_authorization_panel.cpp` worked the answer out
again from `Stage`, in a four-line `live()` predicate. The projection now
carries the core's `device_authorization_command_projection` values, the host
forwards them from the transition that produced them rather than recomputing
them, and the panel looks each command up and enables the button. `live()` is
deleted. An absent command is a disabled command, so a projection that lost
one fails closed rather than falling back to a rule of the panel's own.

This is where the two copies had already drifted: `cancelling` is neither
`requested` nor `awaiting_authorization`, so the panel offered "Get a code"
during a cancellation the core refuses with `cancellation_in_progress`. That
one stage is the whole observable difference, and it now matches the core.
Copying the code and opening a browser stay derived from what is on screen —
neither moves the flow, and the core has no opinion about either.

**Removed: the device host's reach into the base-token slot.** The host was
handed the whole `github_credential_vault_port` and used three of its seven
methods; `store`, `retrieve`, `delete_credential`, and `delete_refresh` were
reachable and unused. `github_credential_refresh_vault_port` now declares the
refresh slot alone — `store_refresh`, `retrieve_refresh`, `delete_refresh`,
`cancel` — and the full vault derives from it, so composition still hands one
platform object to both and no composition line changed. The device host takes
the narrow port. Reaching the base-token slot from it is a compile error
rather than a rule somebody has to keep.

**Removed: the full vault port's duplicated rule of five.** It declared its
own defaulted constructor and destructor and its own four deleted copy and
move operations. Deriving from the refresh port inherits every one of them —
a base with a deleted copy constructor gives a derived class a deleted copy
constructor — so the five declarations said nothing the type did not already
say.

**Removed: nine unreachable base-slot stubs across three test doubles.**
`unavailable_vault`, `slotted_vault`, and `lifecycle_vault` each implemented
`store`, `retrieve`, and `delete_credential` only to be constructible as a
full vault, and nothing ever called them; `lifecycle_vault::Access` was
written by one of them and read by no assertion. All three now implement the
refresh port. The subtraction is the proof: a renewal course that accidentally
stored an access token in the account slot used to be caught by nothing, and
now does not compile.

**Removed: the second spelling of the verification authority.**
`expected_verification_authority` in the GitHub adapter's contract parser and
`default_device_verification_authority` in the host's handoff were two
independent literals for one deliberate double check. The declaration moved to
`bpr/application/github_host_authority.hpp`, beside the other GitHub authority
facts, and the parser compares against it. Both checks stay — the parser
refuses an off-authority `verification_uri` at the wire so it never reaches a
host, and the host refuses it again against the authority its composition
handed it — but a registration that ever moved the device endpoints can no
longer leave half the product trusting the old authority.

**Removed: a comment that fails the architecture gate.**
`src/app/src/system_browser_opener.hpp` named the Windows shell launch API in
a doc comment listing what each platform's opener defers to.
`VerifyArchitectureBoundaries.cmake` forbids that name and cannot tell a
sentence from a call, so `check_boundaries` has been red at the repository
head since BPR-0516 added the file. The comment now describes the service
without spelling it, and says why. No code changed, and the gate stays strict;
relaxing the pattern instead would have widened a security rule to fit a
comment.

**Not removed: the browser-handoff comment.** BPR-0283's scout carded it
because it read "no browser capability is composed in this slice", which
stopped being true when BPR-0516 composed `bpr::app::system_browser_opener`.
BPR-0516 rewrote it in the same commit that made it stale. The current text
describes the composed opener, the tests-enabled recording seam, why that seam
is total rather than fixture-conditional, and the construction cut; it was
read against the composition it describes and left alone.
`github_application.cpp` is therefore unchanged by this pass, and neither of
its two source digest pins
in `tests/end_to_end/application_review_round_trip_composition_contract.cmake`
moved.

**Not removed: the four two-line completion-context visitors.** The host has
`context_of` for device-code and token completions and `vault_context_of` for
store and retrieve completions, each a `std::visit` over one variant. One
function template collapses all four and saves six lines, at the cost of
taking the context type out of the signature — which is exactly what a reader
checks when asking whether a vault context could ever be compared against a
flow context. The comparison is the staleness guard; keeping the types spelled
out is worth more than the six lines.

**Not removed: the disabled reason that travels unrendered.** `Commands`
carries the core's reason code as well as the flag, and the panel renders only
the flag. Forwarding the core's record whole is what keeps a host-authored
shape from appearing between the core and Qt; splitting it to carry exactly
what today's panel draws would put one there. Rendering the reason is a
presentational change, and this pass preserves behaviour.

**Not removed, and re-confirmed:** the per-poll `DeviceCode` copy is the
transport port's stated owned-material contract; `deliver()` republishing the
whole presentation is the atomic-publication rule the application host owes
Qt; the `RegisteredDeviceCourse` fixture override is compile-time gated on
`BPR_BUILD_TESTING` end to end and cannot reach the tests-disabled Release;
and `valid_github_authority_hostname`'s own `github.com` answers a different
question — which hostname the GitHub.com *server contract* names — so
binding it to the verification authority would make a GHES-only change to one
silently move the other.

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

### BPR-0541 owner policy decision — 2026-09-04

The owner selected Remember device sign-in in the complete
[validated verdict](reviews/device-session-retention-policy-v001/verdict-validation.json).
This authorizes the subsequent persistent-device-session implementation in
[[BPR-0541]]; it does not change the evidence or terminal status of the earlier
RFC closeout, and does not claim credential operations or implemented restart
behavior. Manual-token sessions remain session-only under the chosen policy.

### BPR-0541 approved retention implementation — 2026-09-04

The prior closeout's session-only and process-local statements above describe
its immutable source. The owner subsequently approved remembering device
sign-in. BPR-0541 implements private metadata persistence, exact startup
validation, deferred renewal, device-only retention, and visible Sign out.
Manual replacement remains session-only. An unresolved startup load refuses
further sign-in for that application lifetime; a fresh launch retries loading.

Focused storage and separate-process metadata tests passed; the integrator's
final host, renewal and composed restart replay passed 3/3 in 26.86 seconds.
The composed course uses a runtime-only fake vault across application lifetimes
and real SQLite reopen; it proves deferred resume/rotation, Sign out cleanup,
manual replacement, and preservation of the replacement public-preview session.
Independent review's three initial and two related follow-up counterexamples
were reproduced, repaired and narrowly rechecked clean. Dev/Release builds,
scoped strict tidy, format, composition pins, fixture exclusion, boundary and
documentation gates passed as recorded on the terminal BPR-0541 card.

This adds evidence for the approved policy without rewriting the historical
RFC closeout. Native-vault continuity across OS processes and native
Windows/Linux execution of this later change remain unclaimed.
