---
id: BPR-0256
title: Build the attended one-shot live application lane
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0255]
---

# Build the attended one-shot live application lane

BPR-0235 established one default-off designated mutation exception around a
fixed harmless canary. BPR-0255 made the shipped product mutation path
asynchronous, exactly bound, and denied by default because production has no
execution-grant issuer. The remaining portable boundary is an attended
bootstrap that consumes one digest-bound operator approval and can authorize
one exact request in a tests-disabled shipped application.

## Required final state

- The existing canary runner/gate remains a hermetic regression surface. The
  sole opt-in `live_review_mutation_designated_target` command instead names an
  absolute, freshly built `BUILD_TESTING=OFF` shipped application and its
  attended bootstrap. It never launches the tests-enabled fixture app.
- The shipped app accepts one production-only attended bootstrap authority. It
  validates an absolute approval artifact using retained native file/directory
  identity, exact owner-private permissions, bounded bytes, exact schema and
  digest, and atomic one-time consumption without reopening a mutable path.
- Before the app can mint an execution authority, it performs an unauthenticated
  redirect-free public observation and matches every compiled designated
  `MikeMarcin/BPR-Test#1` repository/PR/base/head field. Failure, substitution,
  replay, timeout, malformed response, or identity drift leaves no grant.
- Approval and credential authority remain separate. The approval contains no
  token and grants only permission for one exact publication attempt. Any
  write-scoped credential is entered through the existing sensitive Qt prompt
  after launch; it never enters CTest configuration, argv, environment, the
  approval artifact, IPC, fixtures, diagnostics, or logs.
- The bootstrap carrier is retained privately until the async mutation host has
  validated the exact application session, workbench generation, designated
  target, authenticated acting identity, comparison, file, right-side semantic
  anchor, resolved path, and current-head commit. Only then may one atomic
  consume succeed. Invalid requests do not consume it; the first exact request
  does; every later request refuses before an effect.
- Application invalidation, target/head/actor drift, cancellation, timeout, and
  shutdown revoke unused authority. A transmitted indeterminate POST remains
  guarded by the existing retry identity and server reconciliation; the
  bootstrap never submits directly or bypasses the async host.
- Default configured manifests retain zero live exceptions. The explicit
  opt-in manifest has exactly one test, exactly one `bpr_live_mutation` label,
  the fixed shipped-app/bootstrap command, no CTest pass/skip/disable override,
  no credential environment, a bounded attended timeout, and fail-closed
  missing-path/configuration behavior.
- A digest-bound operator packet records the exact immutable approval subject,
  app/source/build identity, designated PR/head, intended one-comment scope,
  credential-handling instructions, abort conditions, and required GitHub web
  inspection. This card prepares the packet but does not execute or approve it.

## Non-goals

- Do not execute the live lane, access a credential, submit a comment, or claim
  GitHub/web-UI evidence.
- Do not replace or weaken the BPR-0235 canary/fork safety contracts; they remain
  regression evidence, not application-path evidence.
- Do not pass a nonce/capability over a forgeable child pipe, argv, environment,
  or persisted application setting.
- Do not claim target-native Linux/Windows evidence; those remain deferred until
  the end of portable work by product-owner instruction.
- Do not close BPR-0142, BPR-0235, or RFC-BPR-0008.

## Verification

- Hermetic bootstrap tests cover absent/malformed/unsafe/replayed/substituted
  approval, every tuple mismatch, bounded remote failures, atomic consumption,
  and zero grant on failure without network or credentials.
- Async-host tests prove invalid exact-request bindings do not consume the
  authority, the first valid request consumes it immediately before queueing,
  the second refuses, and cancellation/shutdown revoke it.
- Production syntax/source/symbol cuts prove there is one issuer, no fixture
  issuer, no direct provider/coordinator/network/credential bypass, and no
  ambient authorization path.
- Default and opt-in manifest tests prove zero versus exactly one live exception
  and the exact tests-disabled shipped-app command. Hostile PASS/WILL_FAIL/SKIP,
  alternate executable, fixture app, credential environment, and second-lane
  graphs fail.
- Existing designated canary/gate, async host, live isolation, architecture,
  documentation, scoped format/tidy, and `git diff --check` gates remain green.
