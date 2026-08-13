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
| Real anonymous-service contracts | Pending | BPR-0298 | Sanitized executed probes for GraphQL, REST comments/paging, rate/304, 404, and exact GitHub.com shapes |
| Deliberate credential-free native HTTPS | Pending | BPR-0301 | Typed anonymous authority with unchanged RFC-0002 transport rules and zero header/vault canaries |
| Product build/no-build decision | Pending | BPR-0299 | Digest-bound measured limit/capability/UI decision |
| RFC-BPR-0002 Auto-Verified | Pending | BPR-0035/BPR-0036 | Direct real Windows x64 evidence and terminal RFC-0002 closeout |

## Claim matrix

| State | Claim | Owner | Durable final evidence |
|---|---|---|---|
| [ ] | Existing provider/normalizer opens a public PR without a parallel path. | BPR-0302, BPR-0305, BPR-0306 | Tests-disabled recorded-contract composition |
| [ ] | GraphQL and viewer requests refuse before construction. | BPR-0300, BPR-0301, BPR-0306 | Admission and transport counters |
| [ ] | Shape/capabilities are unavailable rather than defaulted. | BPR-0300, BPR-0302, BPR-0304 | Core/normalizer/projection/Qt negatives |
| [ ] | Vault and authorization are untouched. | BPR-0301, BPR-0303, BPR-0306 | Positive vault/header canaries |
| [ ] | Rate limits are typed and not retried. | BPR-0298, BPR-0302, BPR-0303 | Recorded fixture and controlled host course |
| [ ] | 404 is existence-neutral. | BPR-0298, BPR-0302, BPR-0304 | Provider/Qt private-or-missing course |
| [ ] | GHES refusal sends zero requests. | BPR-0301, BPR-0305 | Tests-disabled profile counters |
| [ ] | Mode switching invalidates all identity-sensitive state. | BPR-0303, BPR-0305 | Two-mode host/application course |
| [ ] | Evidence is local/target-native with exact platform availability. | BPR-0306, BPR-0309 | Clean build/runtime/platform ledger |

## Documentation impact ledger

| Surface | Owner | Required disposition |
|---|---|---|
| Product contract/system architecture | BPR-0308 | Explicit public-preview product and core/host/GitHub/Qt/composition ownership |
| GitHub host/provider contracts | BPR-0308 | Credential-free authority, operation set, REST comments, rate/404 behavior |
| Workbench, queue, and epoch architecture | BPR-0308 | Exact unavailable triage/viewer/action/association behavior |
| Foundation security/threat model | BPR-0308 | Zero vault/header, private non-disclosure, no observer-subprocess reuse |
| Build/test guide | BPR-0308 | Probe provenance, clean matrices, Release/platform/no-Actions rules |
| Operator authentication guidance | BPR-0308 | Public-preview limits, sign-in transition, GitHub.com-only rule |
| Agent guidance | BPR-0308 | Update only for a stable reusable boundary or workflow |

## Capability selection by platform

| Platform/profile | Initial production selection after BPR-0305 | Enabling evidence |
|---|---|---|
| GitHub.com on macOS | Available only after BPR-0306's native HTTPS, Keychain-no-lookup, Cocoa, Release, runtime, and hygiene matrix passes | BPR-0306 |
| GitHub.com on Linux | Typed public-preview unavailable, zero requests | Future separately planned target-native card |
| GitHub.com on Windows x64 | Typed public-preview unavailable, zero requests | Future separately planned target-native card |
| Every GHES profile | Typed public-preview unavailable, zero requests | Not enabled by RFC-BPR-0017 |

## Final command ledger

These are planned selector names, not passing evidence. BPR-0306 must register
them exactly or update this ledger and BPR-0309 atomically. Result, durable
evidence, and commit remain pending until final-head execution.

| Claim set | Exact command or attended interaction | Falsifiable proof | Result | Durable evidence | Commit |
|---|---|---|---|---|---|
| Real anonymous-service contracts | Quote the exact future statement in `reviews/rfc0017-anonymous-probe-v001/operator-packet.md`, then run BPR-0298's bounded credential-free course | Named targets/requests only; real GraphQL/comments/rate/304/404 facts; zero credentials/private content/mutation/limit evasion | Pending | `reviews/rfc0017-anonymous-probe-v001/operator-response.md` and BPR-0298 provenance | Pending |
| Product build/no-build decision | Quote the exact future statement in `reviews/rfc0017-public-preview-decision-v001/operator-packet.md` | Measured cost/capability/UI facts match; rejection leaves implementation open and RFC Deferred | Pending | `reviews/rfc0017-public-preview-decision-v001/operator-response.md` | Pending |
| Focused semantic/transport/normalizer/host/Qt/product matrix | `ctest --test-dir build/dev --output-on-failure -R '^(core_anonymous_public_read|github_anonymous_read_contract|native_https_anonymous_read_(contract|fixture)|application_anonymous_read_host|qt_anonymous_public_preview_scale_(100|125|200)|application_anonymous_public_preview_(public_path|unavailable|release_contract))$'` | All selectors pass and every header/vault/GraphQL/viewer/GHES/mutation/private/action bypass cut is non-vacuous | Pending | This row plus final CTest summary and exact fixture paths | Pending |
| Core-only matrix | `cmake --preset core-only && cmake --build --preset core-only && ctest --preset core-only --output-on-failure` | Anonymous legality/unavailability builds and passes without Qt/provider/effects | Pending | This row plus final counts and core selectors | Pending |
| Tests-disabled Release | `cmake --preset app-release && cmake --build --preset app-release` | macOS selection builds; fixture/test seams absent; Linux/Windows/GHES stay unavailable | Pending | This row plus executable digest/size and selection evidence | Pending |
| macOS native surface | `ctest --test-dir build/dev --output-on-failure -R '^(native_https_anonymous_read_fixture|application_anonymous_public_preview_cocoa|application_anonymous_public_preview_release_contract)$'` | Native trust/proxy/redirect and real Cocoa course pass with zero Authorization/vault effects | Pending | This row plus bound host/artifact/version record | Pending |
| Boundary and hygiene | `cmake --build build/dev --target check_boundaries coding-format-check coding-tidy && python3 scripts/verify_documentation.py --root . && git diff --check` | No parallel provider/normalizer, credential fallback, default viewer, test seam, Actions workflow, format/tidy/docs/diff failure | Pending | This row plus gate outputs and scanned inventory | Pending |
| Final complete local matrix | `cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure` | Clean complete configured macOS matrix passes at immutable closeout head | Pending | This row plus final count/duration/commit | Pending |
| Capability-critical closeout | Independent `rfc-critical-reviewer` review of immutable final head, artifacts, packets, commands, and claims | Reviewer returns ACCEPT with no unresolved blocking/high-risk finding | Pending | BPR-0309 final five-section review record | Pending |

## Planning evidence and blockers

- Independent read-only inspection found no anonymous profile, credential-free
  native transport, viewer-less session, availability-aware normalizer, or
  anonymous Qt/product path.
- The current designated public observer is a macOS live-lane subprocess for
  one compiled target and does not count as native-HTTPS/product evidence.
- The exact real-service and product decisions remain attended/external. No
  request, credential, private repository, mutation, or product mode is
  authorized by this ledger.
- Final closeout is blocked by any open eligibility, platform, documentation,
  simplification, clean replay, or critical-review row.
