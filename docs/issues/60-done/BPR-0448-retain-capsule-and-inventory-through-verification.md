---
schema: "implement-rfc.card.v1"
id: "BPR-0448"
title: "Retain capsule and inventory identity through verification and launch"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0352","BPR-0354"]
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

## Implemented and verified — 2026-08-24

Commit `20e65ac` closes both falsified identity intervals. The supervisor opens
one owner-private capsule root, inventory, and digest-bound verifier before
static verification. It gives Python the exact retained verifier and inventory
bytes over anonymous streams, makes the verifier and application resolve from
the retained root descriptor, and computes the application-bound inventory
SHA-256 from those same retained bytes.

The production verifier and application now share one launch controller. That
controller accepts only the fixed relative application path, revalidates before
`posix_spawn` and immediately before `SIGCONT`, and kills and reaps a suspended
child when any post-spawn check or supervisor-channel restoration fails. The
registered architecture contracts forbid moving spawn/resume back into the
supervisor or recovering mutable capsule/inventory pathname authority.

The production-shaped macOS course invokes that same controller, not a
test-owned surrogate. Its deterministic barriers prove:

- whole-root A-to-B replacement immediately before the real verifier spawn
  still verifies retained A, then fails continuity;
- inventory pathname replacement and same-inode writes cannot change the bytes
  verified or the digest bound to the application;
- whole-root and nested-resource replacement before application spawn produce
  zero spawn and zero `SIGCONT`;
- the same replacements after one successful spawn produce one kill/reap and
  zero `SIGCONT`;
- the unchanged course produces exactly one spawn and one `SIGCONT`; and
- an absolute application path and failed post-spawn restoration both fail
  closed.

### Evidence

| Claim | Evidence | Result |
|---|---|---|
| Real production identity/controller course | `ctest --test-dir build/dev --output-on-failure -R '^macos_supervisor_identity_continuity$'` | Passed on the final controller graph |
| Capsule and retained-stream contract | `python3 tests/scripts/designated_live_macos_capsule_tests.py` | 32/32 passed |
| Architecture cuts and hostile mutations | `python3 tests/scripts/architecture_boundary_tests.py` | 126/126 passed |
| Authority, composition, capsule, provenance, isolation, and architecture selection | final exact-commit selector over the twelve affected tests | Eleven passed together; the final round-trip process exceeded its 40,000 ms internal budget by 12 ms under concurrent review load, then passed alone in 27.07 s. Independent review's scoped final matrix passed 7/7 in 226.20 s |
| Source inventory and capability digest | exact application source-tree and production-content locks, plus the attended Release source manifest | Both identity files and both shared-controller files are bound |
| Fresh shipped graph | offline tests-disabled Release `designated_live_macos_capsule` target | Passed at source digest `a6ff7c0cb2744ab3c6e14c788566ce2faad8f88bb2389f441a64c68ed3b0b43e`; embedded `--verify-only` and `--probe-launch` both exited 0 |
| Verifier trust | source hash and compiled supervisor literal | `6380ab34703641ee2d7ed28772f84efdf7e90bd324287cdfaf683a97d1e33ab6` in source, capsule, and supervisor |
| Formatting, changed-file analysis, documentation, and hygiene | format check, touched-file clang-tidy, both documentation commands, and `git diff --check` | Passed. Repository-wide tidy still reports six pre-existing diagnostics in untouched files |

No attended launch, approval, credential, public observation, network request,
or remote mutation was performed. Post-`SIGCONT` resource immutability remains
the separate BPR-0449 responsibility.

## Independent capability-critical review — 2026-08-24

The first review of candidate `20ea90d` returned blocking because the hostile
tests reimplemented verifier/spawn behavior rather than invoking the production
controller. It also found a suspended-child recovery leak when provenance
restoration failed. The shared controller, phase-specific effect counts,
absolute-path cuts, source locks, and kill/reap recovery above repaired those
findings.

Fresh `rfc-critical-reviewer` adjudication of exact range
`30bf008..20e65ac` returned `REVIEW STATUS: CLEAN`: no blocking or high-risk
non-blocking finding, no scoped verification failure, and no unsupported
suspicion. The reviewer independently ran seven decisive selectors, checked the
fresh Release build selection and exact embedded hashes, and confirmed that
BPR-0449—not this card—still owns post-resume read-only-image containment.
