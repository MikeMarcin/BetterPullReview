---
id: BPR-0032
title: Verify RFC-BPR-0002 security and authority boundaries
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0021, BPR-0022, BPR-0023, BPR-0024, BPR-0025, BPR-0026, BPR-0027, BPR-0029, BPR-0030, BPR-0031, BPR-0037, BPR-0038]
completed: 2026-07-30
---

# Verify RFC-BPR-0002 security and authority boundaries

Audit credential lifetime and storage, child-process arguments/environment,
network trust/proxy/redirect behavior, hostname/API-root validation, response
limits, diagnostics, settings, SQLite, UI text, target identity, cache keys,
and dependency direction with disposable canaries and retained secret-free
evidence.

Audit the target-native HTTPS backend and binary dependencies, failure-only
vault states and fallback absence, redirect sinks, certificate rejection,
query-bearing redirect rejection, parsed REST/GraphQL operation types, and
the complete fixed CLI-unavailable boundary.

For milestone one, the CLI security claim is categorical zero launch rather
than partial broker safety. Verify every visible CLI selection returns the
typed `cli_unavailable` result. A clean-start selection performs one bounded,
noncredential profile-resolution effect. A switch from an authenticated native
profile may first exact-delete the prior native credential binding, as required
by identity invalidation. Neither path may enroll or retrieve a credential for
the CLI profile, discover a CLI executable or configuration, construct or
invoke a transport factory, perform wire or capability work, schedule a
CLI-specific effect, create a process, or create descendants.
Verify the tests-disabled Release executable omits the dormant CLI transport,
configuration, request-compiler, and runner symbols. Retain the exact eight
availability reasons and current official `gh` 2.96.0 trust evidence as the
rationale for RFC-BPR-0015: executable-image identity, redirect preflight,
effective `http_unix_socket` rerouting, credential/account binding,
working-directory identity, descendant containment, and numeric PID/PGID
reuse all remain outside the milestone-one production contract.

Complete direct target-native checks on the available macOS and Linux
surfaces. By product-owner direction, BPR-0036 owns the equivalent Windows
10/11 x64 runtime, binary-dependency, Credential Manager, HTTPS,
public-composition, and secret-absence evidence. Positive Windows CLI
execution belongs only to deferred RFC-BPR-0015. BPR-0036's open state keeps
the affected Windows claims and final RFC closeout open without blocking this
cross-platform security-review slice or later RFC implementation.

Only read-only GitHub operations are in scope; no mutation permission or live
mutation target is required.

GitHub Actions has no build, test, package, release, or verification role.

## Result

- Re-audited credential authority/lifetime, native trust and redirect policy,
  read-only request admission, runtime/process scope, cache identity, public
  composition, and documentation claims. Corrected every discovered
  overstatement rather than treating a fixture name or old snapshot as current
  evidence.
- Distinguished the recorded public transport/vault substitution from
  tests-disabled native factory selection and separate target-native
  transport/vault execution. Narrowed native HTTPS, proxy, response-limit,
  secret-retention, and idle-listener claims to their directly observed
  surfaces.
- Extended the public tests-disabled self-test to cover the complete production
  profile shape. It now requires exactly one visible CLI row by default and
  both GitHub.com/GHES rows when `--ghes-host` is supplied, with exact host,
  account, diagnostic, identity-update, and diagnostic-update evidence for
  each selection.
- Confirmed a clean-start CLI row performs one bounded noncredential profile
  resolution. Existing host lifecycle coverage proves a native-to-CLI switch
  may first exact-delete only the predecessor native credential binding.
  Neither path enrolls or retrieves a CLI credential, creates or invokes a
  transport factory, performs wire/capability or CLI-specific work, or starts
  a process or descendant.
- Confirmed the tests-disabled Release executable composes native HTTPS only
  and omits dormant CLI transport, configuration, request-compiler, and runner
  symbols. The retained broker remains test scaffolding for RFC-BPR-0015.
- Kept pinned Linux results attached to their exact BPR-0029/BPR-0030
  snapshots. Windows target-native execution remains open in BPR-0036 and is
  nonblocking for all non-Windows work and later RFCs.

## Evidence

- The BPR-0032 macOS `dev` configure/build/test matrix passed 46/46, and the
  `core-only` matrix passed 17/17.
- The focused AddressSanitizer/UndefinedBehaviorSanitizer security matrix
  passed 16/16, including both public CLI cases, native HTTPS, host/provider,
  credential, profile/capability, Qt sensitive-input, and read-contract
  surfaces.
- `application_public_path` reported `CLI_UNAVAILABLE_COUNT:1`;
  `application_public_path_ghes_cli_unavailable` reported
  `CLI_UNAVAILABLE_COUNT:2`; the tests-disabled Release composition contract
  independently ran and accepted both exact counts while rejecting dormant
  CLI execution bytes/symbols.
- The macOS Keychain setup/lifecycle/cleanup sequence passed 3/3. A separate
  service-scoped `security find-generic-password` lookup reported
  `MACOS_KEYCHAIN_INTEGRATION_ITEM_ABSENT`.
- Native HTTPS contract/fixture, normalized GitHub read/provider/profile/
  capability, host lifecycle, architecture-boundary, fixture-security,
  binary-dependency, and runtime-surface checks passed. The native fixture
  directly covers query-free same-authority redirects, cross-authority and
  query-bearing rejection, certificate failure, proxy ordering, cancellation,
  timeout, response-body, and per-field-header behavior; other documented
  bounds retain their stated source/contract-only limitations.
- Documentation validation, the original pack validator, secret-pattern,
  private-key and embedded-credential scans, tracked/on-disk GitHub Actions
  workflow absence, and `git diff --check` passed without retaining a
  credential, private repository response, or transient audit artifact.
