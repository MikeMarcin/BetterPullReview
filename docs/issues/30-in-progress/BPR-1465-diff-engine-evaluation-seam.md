---
schema: "implement-rfc.card.v1"
id: "BPR-1465"
title: "Let the diff engine take supplied primary matches and evaluation components"
status: "In Progress"
rfcs: ["RFC-BPR-0082"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
---

# BPR-1465: Let the diff engine take supplied primary matches and evaluation components

Part of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). The evaluation may add a core entry point that takes a supplied
primary match list as a bounded observation and runs the remaining first-party
pipeline. This card adds that seam, and the narrower hooks the pairing,
intraline and hunk-sliding candidates need, without changing any existing entry
point. Whether the seam ships is part of adoption, not this card.

## Acceptance

- **AC1:** New core overloads compute a file or material comparison with a
  supplied primary match list, or with evaluation components that supply primary
  matches, pair scores, intraline scalar matches or a post-pass over the primary
  matches. The core validates every supplied value (strictly increasing, in
  range, normalized-equal, scores in range) before assembly, and refuses a
  comparison whose supplied values fail. A component supplies matches, scores or
  spans only, never identities or geometry. A component may count steps through
  the existing step meter, so a stop is honoured inside it.
- **AC2:** The existing entry points produce identical results (RFC AC07): the
  existing diff suites pass unchanged, a corpus-wide digest of their outcomes
  equals the digest at RFC_BASE_SHA, and `core-only` and both
  architecture-boundary checks pass.
- **AC3:** Focused core tests cover the seam: a supplied list equal to the
  baseline's own matches reproduces its alignment and intraline edits; invalid
  supplied lists and scores are refused; and a metered component is stopped
  within its check interval.
