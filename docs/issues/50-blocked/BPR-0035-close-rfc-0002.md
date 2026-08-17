---
id: BPR-0035
title: Verify and close RFC-BPR-0002
status: Blocked
rfc: RFC-BPR-0002
depends_on: [BPR-0030, BPR-0031, BPR-0032, BPR-0033, BPR-0034, BPR-0036, BPR-0037, BPR-0038, BPR-0234]
blocked_by: platform-windows
---

# Verify and close RFC-BPR-0002

RFC base: `bfa9ab3ad6b08fbb22b697ed6fe6688202d3335a`.

Replay the complete claim matrix from clean state, directly inspect the real
composition and rendered application, verify target-native credential adapters
and secret-free evidence, review the final diff skeptically, resolve every
documentation-ledger entry, and move every RFC card to `60-done/`.

Mark RFC-BPR-0002 Auto-Verified only when GitHub.com, recorded GHES 3.18,
native HTTPS, all three native credential stores, the fixed zero-launch CLI
boundary, core/host/Qt boundaries, security assertions, documentation, and
tracker state agree. Positive GitHub CLI execution is deferred to
RFC-BPR-0015 and is not a closeout criterion.

Every passing claim must name a reproducible command or interaction, a
falsifiable success condition, and retained secret-free evidence. Closeout
also requires fresh `dev` and `core-only` matrices, target-native builds and
network/vault checks, architecture/runtime/binary-dependency gates,
documentation validation over the immutable RFC base, fixture and mutation
audits, source-pack validation, and `git diff --check`.

The product-owner-approved Windows deferral does not block available-target
implementation, security review, documentation, simplification, or work on
later RFCs. BPR-0036 remains open until its real-machine evidence exists and
therefore blocks only the affected Windows claims and this final
Auto-Verified transition.

## Available-platform checkpoint

At `a600ab2c29d2b4088a3f2598dbb817c35af2eafd`, BPR-0016 through
BPR-0034 plus BPR-0037/BPR-0038 are in `60-done/`. The current RFC-owned macOS
matrix is 46/46, `core-only` is 17/17, and the final simplification's focused
AddressSanitizer/UndefinedBehaviorSanitizer matrix is 7/7. Documentation,
architecture, security, tests-disabled Release, native macOS, pinned Linux,
original-pack, workflow-absence, and diff gates are recorded in the
verification ledger.

This checkpoint is not an RFC closeout and changes no `Planned` Windows row.
BPR-0036 remains open for the product owner's real Windows run. This card
remains in `backlog`, and RFC-BPR-0002 remains `Proposed`.

## Closeout checklist

Historical evidence is provenance only; every row requires final replay at the final closeout head.

| State | Criterion | Direct verification surface | Command or attended interaction | Expected result | Durable evidence location | Gate class | Live blocker |
|---|---|---|---|---|---|---|---|
| [ ] | A configured GitHub.com profile opens a sanitized fixture PR through the normalized provider surface. | Recorded GitHub.com QAction-to-provider-to-window trace in application_github_public_path_github_com | `ctest --preset dev -R '^(application_github_public_path_github_com)$' --output-on-failure` | The trace reports PUBLIC_PATH_OK, GITHUB_FIXTURE_OK:github.com, and RENDER_OK after normalized metadata and conversations reach the window. | GitHub.com read-path row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “A configured GitHub.com profile opens a sanitized fixture PR through the normalized provider surface.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | A configured GHES 3.18 profile opens the equivalent fixture using only supported contracts. | Recorded GHES 3.18 provider facade and rendered window trace in application_github_public_path_ghes_3_18 | `ctest --preset dev -R '^(application_github_public_path_ghes_3_18)$' --output-on-failure` | The trace selects the exact GHES hostname and API roots, API version 2022-11-28, reports GITHUB_FIXTURE_OK:ghes-3.18, and renders the equivalent normalized result. | GHES 3.18 contract-selection row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “A configured GHES 3.18 profile opens the equivalent fixture using only supported contracts.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | Capability probing reports review threads, viewed-file mutations, resolve/unresolve, and unsupported features accurately. | Capability probe, exact-authority cache, and core command policy in github_capability_probe and application_github_capability_cache | `ctest --preset dev -R '^(github_capability_probe\|application_github_capability_cache\|core_github_policy)$' --output-on-failure` | Supported, unsupported, and insufficient-evidence probes retain distinct tri-state results for threads, viewed state, resolve, and unresolve; unavailable actions stay disabled. | Capability-state row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “Capability probing reports review threads, viewed-file mutations, resolve/unresolve, and unsupported features accurately.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | The UI presents clear authentication, permission, network, API-version, and capability failures. | Typed GitHub failure projection in application_github_host and the three qt_composed_github scale cases | `ctest --preset dev -R '^(application_github_host\|qt_composed_github_scale_(100\|125\|200))$' --output-on-failure` | Authentication, permission, network, API-version, and capability failures produce distinct bounded reviewer text with no raw remote or credential detail. | Failures-are-clear-and-bounded row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “The UI presents clear authentication, permission, network, API-version, and capability failures.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | Canary credentials do not appear in logs, process arguments, diagnostics, SQLite, or UI error text. | Real Windows x64 application, Credential Manager lifecycle, process arguments, diagnostics, SQLite, and rendered error text | Run [BPR-0036](BPR-0036-windows-x64-target-native-verification.md#run-rfc-bpr-0002-windows-x64-target-native-verification)'s clean Windows x64 vault lifecycle, application self-test, process-tree, database, diagnostic, and rendered-error canary procedure. | The disposable canary is absent from process arguments, logs, diagnostics, SQLite, and UI text and the native credential item is deleted. | BPR-0036 target-native evidence summarized by the Windows credential and runtime rows in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “Canary credentials do not appear in logs, process arguments, diagnostics, SQLite, or UI error text.” | target-native | [BPR-0036](BPR-0036-windows-x64-target-native-verification.md) remains open and owns the unreplayed surface. |
| [ ] | Every visible GitHub CLI choice produces `cli_unavailable` without CLI executable or configuration discovery, process creation, or descendants. | Public CLI-unavailable self-test plus tests-disabled composition and zero-call provider/process counters | `ctest --preset dev -R '^(application_public_path_ghes_cli_unavailable\|application_composition_contract\|gh_cli_transport)$' --output-on-failure` | Default and explicit-GHES runs report CLI_UNAVAILABLE_OK with counts 1 and 2; no CLI discovery, process, descendant, credential, wire, or capability call occurs. | Every-visible-CLI-mode-is-unavailable row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “Every visible GitHub CLI choice produces `cli_unavailable` without CLI executable or configuration discovery, process creation, or descendants.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | The native provider rejects certificate failures, unknown-authority redirects before forwarding credentials or bodies, and all query-bearing redirects. | Native HTTPS hostile redirect, TLS, credential-forwarding, and loopback fixture boundary | `ctest --preset dev -R '^(native_https_transport_contract\|native_https_transport_fixture)$' --output-on-failure` | Certificate failure, unknown-authority redirect, and every query-bearing redirect fail before credentials or request bodies reach the new authority. | macOS/Linux native-HTTPS-fails-closed row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “The native provider rejects certificate failures, unknown-authority redirects before forwarding credentials or bodies, and all query-bearing redirects.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | Switching host or account invalidates all identity-sensitive caches and projections. | Core generation policy, host staged-completion lane, and atomic Qt identity projection | `ctest --preset dev -R '^(core_github_policy\|application_github_host\|qt_composed_github_scale_(100\|125\|200))$' --output-on-failure` | Switching host or account advances identity generation, clears caches/projections, exact-deletes the predecessor binding, and rejects every old in-flight completion. | Host/account-switching invalidation row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “Switching host or account invalidates all identity-sensitive caches and projections.” | automated | No live blocker; final-head focused replay remains required. |
| [ ] | Core tests run with no Qt/network/credential implementation, host tests use fake transports and reordered completions, and Qt tests prove event translation and capability-driven action state without credential access. | Layer boundary scans plus core policy, application host, and Qt conversation projection suites | `ctest --preset dev -R '^(core_dependency_boundary\|architecture_boundaries\|core_github_policy\|application_github_host\|qt_github_conversation_panel)$' --output-on-failure` | Core links no Qt, network, or credential implementation; host owns effects and acceptance staging; Qt emits intents and renders accepted projections without credential access. | Core-decides-host-performs-Qt-presents row in the claim matrix in [RFC-BPR-0002 verification ledger](../../verification/RFC-BPR-0002.md#claim-matrix); final replay record keyed by “Core tests run with no Qt/network/credential implementation, host tests use fake transports and reordered completions, and Qt tests prove event translation and capability-driven action state without credential access.” | automated | No live blocker; final-head focused replay remains required. |
