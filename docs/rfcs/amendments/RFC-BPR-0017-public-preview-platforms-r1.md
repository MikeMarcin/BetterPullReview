# RFC-BPR-0017 public-preview platforms amendment r1

Mandate `public-preview-platforms-r1`, 2026-10-19: “Enable public preview on
Windows and Linux, and run the native verification needed to support it.”
Authorized for implementation by that explicit request. Baseline `12ef1db532f78f25ceaa821a5a7bb2fcda001aa2`.

## Grounded change

[RFC-BPR-0017](../RFC-BPR-0017-unauthenticated-public-review-read.md) closed
its original macOS scope with Linux/Windows deliberately unavailable. The
[composition](../../../src/app/src/github_application.cpp) still selects that
policy. The existing [native platform course](../../../scripts/verify_rfc0017_platform_course.py)
and bounded live-candidate mode already exercise the shared anonymous path.
The immutable original issue plan and completed platform cards retain their
historical meaning. This amendment adds one follow-up acceptance unit,
BPR-1664, without reopening or erasing that evidence.

## Proposed behavior and boundaries

Enable the existing GitHub.com Public preview composition on native Windows
and Linux, preserving macOS. Activation sends no request. Opening a public
pull request uses the existing anonymous native HTTPS provider and public Git
comparison path. GHES refuses before traffic. Existing unavailable viewer,
GraphQL, viewed state, thread resolution, publishing and account-relative
capabilities stay unavailable. No vault, authorization-header, CLI sign-in,
credential fallback, new provider, network permission or UI design is added.

## Acceptance

- PLATFORM-1: native Windows and Linux tests-disabled artifacts independently
  report enabled GitHub.com activation and GHES refusal with zero requests,
  header, vault and sign-in effects.
- PLATFORM-2: each target passes the existing applicable anonymous composition,
  authority, provider/transport, Git/cache, SQLite and Qt course selectors;
  a bounded production-factory read of MikeMarcin/BPR-Test#1 observes a public
  snapshot, no credential effects and at most fifteen forwarded REST requests.
- PLATFORM-3: macOS selection stays enabled; public documentation states the
  actual verified platforms, documentation and focused composition gates pass,
  and the exact candidate/evidence and remaining qualification limits are recorded.

These criteria authorize no release, push or publication. Native here includes
WSL2 Linux per the station policy; it does not establish a clean Ubuntu desktop,
Windows installation/vault/accessibility qualification, or general release readiness.
No human visual judgement is added: the existing presentation is unchanged.

## Reconciliation and next action

One issue-scoped milestone owns the added obligations. Its plan does not replace
the original RFC's legacy plan or change any protected attended candidate.
Reconcile station writers, then implement the selection/expectations and run the
native courses. Native acceptance remains pending until supplied; an unavailable
station is recorded as a blocker, never a passing platform.

## Execution reconciliation: diagnostic budget

The first Linux read of PR 1 forwarded nine successful first pages but could
not finish. Source inspection identifies the subsequent commit-status read
added at `34f62d7e3`; the diagnostic's historical nine-request ceiling predates
it. PR metadata and review counts exclude target pagination as the cause.
The production anonymous catalog already admits twelve REST kinds, including
statuses, timeline and merge-base reads. Raise only the test-only diagnostic
ceiling to twelve, retain first-error retry suppression and a separate literal
self-check of twelve forwards/thirteenth refusal across shared wrappers.
The owner mandate permits this bounded verification repair. It adds no
production operation or authority; PR 1 remains the designated public target.
Preflight requires at least thirty anonymous requests remaining before a run.


The twelve-request repair also stopped before the provider's final audit.
The complete single-page control flow is ten content/status reads, one timeline,
one exact merge base, one branch-tip merge base for an open PR, at most one
conditional ancestry read, and one fresh PR confirmation: fifteen sends maximum
(`github_provider.cpp`, BPR-1560/BPR-1572). Closed PRs omit branch-tip/ancestry;
PR 1's unchanged base uses fourteen. The diagnostic ceiling is therefore fifteen,
with independent synthetic coverage of that complete worst-case operation
sequence plus a fifteen-page stream, shared replacement transports and refusal
of a sixteenth send. This is the final derived bound, not a catalog count.
Pagination may exceed it and is deliberately refused. The thirty-request
preflight floor remains; wait for the real server reset when it is not met.
