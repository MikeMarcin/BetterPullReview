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
| Registered public application identity | Pending | BPR-0275/BPR-0276 | Digest-bound owner authority, selected application record, public client identifier, exact permissions, and no shipped client secret |
| Resolved application kind | Pending | BPR-0275 | Accepted GitHub App or OAuth App decision with installation, lifetime, refresh, and rejected-alternative rationale |
| Real sanitized wire contracts | Pending | BPR-0276 | Actual selected-service success, pending, slow-down, denied, expired, disabled, malformed, oversized, and rate-limited capture with secret-free evidence |
| RFC-BPR-0002 Auto-Verified | Pending | BPR-0035/BPR-0036 | Direct real Windows x64 evidence and terminal RFC-BPR-0002 closeout; macOS, mocks, cross-compilation, and hosted workflows do not substitute |

The pending
[`rfc0016-prerequisite-exception-v001`](reviews/rfc0016-prerequisite-exception-v001/operator-packet.md)
packet can authorize available-target implementation only. It cannot verify or
remove the fourth final-closeout gate. The independent
[`rfc0016-github-app-registration-v001`](reviews/rfc0016-github-app-registration-v001/operator-packet.md)
packet can authorize only the exact registration and bounded capture it names.

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
| [ ] | Core, Qt, and projection expose no token, refresh token, device code, client identifier, or endpoint. | BPR-0277, BPR-0280, BPR-0282 | Compile-time boundary/type/symbol inventory with forbidden-field negative controls |
| [ ] | Pending, denied, expired, rate-limited, and disabled outcomes are typed and retain no transient material or partial profile. | BPR-0278, BPR-0279, BPR-0282 | Real-contract adapter replay plus host cleanup cuts |
| [ ] | Polling honors interval, slow-down, total bound, UI cancellation, and shutdown cleanup. | BPR-0279, BPR-0282 | Controlled-clock/manual-executor host matrix |
| [ ] | Certificate, unknown-authority redirect, and query redirect fail closed on both endpoints before forwarding. | BPR-0278, BPR-0282 | Native HTTPS hostile loopback matrix for device and token routes |
| [ ] | Off-authority verification address is refused with zero browser calls. | BPR-0280, BPR-0282 | Host browser-capability authority cut and Qt projection course |
| [ ] | Browser handoff failure leaves the flow live and later authorization can complete elsewhere. | BPR-0279, BPR-0280, BPR-0282 | Failing-opener application/Qt course followed by authorized terminal |
| [ ] | Manual token enrollment remains functional for every profile. | BPR-0280, BPR-0281, BPR-0282 | Manual-before/device/manual-after public composition courses including GHES |
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
- BPR-0035/BPR-0036, documentation, simplification, clean final replay, or
  independent capability-critical review remains open.
