# RFC-BPR-0017 Verification Ledger

RFC base: `a45750f6bd84716240aa27a20f0734f33808ab8e`

Current RFC status: `Deferred`

This is planning, not passing evidence. RFC-BPR-0016 remains the stronger
answer unless BPR-0298 and BPR-0299 establish that public preview is useful and
honest.

## Eligibility ledger

| Gate | State | Owner | Required evidence |
|---|---|---|---|
| Correct live-tree scoping | Pending | BPR-0297 | RFC records missing REST comments, credential-mandatory transport, viewer/account assumptions, and normalizer gaps |
| Real anonymous-service contracts | Partial — 4/5 recorded; the GraphQL sanitizer is hermetically proven and its fresh one-request authorization remains pending | BPR-0298, BPR-0445 | Sanitized executed probes for GraphQL, REST comments/paging, rate/304, 404, and exact GitHub.com shapes |
| Deliberate credential-free native HTTPS demonstration | Pending | BPR-0313 | `BUILD_TESTING`-only non-product typed request with unchanged RFC-0002 mechanics, zero header/vault/non-loopback network effects, and Release symbol absence |
| Product build/no-build decision | Pending | BPR-0299 | Digest-bound measured limit/capability/UI decision |
| Positive build approval | Pending | BPR-0312 | Exact affirmative response and validation record; rejection leaves this gate unresolved |
| RFC-BPR-0002 Auto-Verified | Pending | BPR-0035/BPR-0036 | Direct real Windows x64 evidence and terminal RFC-0002 closeout |

## Claim matrix

| State | Claim | Owner | Durable final evidence |
|---|---|---|---|
| [ ] | Existing provider/normalizer opens a public PR without a parallel path. | BPR-0302, BPR-0305, BPR-0306 | Tests-disabled recorded-contract composition |
| [ ] | Credential-free public Git/cache/epoch authority renders exact selected-file diff content. | BPR-0317, BPR-0304, BPR-0305, BPR-0306 | Discriminated authenticated/anonymous authority, versioned SQLite migration, installed-Git/SQLite restart, and real rendered selected-file course |
| [ ] | GraphQL and viewer requests refuse before construction. | BPR-0300, BPR-0301, BPR-0306 | Admission and transport counters |
| [ ] | Shape/capabilities are unavailable rather than defaulted. | BPR-0300, BPR-0302, BPR-0304 | Core/normalizer/projection/Qt negatives |
| [ ] | Vault and authorization are untouched. | BPR-0301, BPR-0303, BPR-0306 | Positive vault/header canaries |
| [ ] | Rate limits are typed and not retried. | BPR-0298, BPR-0302, BPR-0303 | Recorded fixture and controlled host course |
| [ ] | 404 is existence-neutral. | BPR-0298, BPR-0302, BPR-0304 | Provider/Qt private-or-missing course |
| [ ] | GHES refusal sends zero requests. | BPR-0301, BPR-0305 | Tests-disabled profile counters |
| [ ] | Mode switching invalidates all identity-sensitive state. | BPR-0303, BPR-0305 | Two-mode host/application course |
| [ ] | Evidence is local/target-native with exact platform availability. | BPR-0306, BPR-0314, BPR-0315, BPR-0316, BPR-0309 | Clean build/runtime/platform ledger |

## Documentation impact ledger

| Surface | Owner | Required disposition |
|---|---|---|
| Product contract/system architecture | BPR-0308 | Explicit public-preview product and core/host/GitHub/Qt/composition ownership |
| GitHub host/provider contracts | BPR-0308 | Credential-free authority, operation set, REST comments, rate/404 behavior |
| Workbench, queue, epoch, Git, and SQLite architecture | BPR-0308 | Exact unavailable triage/viewer/action/association behavior plus discriminated authority, catalog migration, and cache/restart ownership |
| Foundation security/threat model | BPR-0308 | Zero vault/header, private non-disclosure, no observer-subprocess reuse |
| Build/test guide | BPR-0308 | Probe provenance, clean matrices, Release/platform/no-Actions rules |
| Operator authentication guidance | BPR-0308 | Public-preview limits, sign-in transition, GitHub.com-only rule |
| Agent guidance | BPR-0308 | Update only for a stable reusable boundary or workflow |
| `README.md` and public launch guidance | BPR-0308 | Exact discoverability, sign-in alternative, supported platform/profile selection |
| `docs/ux/workbench-specification.md` | BPR-0308 | Public-preview naming, diff reading, unavailable triage/actions, rate/404 presentation |
| `docs/verification/fixture-catalog.md` | BPR-0308 | Real versus synthetic provenance and public/private-content constraints |
| `docs/verification/master-verification-matrix.md` | BPR-0308 | Exact anonymous core/transport/diff/Qt/composition/platform rows |
| `docs/verification/release-gates.md` | BPR-0308 | Tests-disabled selection, Release seam absence, runtime/dependency and platform gates |
| Immutable-base changed-path disposition | BPR-0308 | `git diff --name-status a45750f6bd84716240aa27a20f0734f33808ab8e..HEAD` classifies every path as RFC-0017 update/no-impact or named out-of-scope ownership |

## Capability selection by platform

| Platform/profile | Initial production selection after BPR-0305 | Enabling evidence |
|---|---|---|
| GitHub.com on macOS | Unavailable through BPR-0306; BPR-0316 enables only after native candidate proof and same-slice selected-artifact rerun | BPR-0316 |
| GitHub.com on Linux | Typed public-preview unavailable, zero requests | BPR-0314 target-native unavailable-path proof |
| GitHub.com on Windows x64 | Typed public-preview unavailable, zero requests | BPR-0036 plus BPR-0315 target-native unavailable-path proof |
| Every GHES profile | Typed public-preview unavailable, zero requests | Not enabled by RFC-BPR-0017 |

## Final command ledger

These are planned selector/script names, not passing evidence. Owning cards must
register them exactly or update this ledger and BPR-0309 atomically. Inventory
scripts use `ctest -N --show-only=json-v1` and require the complete exact name
set and count before execution. The first Linux/Windows unavailable courses and
the one-time macOS enablement record their BPR-0314/BPR-0315/BPR-0316 commits as
provenance. BPR-0309 separately reruns all three platform courses at immutable
final head. Earlier probe and decision rows retain their owning-commit evidence
and are byte-revalidated at closeout; every replayable automated row is executed
again against the immutable final head.

| Claim set | Exact command or attended interaction | Falsifiable proof | Result | Durable evidence | Commit |
|---|---|---|---|---|---|
| Real anonymous-service contracts | Validate the four immutable v001 captures with `python3 scripts/record_rfc0017_anonymous_contracts.py --packet docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-packet.md --response docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-response.md --course-manifest docs/verification/reviews/rfc0017-anonymous-probe-v001/course-manifest.json --output-root docs/verification/reviews/rfc0017-anonymous-probe-v001/captures --validate-existing`; the fifth course requires BPR-0445's new packet and exact response before exactly one GraphQL request | Script validates digest-bound authority before traffic; named targets/requests only and once with no proxy/redirect escape; capture requires exact response bytes, a packet-bound current recorder, and an exact packet-embedded course list matching the packet-bound manifest before durably reserving a repository-wide packet-identity receipt; reordered or conflicting authority/course blocks refuse before traffic; GraphQL 403 zero-quota refusal normalizes exactly one caller address and stores no raw-body digest; other bodies remain byte-faithful; zero credentials/private content/mutation/limit exhaustion or evasion | Partial — the v001 capture ran once (`CAPTURED:5`) and four courses remain immutable (`VALID:4`); BPR-0445's 82 hermetic tests prove scoped IPv4/IPv6 normalization, hostile-shape/duplicate-course refusal, bidirectional packet/manifest course-list binding, exact authority/manifest/capture types, exact packet/response/recorder binding with zero-request stale/contradictory/reordered rejection, copied-directory/cross-root crash-durable single-use receipt, pre-request destination reservation, no-proxy/no-redirect transport, the 403 quota contract, and normalized-record structural/digest consistency without traffic; v002 packet `8ff060e82c04f30010ad11e5410200d8797d8f1c5b08557768ded86ed0eafdd0` now waits on fresh authorization before the one GraphQL recapture, as does BPR-0298's authentication of every minimized record against immutable committed evidence | Consumed v001 response, trimmed manifest, four minimized captures, v002 pending packet/manifest, BPR-0298 provenance, and BPR-0445 sanitizer tests | `4d250ab6`; `a27e4a5`; `c03abcf`; BPR-0445 parked for attended response |
| Non-product credential-free demonstration | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && cmake --preset app-release --fresh && cmake --build --preset app-release --clean-first && python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --required native_https_credential_free_demonstration --release-binary build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview --verify-release-absence && ctest --test-dir build/dev --no-tests=error --output-on-failure -R '^native_https_credential_free_demonstration$'` | Exact test exists; typed credential-free loopback uses zero header/vault/subprocess/non-loopback network; invalid authenticated material does not fall back; platform-aware symbol/string scan proves the tests-disabled Release artifact excludes the demonstration seam | Pending | BPR-0313 direct and Release-absence evidence | Pending |
| Product build/no-build decision and positive gate | Record the exact future response from `docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-packet.md`, then run `python3 scripts/validate_rfc0017_public_preview_decision.py --packet docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-packet.md --response docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-response.md --require build --output docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json` | Validator binds measured cost/capability/UI facts and accepts only the exact affirmative branch; rejection leaves BPR-0312 unresolved and implementation blocked | Pending | Packet response plus BPR-0312 validation record | Pending |
| Immutable external-evidence validation | `python3 scripts/record_rfc0017_anonymous_contracts.py --packet docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-packet.md --response docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-response.md --course-manifest docs/verification/reviews/rfc0017-anonymous-probe-v001/course-manifest.json --output-root docs/verification/reviews/rfc0017-anonymous-probe-v001/captures --validate-existing && python3 scripts/validate_rfc0017_public_preview_decision.py --packet docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-packet.md --response docs/verification/reviews/rfc0017-public-preview-decision-v001/operator-response.md --require build --output docs/verification/reviews/rfc0017-public-preview-decision-v001/validation-record.json --validate-existing` | No traffic or write occurs; every decision/probe/capture digest, minimized-output record, designated authority, positive branch, and owning commit remains exact at immutable final head | Pending | BPR-0309 no-write validator output and immutable-head identity | Pending |
| Focused semantic/transport/normalizer/host/diff/Qt/product matrix | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all && ctest --test-dir build/dev --no-tests=error --output-on-failure -R '^(core_anonymous_public_read|github_anonymous_read_contract|native_https_anonymous_read_(contract|fixture)|git_anonymous_public_read|sqlite_anonymous_epoch_authority|application_anonymous_read_host|application_anonymous_diff_authority|qt_anonymous_public_preview_scale_(100|125|200)|application_anonymous_public_preview_(public_path|unavailable|release_contract))$'` | Fresh identity and exact inventory/count exist; all selectors pass, real Git/SQLite restart and migration evidence is present, and header/vault/GraphQL/viewer/GHES/mutation/private/action/diff bypass cuts are non-vacuous | Pending | This row plus build identity, inventory, final CTest summary, and fixture paths | Pending |
| Core-only matrix | `cmake --preset core-only --fresh && cmake --build --preset core-only --clean-first && python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/core-only --required core_anonymous_public_read && ctest --test-dir build/core-only --no-tests=error --output-on-failure -R '^core_anonymous_public_read$' && ctest --preset core-only --output-on-failure` | Exact core selector exists in the fresh Qt/provider-free inventory, passes directly, and the complete core-only matrix passes | Pending | This row plus build identity, exact inventory/count, final counts, and core selector | Pending |
| Sanitizer course | `python3 scripts/verify_rfc0017_sanitizer_course.py --build-root build/rfc0017-sanitize --fresh` | Script creates an isolated ASan/UBSan build and inventory-runs exact changed core/transport/normalizer/Git/SQLite/host/diff selectors with no sanitizer diagnostic | Pending | Script manifest, toolchain flags, selector inventory, output, commit | Pending |
| Tests-disabled Release | `cmake --preset app-release --fresh && cmake --build --preset app-release --clean-first && python3 scripts/verify_binary_dependencies.py build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview && python3 scripts/verify_runtime_surface.py build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview` | Fresh artifact has selected macOS mode, fixture/test seams absent, idle runtime bounded, dependencies allowed; Linux/Windows/GHES policy remains unavailable | Pending | Executable digest/size, build identity, dependency/runtime and selection evidence | Pending |
| macOS one-time enablement | In BPR-0316 run `python3 scripts/verify_rfc0017_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0017-macos-enablement --expected-preview-selection unavailable --fresh`; flip only macOS selection and rerun with `--expected-preview-selection enabled --fresh` | Inventory-validated isolated course runs native HTTPS, zero Keychain/header, REST comments, rate/404, Git/cache/epoch/diff, browser/Cocoa, runtime/dependency, Release, and hygiene before and after flip | Pending | BPR-0316 bound host/toolchain/artifact and one-time flip record | Pending |
| Linux one-time unavailable-path proof | In BPR-0314 on supported Linux run `python3 scripts/verify_rfc0017_platform_course.py --platform linux --dev-preset dev --release-preset app-release --build-root build/rfc0017-linux-enablement --expected-preview-selection unavailable --fresh` | Inventory-validated native course proves typed unavailable and zero keyring/header/REST/GraphQL/Git/browser/mutation/child/private effects plus ELF/runtime/dependencies | Pending | BPR-0314 bound Linux provenance | Pending |
| Windows one-time unavailable-path proof | In BPR-0315 on real Windows 10/11 x64 run `py -3 scripts/verify_rfc0017_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0017-windows-enablement --expected-preview-selection unavailable --fresh` | Inventory-validated native course proves typed unavailable and zero Credential Manager/header/REST/GraphQL/Git/browser/mutation/child/private effects plus PE/runtime/dependencies | Pending | BPR-0036/BPR-0315 bound Windows provenance | Pending |
| Immutable macOS final-head replay | `python3 scripts/verify_rfc0017_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0017-macos-closeout --expected-preview-selection enabled --fresh` | Already-enabled immutable head passes isolated fresh Debug/Release inventory, native transport/vault absence, rendered diff, browser/Cocoa, runtime/dependency and hygiene; no selection mutation occurs | Pending | BPR-0309 final macOS course record | Pending |
| Immutable Linux final-head replay | On supported Linux run `python3 scripts/verify_rfc0017_platform_course.py --platform linux --dev-preset dev --release-preset app-release --build-root build/rfc0017-linux-closeout --expected-preview-selection unavailable --fresh` | Immutable head still selects typed unavailability and passes isolated fresh Debug/Release inventory, zero-effect, ELF/runtime/dependency, and hygiene checks | Pending | BPR-0309 final Linux course record | Pending |
| Immutable Windows final-head replay | On real Windows 10/11 x64 run `py -3 scripts/verify_rfc0017_platform_course.py --platform windows --dev-preset windows-dev --release-preset windows-app-release --build-root build/rfc0017-windows-closeout --expected-preview-selection unavailable --fresh` | Immutable head still selects typed unavailability and passes isolated fresh Debug/Release inventory, zero-effect, PE/runtime/dependency, and hygiene checks | Pending | BPR-0036/BPR-0309 final Windows course record | Pending |
| Boundary/documentation/hygiene | `cmake --build build/dev --target check_boundaries coding-format-check coding-tidy && python3 scripts/verify_documentation.py --root . && git diff --check a45750f6bd84716240aa27a20f0734f33808ab8e..HEAD && git diff --name-status a45750f6bd84716240aa27a20f0734f33808ab8e..HEAD` | No parallel provider/helper selection, credential fallback, default viewer, test seam, workflow, format/tidy/docs/diff failure; every path is dispositioned | Pending | Gate outputs and immutable-base disposition ledger | Pending |
| Final complete local matrix | `cmake --preset dev --fresh && cmake --build --preset dev --clean-first && python3 scripts/verify_rfc0017_test_inventory.py --build-dir build/dev --all && ctest --preset dev --output-on-failure` | Fresh inventory requires every RFC selector and complete configured macOS matrix passes | Pending | Build identity, inventory, final count/duration/commit | Pending |
| Capability-critical closeout | Independent `rfc-critical-reviewer` review of immutable final head, artifacts, packets, commands, and claims | All five required review sections are present; Blocking findings and Verification failures have no unresolved item after primary reproduction; high-risk residuals are explicitly adjudicated | Pending | BPR-0309 five-section record and primary reproduction/adjudication | Pending |

## Immutable-base scope ledger

`a45750f6bd84716240aa27a20f0734f33808ab8e..HEAD` intentionally spans other
work. BPR-0308 must classify every exact changed path. RFC-BPR-0016 cards,
plans, RFC text, reviews, and verification ledger; RFC-BPR-0019 Dif cards,
plans, RFC text, source/assets/tooling, reviews, and verification ledger; and
pre-existing BPR-0261 replay work retain their own ownership and are
out-of-scope for RFC-BPR-0017 claims. The classification records exact paths
and commits; prefix or narrative exclusion alone is insufficient.

## Planning evidence and blockers

- Independent read-only inspection found no anonymous profile, credential-free
  native transport, viewer-less session, REST review-comment operation,
  anonymous diff authority, or anonymous Qt/product path. The selected pager
  already handles an all-false GraphQL shape; the fixed-ten/full-shape helper is
  tests-only and is not mislabeled as production.
- The current designated public observer is a macOS live-lane subprocess for
  one compiled target and does not count as native-HTTPS/product evidence.
- The exact real-service and product decisions remain attended/external. No
  request, credential, private repository, mutation, or product mode is
  authorized by this ledger.
- Final closeout is blocked by any open eligibility, platform, documentation,
  simplification, clean replay, or critical-review row.
