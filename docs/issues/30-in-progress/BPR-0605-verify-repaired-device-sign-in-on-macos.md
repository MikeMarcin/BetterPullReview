---
id: BPR-0605
title: Verify repaired device sign-in on macOS
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0559]
---

# Verify repaired device sign-in on macOS

The owner asked to continue after the verified BPR-0559 repair. Observe one
sign-in-only course with the changed ordinary Release executable, using the
existing sanitized logger. This is live follow-up evidence, not a reopening of
BPR-0559's completed offline acceptance or permission to publish a review.

## Acceptance

- Receive the exact changed-build approval, verify packet/executable identity,
  and launch once using the packet's environment and credential-handling scope.
- Observe the terminal connection result for the intended MikeMarcin account
  and read only the new run's sanitized local diagnostic records. Record whether
  capability discovery and session establishment succeed; do not infer success
  from browser authorization alone.
- Explicitly Sign out, observe disconnected status, and quit normally. Record
  any cleanup failure. No PR opening, publication, second launch or retry.
- Retain a minimal sanitized result; keep the complete local log out of Git.
  If sign-in still fails, record the specific evidence and create bounded repair
  work without weakening validation or claiming successful live acceptance.

## Prepared checkpoint — 2026-09-05

The [v005 packet](../../verification/reviews/device-session-bootstrap-diagnostic-v005/operator-packet.md)
has SHA-256 `c0ba957736397bde0a87de55b21b483df4627b8a9a2ce8c98558b06736550269`.
It binds the existing 11,066,176-byte tests-disabled Release executable
`3dd5c3bb794ae0ef6138e4921b21f4bde8dd7131874d8f5f01609464d5913a16`,
produced from `b14a69d0`. The current `0aa57ebe` adds documentation only.
Hash, length and Release/tests-disabled configuration were read directly;
process-name checks found no application instance. No new launch or approval
has occurred. Prior one-use authority is consumed.

Reuse BPR-0559's passing source/fixture/build evidence: inputs have not changed.
Documentation structure and its 62 tests pass; diff hygiene passes. No new
worktree, build root or inspection log was created. Retain the ordinary Release
build for this pending consumer and preserve the owner's existing local log.

## Exact v005 approval received

The owner supplied the exact [operator response](../../verification/reviews/device-session-bootstrap-diagnostic-v005/operator-response.md). Packet and executable identities match and no app instance was running. Resume this one approved launch, then collect its sanitized terminal result and explicitly Sign out and quit. No PR or publication is authorized.
