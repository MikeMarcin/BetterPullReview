---
schema: "implement-rfc.card.v1"
id: "BPR-0082"
title: "Simplify the completed RFC-BPR-0004 change set"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0081"]
---

# Simplify the completed RFC-BPR-0004 change set

Run the repository code-simplifier or a conservative focused pass over the
complete RFC-BPR-0004 range. Remove duplicated policy/mappings, needless
serialization or allocations, avoidable scans, stale comments, and
overcomplicated Qt plumbing while preserving stable identities, fail-closed
authority, measured-axis mathematics, and direct evidence.

Commit coherent simplifications separately or record a reviewed no-change
result, then rerun every affected focused and composed interaction gate.

## Implemented simplifications

- Change groups are built during discovery instead of retaining and replaying
  an intermediate range vector.
- Intraline edits are appended in canonical pair order instead of retaining
  nested per-pair vectors and flattening them in a second pass.
- A canonical-mapping detail builder lets `detail_for_line` perform one
  semantic lookup while the public mapping overload retains fail-closed
  validation.
- The application host reuses its context-to-job search for stop and erase.
- Link-map selection/hover identity matching is shared, and redundant
  workbench context and cleared-selection assignments are gone.

The pass intentionally retains the two borrowed temporary-index adapters:
centralizing them would require a private lifetime-bearing abstraction larger
than the duplicate call sites. Their non-owning alias remains a documented
future-lifetime risk if a caller ever retains it; all current callers consume
it synchronously. The pass also retains the public measured-axis validation
route rather than adding a privileged prevalidated path that would complicate
or weaken fail-closed authority.

## Verification

- The five affected core/application unit targets pass 5/5 in both the
  core-only and ASan/UBSan build trees.
- The selected link-map/workbench scale and application composition cases pass
  8/8 serially with `QT_QPA_PLATFORM=offscreen`; the final
  post-simplification composition/shutdown replay passes 2/2 under the same
  bounded conditions.
- Strict touched-file clang-tidy, repository formatting, documentation,
  immutable-RFC, workflow-absence, and diff-hygiene gates pass.
- The independent mechanical replay passed all five focused tests and launched
  no Qt, GUI, end-to-end, or application test. Its pre/post worktree state was
  identical.
- Independent closeout review found no blocking finding, verification gap, or
  unsupported behavior suspicion after exact command/build/artifact provenance
  was added to the RFC ledger.

The owner-activated target-native System-theme transition and final rendered
two-direction interaction session remain assigned to BPR-0083.
