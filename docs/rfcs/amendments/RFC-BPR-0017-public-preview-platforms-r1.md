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
  snapshot, no credential effects and at most nine forwarded REST requests.
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
