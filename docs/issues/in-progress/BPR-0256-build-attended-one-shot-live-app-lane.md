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
  absolute signed-capsule supervisor and inventory around a freshly built
  `BUILD_TESTING=OFF` shipped application and its attended bootstrap. It never
  launches the tests-enabled fixture app or the GUI directly.
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

## Unattended implementation checkpoint

The signed-capsule prelaunch supervisor, private in-application bootstrap,
narrow GitHub-adapter public observation, private one-use host carrier, exact
request admission, production-only CLI surface, and tests-disabled shipped-app
CTest registration are implemented. Hermetic tests
exercise unsafe/malformed/replayed/substituted approvals, digest and every
target/build mismatch, observer failures, concurrent consume, invalid-before-
valid host admission, exact one-use behavior, expiry, cancellation, and
shutdown without network or credentials. Hostile syntax/source checks reject
external authority construction and redefinition of the sealed issuer.

The prior BPR-0235 runner/gate/canary sources remain unchanged and are
regression-only. Configure-time and independent manifest checks reject that
runner, direct GUI and fixture/alternate apps, alternate supervisors or
inventories, tests-enabled/non-Release producer trees, missing approval, stale
artifacts, credential environment, verdict overrides, or a second live lane.

The digest-bound operator packet is pending attended review. The live test has
not been executed; no credential or network was accessed and no mutation was
submitted. Target-native Linux/Windows evidence remains deferred by product-
owner direction, and the Windows bootstrap remains fail-closed. Accordingly
this card stays in progress and makes no BPR-0142, BPR-0235, or RFC closeout
claim.

## Session handoff — 2026-08-11

The operator ended the implementation session and requested that all current
work be committed and merged without claiming this card complete. The saved
tree contains the signed macOS capsule/supervisor, private approval bootstrap,
one-shot async-host admission, public target observer, durable publication
recovery, static live-lane registration, hostile contracts, documentation, and
the pending operator packet. Future authentication/read investigations are
preserved as draft RFC-BPR-0016 and RFC-BPR-0017; they are not acceptance
claims for this card.

Exact unfinished work remains owned here:

- Re-run `architecture_boundaries` and `architecture_boundary_contract` after
  the saved verifier update from the old `inventory` expression to the current
  dereferenced `*inventory` expression. The interrupted 16-test replay passed
  the other twelve completed BPR-0256 tests, failed those two stale-expression
  gates before the correction, and was stopped while the two composition
  contracts were still pending.
- Rebuild the signed capsule and refresh `source-manifest.txt`,
  `approval-subject.txt`, and `operator-packet.md` after any subsequent source
  edit. The currently recorded packet binds source digest
  `d8ad05b9e356b0df1177c9b276feeea2a80978da20134682ce2ce02fa8ab2791`;
  it must be treated as stale if this commit changes a bound source byte.
- Reconfigure and inspect the static opt-in manifest against the final packet,
  then run scoped format/tidy, documentation validation, `git diff --check`,
  and the complete focused matrix from the final committed tree.
- Run the capability-critical independent closeout review. The required
  read-only reorientation lane could not be created in this session because
  the runtime repeatedly returned `agent thread limit reached`; this is a
  recorded serial-fallback trigger, not a waiver of the review gate.
- Do not execute the live lane until the regenerated operator packet receives
  its exact approval statement. BPR-0142 still owns the attended publication
  and GitHub web inspection; BPR-0235 still owns target-native/default-deny
  closeout. Terminal BPR-0241 through BPR-0245 remain in backlog under their
  existing dependency cards.
