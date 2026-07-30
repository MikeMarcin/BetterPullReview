---
id: BPR-0027
title: Implement GitHub application-host orchestration
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0018, BPR-0019, BPR-0020, BPR-0021, BPR-0022, BPR-0023, BPR-0024, BPR-0025, BPR-0026]
completed: 2026-07-30
---

# Implement GitHub application-host orchestration

Execute typed probe and read requests through narrow ports; marshal normalized
completions through the serialized decision lane; own cancellation, retry,
rate-limit, session, cache, and lifecycle behavior; and invalidate
identity-sensitive state on host or account changes.

Production session establishment must bind three independently validated
facts in order: a validated host profile, the authenticated account observed
from the strict `/user` contract, and normalized capability-probe evidence
for that exact profile revision/host/account/server/probe-contract key. The
BPR-0019 recorded normalizer's caller-supplied context is fixture scaffolding;
the production host must not treat caller-provided host, account, or
capability values as observations or use them to launder provider data.

Native enrollment must solve account discovery without weakening the vault
key. Treat submitted material as transient host-owned input, use it only
through the credential-bearing transport to observe `/user`, then store it
under that exact observed account authority. A login string or caller-claimed
account cannot select a vault entry. Profile replacement or identity
invalidation must delete the previous exact authority so a revision-keyed
credential cannot become an orphaned secret.

Production GraphQL reads must be compiled from that exact capability
observation. Omit unsupported schema fields, preserve review-thread reads when
only resolve or unresolve is unsupported, and normalize omitted per-file
viewed state as `unavailable` and omitted per-thread target permission as a
safe unavailable/false fact. Absence caused by capability-shaped selection
must never be staged as an authoritative empty collection. The BPR-0019
single-query recorded facade covers only the fully supported selected
GitHub.com/GHES 3.18 contract and is not the production fallback for a
partially supported schema.

Fake-port tests must reorder completions, switch identity in flight, exhaust
bounded retries, cancel jobs, and prove workers never touch core or Qt state.

The production host must exhaust every REST `Link` continuation and GraphQL
`pageInfo` cursor under the aggregate record and byte ceilings before staging
one snapshot. It validates raw status, content type, headers, pagination
envelopes, cursor progress, and repository authority, projects only the
allowlisted normalized fields, and rejects unknown completeness, repeated or
missing cursors, partial pages, excess pages, and truncated totals atomically.
The one-page BPR-0019 recorded facade is test scaffolding, not evidence that a
larger live pull request loaded completely.

## Completed implementation surface

- The production provider owns bounded REST and GraphQL page exhaustion,
  capability-shaped selection, normalized snapshot assembly, exact
  repository/head/count correlation, cancellation, and typed failure
  classification.
- GitHub.com requires the applicable exact `core`, `search`, or `graphql`
  rate tuple. GHES may omit a whole tuple; partial, malformed, duplicate,
  unknown, or wrong-resource metadata fails closed. The provider and host
  accumulate independent observations across pages and retries and preserve
  an exact causal record for primary, secondary, and structured GraphQL rate
  limits.
- The application host owns the capability cache and accepts no
  caller-populated cache. Only a successful exact provider probe may stage and
  commit capability evidence.
- Native enrollment stores only after strict bootstrap `/user` observes the
  numeric account. Resume accepts only a profile and uses a noncopyable,
  host-owned catalog entry established by that observation. Profile
  replacement, account mismatch, transport replacement, absence, and explicit
  invalidation exact-delete or forget the selected authority.
- Projection exceptions are observational and latched. Reentrant public
  ingress is processed in bounded FIFO generations; shutdown has terminal
  priority; mandatory effects and cleanup continue after projection failure.
- Provider and vault ports are nonblocking, yield exactly one terminal for an
  accepted submission, and marshal it through a decision executor that stays
  live until host quiescence. Duplicate, stale, malformed, reordered, and late
  terminals cannot rewrite accepted state or credential lifecycle.

The credential binding catalog proves only same-process host recreation when
composition shares the catalog across host instances. Normal shutdown
intentionally preserves an accepted native credential, but the current vault
cannot enumerate or reconstruct its account-keyed authority after process
restart. BPR-0029 must add trusted durable rehydration or exact shutdown
deletion before native persistence is composed; BPR-0027 makes no
process-restart or public persistence claim.

Windows target-native execution remains BPR-0036 and is nonblocking for this
slice. GitHub Actions has no build, test, package, release, or verification
role.

## Evidence

- The final macOS arm64 focused host, provider, pager, recorded-contract,
  capability, and native-wire matrix passed 9/9.
- A fresh Qt-enabled `dev` configure, clean build, and complete test run
  passed 35/35. A fresh Qt-free `core-only` configure, clean build, and
  complete test run passed 17/17.
- The focused application-host, pager, and provider targets passed 3/3 under
  AddressSanitizer and UndefinedBehaviorSanitizer.
- A tests-disabled Release configure and clean build passed. Binary inspection
  found no test-only native-transport factory, the production target had no
  `BPR_BUILD_TESTS` definition, the binary-dependency and runtime-surface
  gates passed, and the public application self-test reported
  `PUBLIC_PATH_OK`. This does not claim the BPR-0029 provider composition.
- The documentation validator, immutable RFC source-pack validator, and
  `git diff --check` passed. Tracked and on-disk scans found no GitHub Actions
  workflow.
- Independent host and skeptical pre-freeze reviews found no P0/P1 defect.
  The simplification pass preserved the frozen behavior while consolidating
  credential selection and delete-failure handling.
- The final Ubuntu 26.04 x86_64 full-system QEMU build compiled the selected
  production and test targets with GCC 15.2 under the repository
  warnings-as-errors policy. The focused host, provider, pager,
  recorded-contract, capability, and native-wire matrix passed 9/9 in
  39.56 seconds under Qt 6.10.2. This target-native pass also found and closed
  GCC aggregate initialization and test-temporary lifetime defects before
  commit.
- Windows 10/11 x64 target-native execution remains openly deferred to
  BPR-0036; it blocks only affected Windows claims and final RFC-BPR-0002
  Auto-Verified closeout.
