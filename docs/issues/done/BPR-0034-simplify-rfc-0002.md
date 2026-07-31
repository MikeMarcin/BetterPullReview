---
id: BPR-0034
title: Simplify the RFC-BPR-0002 change set
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0030, BPR-0031, BPR-0032, BPR-0033]
completed: 2026-07-30
---

# Simplify the RFC-BPR-0002 change set

Run the code-simplifier skill over the completed RFC slice. Remove duplicated
policy, provider leakage, broad service access, unnecessary serialization,
whole-state refreshes, unsafe convenience paths, stale comments, and avoidable
complexity without changing behavior, then replay focused verification.

## Result

- Centralized the core's identity-generation advance and complete
  identity-scope clear while preserving profile activation/deactivation
  outputs and request ordering.
- Centralized capability-cache slot selection while deliberately recomputing
  it at both stage and commit; no iterator or decision is retained across the
  acceptance boundary.
- Unified the Qt status-plus-visible-diagnostic text used by the status
  banner's accessible description and deduplicated polite announcement.
- Grouped the public CLI-unavailable automation's expected identity and
  counters, consolidated its failure-to-shutdown path, and deduplicated the
  Release default/GHES self-test assertions without weakening exact counts.
- Confirmed through architecture and model tests that the slice has no
  provider leakage, broad service access, unnecessary serialization, or
  whole-model reset to remove.
- Left native-vault, bounded-JSON-parser, asynchronous-host, and native-HTTPS
  security flows explicit. Consolidating them would be a separately scoped
  cross-platform behavior change, not low-risk closeout cleanup, and direct
  Windows execution remains BPR-0036.

## Evidence

- The complete current shared-worktree matrices passed 47/47 for `dev` and
  18/18 for `core-only`. One project-source-dump test in each belongs to
  unrelated in-progress user work excluded from this card; the RFC-owned
  matrices remain 46/46 and 17/17.
- The focused normal matrix passed 8/8: core policy, capability cache, all
  three Qt scales, both CLI-unavailable public paths, and the tests-disabled
  Release composition contract.
- A fresh AddressSanitizer/UndefinedBehaviorSanitizer build passed the seven
  changed-path core, cache, three-scale Qt, and one-row/two-row CLI tests.
- Python compilation, documentation, original-pack, architecture,
  tracked/on-disk GitHub Actions workflow-absence, and `git diff --check`
  gates passed.
