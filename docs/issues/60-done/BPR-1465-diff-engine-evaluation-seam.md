---
schema: "implement-rfc.card.v1"
id: "BPR-1465"
title: "Let the diff engine take supplied primary matches and evaluation components"
status: "Done"
rfcs: ["RFC-BPR-0082"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"094df23cb95fc55829c734564dbe212e3c5d1411","evidence":{"kind":"observation","detail":"src/core/include/bpr/core/diff_compute_components.hpp adds compute_file_diff/compute_material_diff overloads taking diff_components (primary matches, slide post-pass, pair scores, intraline scalar matches) or a diff_primary_observation. The engine assigns line classes, validates strictly increasing, in-range, class-equal matches and scores <= 1000, and refuses (invalid_alignment) otherwise; components return only matches/scores; ticks go through diff_step_meter.","path":"docs/issues/60-done/BPR-1465-diff-engine-evaluation-seam.md"}},{"criterion":"AC2","outcome":"pass","revision":"094df23cb95fc55829c734564dbe212e3c5d1411","evidence":{"kind":"observation","detail":"core-only preset: 269/273 ctest pass; the 4 others are worktree environment (board_model import for 3 documentation tests) and the pre-format run of coding_format_check, which passes after formatting. core_dependency_boundary, core_boundary_contract, architecture_boundaries, architecture_boundary_contract, git_protocol_boundary and every core_diff suite pass. tool_diff_baseline_digest over 1,162 comparisons (generated option and limit matrix plus the 86-pair evaluation corpus) is byte-identical between the core at RFC_BASE_SHA cc9e9376e and this revision, both in core-only and in the evaluation build with the hook compiled in.","path":"docs/issues/60-done/BPR-1465-diff-engine-evaluation-seam.md"}},{"criterion":"AC3","outcome":"pass","revision":"094df23cb95fc55829c734564dbe212e3c5d1411","evidence":{"kind":"command","detail":"core_diff_compute_components (tests/core/diff_compute_components_tests.cpp) passes: no-parts and declined components are identical to the entry points; the baseline's own matches supplied reproduce alignment and intraline edits (file and material); classes follow normalization; out-of-order, out-of-range, unequal matches, out-of-range or misshapen scores and unequal intraline matches are refused; a post-pass decides equal lines; a metered component pauses within one check interval, resumes only with the same components.","path":"docs/issues/60-done/BPR-1465-diff-engine-evaluation-seam.md"}}]
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
