# RFC-BPR-0017 Verification Ledger

RFC base: `a45750f6bd84716240aa27a20f0734f33808ab8e`

Current RFC status: `Auto-Verified`

The eligibility gates and claim matrix are satisfied by the named focused
evidence. BPR-0309 records clean independent review of candidate `d9006237`
and the explicit platform and verification limits. BPR-0298 measured the
anonymous service, BPR-0299 decided to build on those measurements, and
BPR-0312 bound the four definitions that decision left owed. RFC-BPR-0016
remains the answer for anyone who signs in; this RFC is the credential-optional
profile beside it.

## Eligibility ledger

| Gate | State | Owner | Required evidence |
|---|---|---|---|
| Correct live-tree scoping | Verified — the corrected RFC records the missing REST review-comment read, the credential-mandatory transport, the viewer/account assumptions, and the normalizer gaps (BPR-0297 terminal) | BPR-0297 | RFC records missing REST comments, credential-mandatory transport, viewer/account assumptions, and normalizer gaps |
| Real anonymous-service contracts | Verified — all five captures and the derived rate case have recorded provenance; BPR-0298/BPR-0445 are terminal | BPR-0298, BPR-0445 | Sanitized executed probes for GraphQL, REST comments/paging, rate/304, 404, and exact GitHub.com shapes |
| Deliberate credential-free native HTTPS demonstration | Verified — `rfc0017_credential_free_native_https` under `BPR_NATIVE_HTTPS_TESTING` with Release absence (BPR-0313 terminal, `b9bd446`, `aa4a514`) | BPR-0313 | `BUILD_TESTING`-only non-product typed request with unchanged RFC-0002 mechanics, zero header/vault/non-loopback network effects, and Release symbol absence |
| Product build/no-build decision | Decided 2026-09-01: build, on the measured limits and capabilities, recorded on `run-boundary-decisions-v001` (BPR-0299 terminal) | BPR-0299 | Digest-bound measured limit/capability/UI decision |
| Positive build approval | Verified 2026-09-02: the owner's byte-exact affirmative verdict validated (`RFC0017_PUBLIC_PREVIEW_DECISION_RECORDED:build`, four definitions bound, eleven evidence files bound) and revalidates with nothing written (BPR-0312 terminal) | BPR-0312 | Exact affirmative response and validation record; rejection leaves this gate unresolved |
| RFC-BPR-0002 Auto-Verified | Verified — RFC-BPR-0002 closed Auto-Verified 2026-08-28 at `76d18e60` with the Windows x64 target-native matrix (BPR-0035 and BPR-0036 terminal) | BPR-0035/BPR-0036 | Direct real Windows x64 evidence and terminal RFC-0002 closeout |

## Claim matrix

| State | Claim | Owner | Durable final evidence |
|---|---|---|---|
| [x] | Existing provider/normalizer opens a public PR without a parallel path. | BPR-0302, BPR-0305, BPR-0306 | Tests-disabled recorded-contract composition |
| [x] | Credential-free public Git/cache/epoch authority renders exact selected-file diff content. | BPR-0317, BPR-0304, BPR-0305, BPR-0306 | Discriminated authenticated/anonymous authority, versioned SQLite migration, installed-Git/SQLite restart, and real rendered selected-file course |
| [x] | GraphQL and viewer requests refuse before construction. | BPR-0300, BPR-0301, BPR-0306 | Admission and transport counters |
| [x] | Shape/capabilities are unavailable rather than defaulted. | BPR-0300, BPR-0302, BPR-0304 | Core/normalizer/projection/Qt negatives |
| [x] | Vault and authorization are untouched. | BPR-0301, BPR-0303, BPR-0306 | Positive vault/header canaries |
| [x] | Rate limits are typed and not retried. | BPR-0298, BPR-0302, BPR-0303 | Recorded fixture and controlled host course |
| [x] | 404 is existence-neutral. | BPR-0298, BPR-0302, BPR-0304 | Provider/Qt private-or-missing course |
| [x] | GHES refusal sends zero requests. | BPR-0301, BPR-0305 | Tests-disabled profile counters |
| [x] | Mode switching invalidates all identity-sensitive state. | BPR-0303, BPR-0305 | Two-mode host/application course |
| [x] | Evidence is local/target-native with exact platform availability. | BPR-0306, BPR-0314, BPR-0315, BPR-0316, BPR-0309 | Clean build/runtime/platform ledger |

## Documentation impact ledger

| Surface | Owner | Disposition in BPR-0308 |
|---|---|---|
| Product contract/system architecture | BPR-0308 | Updated — Explicit public-preview product and core/host/GitHub/Qt/composition ownership |
| GitHub host/provider contracts | BPR-0308 | Updated — Credential-free authority, operation set, REST comments, rate/404 behavior |
| Workbench, queue, epoch, Git, and SQLite architecture | BPR-0308 | Updated — Exact unavailable triage/viewer/action/association behavior plus discriminated authority, catalog migration, and cache/restart ownership |
| Foundation security/threat model | BPR-0308 | Updated — Zero vault/header, private non-disclosure, no observer-subprocess reuse |
| Build/test guide | BPR-0308 | Updated — Probe provenance, clean matrices, Release/platform/no-Actions rules |
| Operator authentication guidance | BPR-0308 | Updated — Public-preview limits, sign-in transition, GitHub.com-only rule |
| Agent guidance | BPR-0308 | Updated — Update only for a stable reusable boundary or workflow |
| `README.md` and public launch guidance | BPR-0308 | Updated — Exact discoverability, sign-in alternative, supported platform/profile selection |
| `docs/ux/workbench-specification.md` | BPR-0308 | Updated — Public-preview naming, diff reading, unavailable triage/actions, rate/404 presentation |
| `docs/verification/fixture-catalog.md` | BPR-0308 | Updated — Real versus synthetic provenance and public/private-content constraints |
| `docs/verification/master-verification-matrix.md` | BPR-0308 | Updated — Exact anonymous core/transport/diff/Qt/composition/platform rows |
| `docs/verification/release-gates.md` | BPR-0308 | Updated — Tests-disabled selection, Release seam absence, runtime/dependency and platform gates |
| Immutable-base changed-path disposition | BPR-0308 | Updated — `git diff --name-status a45750f6bd84716240aa27a20f0734f33808ab8e..HEAD` classifies every path as RFC-0017 update/no-impact or named out-of-scope ownership |


## Capability selection by platform

| Platform/profile | Current production selection | Evidence |
|---|---|---|
| GitHub.com on macOS | Enabled; activation sends zero requests | BPR-0316 native live candidate and 19-selector before/after Release course, 2026-09-04 |
| GitHub.com on Linux | Enabled; activation sends zero requests | BPR-1664 native WSL2 course and tests-disabled artifact; see platform expansion r1 |
| GitHub.com on Windows x64 | Enabled; activation sends zero requests | BPR-1664 native Windows course and tests-disabled artifact; see platform expansion r1 |
| Every GHES profile | Refused by policy; zero requests | BPR-0316 and BPR-1664 tests-disabled probes; never enabled by RFC-BPR-0017 |

## Original closeout command ledger

BPR-0309 follows the current board and implement-rfc verification policy:
focused acceptance checks and direct product execution, with unchanged passing
results reused. The earlier blanket clean-build/matrix replay plan is superseded.
No full clean dev/core/sanitizer matrix or whole-repository tidy rerun is claimed.
At that original closeout, only macOS was enabled. Earlier native Linux/Windows
unavailable-path evidence keeps its owning commit; that Mac run claims no native
replay on either platform. The later platform-expansion ledger below owns the
current Windows/Linux selection.

| Claim set | Exact command or interaction | Result | Evidence/source |
|---|---|---|---|
| External captures and affirmative product decision | Both `--validate-existing` commands below | Pass at `9da7ed90`: `VALID:5`, `BOUND:5`, `DERIVED:1`; positive build gate, four definitions and eleven evidence files bound. Inputs unchanged | Immutable v001 captures/decision, BPR-0298/BPR-0312/BPR-0445 |
| Current macOS product and semantic surface | Incremental `cmake --build --preset dev --parallel 6`; inventory and the existing nineteen course selectors below | 18/19 passed in 35.86 s; request-ID integration mismatch repaired, direct core rerun 1/1 in 0.40 s; final composed recheck recorded on BPR-0309 | BPR-0309 candidate over `3c68be8e` |
| Current tests-disabled Release | Incremental app-release build; existing course functions for Release absence, composed hosts, prohibited surface, runtime/dependencies, enabled selection and GHES refusal | Pass in 1.89 s; enabled GitHub.com and refused GHES, each with zero request/header/vault/sign-in effects | BPR-0309 candidate, `BUILD_TESTING=OFF` |
| Credential-free native HTTPS demonstration | `rfc0017_credential_free_native_https` plus Release absence in the selected course | Pass in the selected course and Release absence check | Existing BPR-0313 seam, not the designated live observer |
| Core-only and sanitizer scope | Retain historical BPR-0300 core anonymous test/112-test core-only result and BPR-0306 four-selector ASan/UBSan provenance; use current focused dev evidence for changed composition/host inputs | Historical passing results, not a current sanitizer claim. No clean replay claimed. BPR-0541 separately rechecked changed core request identity and dependency boundaries 5/5 | BPR-0300, BPR-0306, `141250a6` |
| macOS native enablement and public-service demonstration | BPR-0316 before/after nineteen-selector course and the separately authorized capped native read of MikeMarcin/BPR-Test#1 | Pass: nine HTTP 200 responses, zero credential/header/vault effects; no additional live read at closeout | `a319dd51`, public head `17990ba0c2fb50338141f35c0717f0f99639c7ba` |
| Linux and Windows platform policy | Reuse native unavailable courses; keep selection unavailable | Linux seventeen selectors/222 s and Windows fifteen selectors/1,136 s at their owning commits; final-head native replays omitted here | BPR-0314/BPR-0315 |
| Documentation, scope and hygiene | `python3 scripts/verify_documentation.py --root .`; `python3 tests/scripts/verify_documentation_tests.py`; `git diff --check`; exact base-to-candidate path reconciliation | Pass at closeout; 54 validator tests pass unchanged | BPR-0308, BPR-0309; changed C++ format/tidy and boundaries retained on BPR-0558/BPR-0541 |
| Capability-critical review | One native `rfc-critical-reviewer` on the committed candidate; only contained findings trigger narrow rechecks | Clean review of immutable `d9006237`; no blockers or in-scope follow-ups | BPR-0309 review record |

The no-write external validation commands (already passed, unchanged inputs) are:

```sh
python3 scripts/record_rfc0017_anonymous_contracts.py --packet docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-packet.md --response docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-response.md --course-manifest docs/verification/reviews/rfc0017-anonymous-probe-v001/course-manifest.json --output-root docs/verification/reviews/rfc0017-anonymous-probe-v001/captures --validate-existing
python3 scripts/validate_rfc0017_public_preview_decision.py --packet docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-packet.md --response docs/verification/reviews/rfc0017-public-preview-decision-v001/human-verdict.json --require build --output docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json --validate-existing
```

The selected macOS course imports the existing registration and assertion
functions from `scripts/verify_rfc0017_platform_course.py`, using `build/dev`
and `build/app-release` directly. It does not call `configure_and_build` or
create another build root. Registration is checked before execution, every
owed selector is named, and the returned count must be nineteen. The exact
selector is:

```sh
ctest --test-dir build/dev --no-tests=error --output-on-failure -R '^(application_anonymous_diff_authority|application_anonymous_public_preview_enabled_probe|application_anonymous_public_preview_ghes_refusal|application_anonymous_public_preview_live_candidate_wiring|application_anonymous_public_preview_public_path|application_anonymous_public_preview_release_contract|application_anonymous_public_preview_unavailable|application_anonymous_read_host|composed_application_hosts|core_anonymous_public_read|git_anonymous_public_read|github_anonymous_read_contract|native_https_anonymous_read_contract|native_https_anonymous_read_fixture|qt_anonymous_public_preview_scale_100|qt_anonymous_public_preview_scale_125|qt_anonymous_public_preview_scale_200|rfc0017_credential_free_native_https|sqlite_anonymous_epoch_authority)$'
```

## Immutable-base scope ledger

`a45750f6bd84716240aa27a20f0734f33808ab8e..HEAD` intentionally spans other
work. BPR-0308 must classify every exact changed path. RFC-BPR-0016 cards,
plans, RFC text, reviews, and verification ledger; RFC-BPR-0019 Dif cards,
plans, RFC text, source/assets/tooling, reviews, and verification ledger; and
pre-existing BPR-0261 replay work retain their own ownership and are
out-of-scope for RFC-BPR-0017 claims. The classification records exact paths
and commits; prefix or narrative exclusion alone is insufficient.

## Planning evidence and blockers

- At the immutable planning base, independent read-only inspection found no
  anonymous profile, credential-free
  native transport, viewer-less session, REST review-comment operation,
  anonymous diff authority, or anonymous Qt/product path. The selected pager
  already handles an all-false GraphQL shape; the fixed-ten/full-shape helper is
  tests-only and is not mislabeled as production.
- The current designated public observer is a macOS live-lane subprocess for
  one compiled target and does not count as native-HTTPS/product evidence.
- The real-service and positive product decisions are recorded in their immutable
  review directories. This ledger grants no new request, credential access,
  private repository access, or mutation authority.
- Final closeout is blocked by an unmet acceptance criterion or unresolved
  critical finding; omitted catalog matrices are reported without becoming
  new acceptance requirements.

## BPR-0308 documentation reconciliation — 2026-09-04

Current public behavior is documented in the product/system contracts, provider
and host contracts, Git/epoch and queue architecture, threat/foundation security,
workbench UX, README/operator guidance, build/test guide, fixture catalog, master
matrix, and release gates. The RFC's original scoping analysis is explicitly
bound to its immutable pre-implementation base; it no longer describes missing
REST comments or missing factories as current facts. The Qt agent guide adds
only the stable narrow public-preview projection boundary.

BPR-0558 repaired discoverability discovered during this pass: the ordinary
Public preview control now emits the selected profile/mode, and both fixture
and tests-disabled probes use it. Its 12 focused tests, 2 composition contracts,
Release probes, and the integrator's 7/7 reproduction are recorded on that card.
BPR-0316's before/after native course and capped live read remain the platform
enablement provenance. Neither record closes this RFC or enables Linux/Windows.

The [exact changed-path dispositions](RFC-BPR-0017-path-dispositions.tsv) expand
renames into old/new paths and classify the immutable-base difference through
product head `670726f9` plus this documentation slice. Each row names its owning
RFC/workstream, an update or no-impact disposition, and source commit references.
This is the path ledger explicitly required by BPR-0308; it is not a new test or
release gate. BPR-0309 reconciles later changed paths at final closeout.

## BPR-0309 closeout — 2026-09-04

The initial nineteen-selector run passed eighteen tests and exposed the new
invalidation request-ID integration mismatch. The helper now carries the
caller-issued core identity; the original non-default-ID equality check remains
intact. Core policy, anonymous host and the direct composed public path then
passed 3/3 in 0.99 seconds. Current Release checks passed in 1.89 seconds, with
zero effects for enabled GitHub.com and refused GHES. Strict tidy for the two
changed translation units, format, boundaries, documentation and diff gates
passed. Unchanged external verdict/capture inputs retain their read-only
validation; broad historical matrices are not presented as new evidence.

One native capability-critical reviewer accepted candidate `d9006237` with no
blocker. The integrator records the explicit exclusions and closes this RFC.
The exact path ledger is reconciled through this status slice, including later
BPR-0541 policy work and BPR-0256's separate claim as out-of-scope ownership.


## Platform expansion r1

BPR-1664 implements the [owner's additive platform mandate](../rfcs/amendments/RFC-BPR-0017-public-preview-platforms-r1.md),
`public-preview-platforms-r1`, from baseline `12ef1db532f78f25ceaa821a5a7bb2fcda001aa2`.
Candidate `c5a906abe` enables the existing anonymous composition on Windows and
Linux, repairs its stale test-only live diagnostic and includes the existing
conversation Qt selector in the strict inventory. No production provider,
host policy, mutation branch, credential capability, dependency or UI design
changes. Original closeout/plan and unavailable-platform evidence retain their history.

### Candidate identity

Native roots export `a771522d1` with exact final overlays from `c5a906abe`.
Application, CMake expectations, reviewed source pins and inventory source hashes
match the local committed files on both native targets. The final application
SHA-256 is `c6a28cceec84df80aab180e27ce3015a4ff85edc238728cb18958ee9d6c987f6`;
production app manifest `4b0e7b0739fef3e689f29c6034dc31c7e37090efa7f7df0c39dd3d3ab0af7ad5`;
mutation branch remains `16d135419d39078bc3993b4e1cb80226e190edce50cd24d9456843066043a0e1`.

| Native artifact | SHA-256 |
|---|---|
| Windows final Debug | `a1c36f27ffdec07e506ae093a0dd19eb7718aced8329a89b9966bb5b1ff12169` |
| Windows tests-disabled Release | `88f84d4d7da6c96848b756c7ce3f5ee2f632f9f8df4843e21ba634bc98fa7114` |
| Linux final Debug | `cc9e4f4eebe4ecba759450778f28d4a97a0fcbb76be35b4d946830a2d8ac0a2e` |
| Linux tests-disabled Release | `8b125e198d434f58b73f93077446d7df2b0672eea11c236ea383a92373c82e68` |

Release builds use the cap-12 source at `2e409ba54`; the later cap-15 sequence
and self-check repair affect only the diagnostic factory instantiated under
`BPR_BUILD_TESTING` and its guarded self-check, so these exact Release artifacts'
production path is unchanged. Final Debug applications
are rebuilt from the cap-15 source and their affected six compositions pass.

### Verification and command dispositions

| Surface | Candidate-bound evidence | Result |
|---|---|---|
| macOS regression | Actual public application rebuilt; canonical inventory plus all 20 applicable anonymous selectors; final cap-15 affected app checks | Pass; final six 8.50 s |
| Composition integration | Final four `application_composition_contract`, `application_review_round_trip_composition_contract`, `rfc0017_test_inventory`, `rfc0017_platform_course_launch_contract` | Pass 4/4, 76.48 s |
| Diagnostic negative control | Independent literal 15-operation worst-case sequence, 15-page stream, shared wrappers, 16th-send refusal; temporary cap-16 mutant | Mutant fails as required; restored committed cap-15 checks pass |
| Network isolation manifest | Ordinary shared-checkout CTest scans unrelated ignored `.mask/sessions` release source copies. Unchanged checker with source enumeration limited to `git ls-files`, actual configured manifest otherwise unchanged | Pass 811 configured / 738 source registrations; ordinary checkout remains environment-contaminated |
| Inventory regression | 20 inventory tests, absent/wrong-mode conversation and absent-selector-command controls; 47 platform-course contracts | Pass |
| Source hygiene | clang-format 22.1.8; strict clang-tidy 22 on changed C++ translation unit; `git diff --check` | Pass |
| Documentation | Documentation gate and 67 documentation validator tests | Pass before final evidence; rerun at finish |
| Native Windows | Windows 11 Home 10.0.26200 x64, MSVC 14.51, Qt 6.11.2; exact applicable native inventory/registration/selector functions from the canonical course | All 18 pass on final Debug; Windows deliberately omits two wire-fixture selectors |
| Windows tests-disabled artifact | Canonical surface, seam-absence, composed-host, dependency, enabled-activation and GHES-refusal assertions | Pass; both probes zero request/header/vault/sign-in effects |
| Native Linux | Ubuntu 26.04 / WSL2 6.18.33.2 x64, GCC 15.2.0, Qt 6.11.2; isolated Debug/Release canonical course | 20 selectors and Release assertions pass (`RFC0017_PLATFORM_COURSE_OK`, 183 s); final cap-15 six app checks pass 3.99 s |
| Native production reads | Final cap-15 production-factory public reads of MikeMarcin/BPR-Test#1 after actual quota reset | Both pass:14 HTTP200s each, same accepted public snapshot/head, zero credential effects; details below |

The base native command is `python3 scripts/verify_rfc0017_platform_course.py
--platform PLATFORM --dev-preset DEV --release-preset RELEASE --build-root ROOT
--expected-preview-selection enabled`. Windows uses `py -3`, `windows-dev` and
`windows-app-release`; Linux uses `python3`, `dev` and `app-release`.
An external bounded builder replaces only configure/build: same configure,
eleven Debug application/anonymous/sandbox targets, one Release app target;
canonical selector inventory, registrations, assertions and artifact checks are
retained. Initial build-all commands compiled unrelated targets and were stopped
by exact task-owned PID; no product verdict is inferred from those interruptions.
The [durable checkpoint](../rfcs/plans/public-preview-platforms-r1/handoff.md)
records exact source/build roots, candidate identities and command state.

Windows' six contained Qt/provider tests initially exited before main with
`0xC0000142`. A minimal USER32 loader probe isolates the cause: SSH's
`Service-0x0-55790f2f$` window station permits ordinary USER32 loading but its
zero-capability AppContainer fails DLL initialization (error1114). The unchanged
six rows pass 57.34 s from a unique InteractiveToken scheduled task in `WinSta0`,
with `QT_QPA_PLATFORM=offscreen`. The final unified 18-selector run uses that
same native offscreen context. No foreground interaction/capture, containment
relaxation, shared-root ACL or unrelated process/lease change. The one-off task
was removed after completion.

`windows-app-release` is now a restricted distribution graph; the legacy course
tries its raw build image before preparing adjacent plugins, which refuses with
`INSTALLED_RUNTIME_REFUSED`. The task-owned artifact was supplied locked Qt
plugins and an additional offscreen verification plugin, leaving executable
bytes unchanged; its actual activation and GHES probes then pass. Generic idle
runtime inspection is not claimed: that legacy invocation would open the
owner's default app profile and has no production private-root argument. The
scoped amendment requires the native selector set and tests-disabled probes,
not installation or general idle-runtime qualification. No release was made.

The first cap-9 and subsequent cap-12 Linux production reads failed after nine
and twelve HTTP200 first pages, respectively. They did not produce an accepted
snapshot. Production source audit identifies the complete one-page sequence:
ten content/status reads, timeline, recorded merge base, branch-tip merge base,
conditional ancestry and final confirmation (at most15). The final diagnostic
bounds that sequence and refuses pagination beyond its budget. A prematurely
started old Linux binary was stopped by its exact owned PID and is not evidence.
No retry, credential fallback or remote mutation was added or performed.
A focused self-review kept the linear platform selection and literal diagnostic
sequence: no new abstraction or production authority is needed. The counting
transport declaration is shared source, but only its test-guarded diagnostic
factory instantiation can reach the changed ceiling; shipped reads use the
existing production factory.

### Final production-service acceptance — 2026-10-20

After waiting for actual reset1792468516 (03:55:16UTC), both exact final Debug
artifacts completed their native production-factory course without retry.
The target was MikeMarcin/BPR-Test#1 at
`17990ba0c2fb50338141f35c0717f0f99639c7ba` on both platforms.

| Native target | Preflight core remaining/limit | Actual wire result | Accepted snapshot | Observed final core budget |
|---|---|---|---|---|
| Linux WSL2 |59/60|14 HTTP200s,5107ms|`public_preview`, production factories=true|60/46/reset1792472127|
| Windows11 x64 |45/60|14 HTTP200s,2724ms|`public_preview`, production factories=true|60/32/reset1792472127|

Each sequence performs ten initial content/status reads, timeline, two merge-base
reads and final PR confirmation. Neither target needed conditional ancestry or
pagination. Both record `PUBLIC_PREVIEW_LIVE_CANDIDATE_OK` and
`NATIVE_LIVE_PREVIEW_OK`; header/vault/sign-in, capability-probe and credentialed
refusal effects are all0, and independent single-attempt self-check=true. The
fifteen-send diagnostic ceiling and >=30 preflight floor were preserved. There
were no authenticated reads or remote mutations in these live courses.

All scoped PLATFORM-1/2/3 and card AC1/2/3 obligations now have direct evidence.
Source/authority predicates and all macOS protections remain unchanged. Exact
native roots were removed after commands completed, removing their source-cache
links first and preserving shared caches/leases/checkouts. Compact nonsecret
logs and launcher receipts remain outside the repository at
`/tmp/bpr-public-preview-expansion/` for coordinator consumption.

### Documentation and remaining qualification

Updated README, product contract, GitHub host, security threat model/corporate
overview, settings/build guide, workbench specification, RFC/index and master
verification matrix to state supported preview platforms and retain authority
limits. System architecture/core/provider/host/cache/Git/SQLite ownership,
fixture catalog and release policy have no semantic impact: their implementations,
authority/refusals, dependencies and qualification contracts are unchanged.

Platform enablement is separate from release qualification: no clean-account
installation, credential-vault, native desktop accessibility, crash privacy,
uninstall or vulnerability-matrix acceptance is claimed. WSL2 is Linux
functional evidence, not clean Ubuntu desktop evidence. Existing macOS evidence
and original RFC terminal cards are preserved. No human judgement is required
by this unchanged presentation.
