---
id: BPR-0448
title: Retain capsule and inventory identity through verification and launch
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0352, BPR-0354]
---

# Retain capsule and inventory identity through verification and launch

BPR-0256's v005 capability-critical review falsified two completed provenance
claims in the production supervisor even though every registered gate passed.

First, `run_static_verifier` finishes before the capsule root is opened and
captured. A same-user substitution can therefore replace verified root A with
root B before capture; the final `unchanged()` check compares B with B and
accepts it. The source comment claiming the descriptor was retained before the
verifier ran is false. Existing `macos_capsule_identity` coverage starts at
capture and cannot observe this verifier-to-capture interval.

Second, the Python verifier opens the inventory pathname and the supervisor
later reopens that pathname to compute the digest handed to the application.
Inventory A can authorize verification of one capsule and then be replaced by
operator-approved inventory B before the subject-bound digest is computed.

## Required final state

- The supervisor retains one capsule-root identity before verification, the
  verifier consumes that identity, and capture/revalidation/launch cannot
  silently resolve a replacement root.
- It retains and validates one inventory object before verification. The
  verifier and application-bound SHA-256 consume that same object without a
  second pathname open.
- A production-shaped deterministic barrier replaces whole root A with B after
  static-verifier success while preserving the top-level executable identity
  and changing a nested resource; launch refuses before spawn or `SIGCONT`.
- Deterministic inventory A-to-B substitutions during and after verification
  either refuse before launch or preserve A's digest so B's approval cannot
  match.
- The focused supervisor/capsule, authority/composition, isolation,
  documentation, formatting, and hygiene gates pass on the real macOS graph.

## Non-goals

- No credential, approval materialization, public observation, live supervisor
  launch, GitHub request, or remote mutation.
- No v006 regeneration. BPR-0256 and BPR-0449 must be resolved before a
  successor packet can be built.
- Do not weaken the fixed target, signing, replay, one-use, cancellation, or
  recovery boundaries to make a test pass.

## Claim — 2026-08-24

This macOS session owns the production supervisor identity continuity and its
production-shaped tests. It does not own the separate image-to-runtime command
topology recorded by BPR-0449.
