---
schema: "implement-rfc.card.v1"
id: "BPR-0922"
title: "Fix source files refusing to open with unavailable exact comparison"
status: "Backlog"
rfcs: ["RFC-BPR-0003", "RFC-BPR-0015", "RFC-BPR-0029"]
depends_on: []
priority: "P1"
---

# Fix source files refusing to open with unavailable exact comparison

## Owner feedback

“I cant open any source code they all say ‘The selected file changed before
it could be loaded. Review history could not be updated. The exact comparison
is unavailable.’”

Reported after the first loaded PR, following publication of 0.1.3. The exact
running build, PR and environment have not been independently captured.

## Scope and investigation

This blocks the owner's core source-review workflow after successfully opening
the PR. Preserve the three messages as the reported sequence; do not presume
a real file change. Trace selected-file identity, accepted comparison, history
update, Git object acquisition and CLI/native capability refusal. Check actual
CLI/Git versions and qualified comparison support if relevant, without assuming
which versions the owner ran.

Use a synthetic GHES PR through the composed CLI path with stable head/base and
multiple files. Separate a stale-selection guard from unavailable objects,
history failure, provider capability limits or projection errors. Coordinate
[[BPR-0921]]; neither a diagnostic wording change nor bypassing the exactness
guard is a fix for inability to read source.

Starting points: `src/adapters/qt/src/workbench_window.cpp`,
`src/adapters/qt/src/epoch_comparison_control.cpp`, and the application history,
comparison and file-loading hosts.

## Acceptance

- Establish the failing boundary with a composed reproducer and exact request/comparison identity; distinguish observation from root-cause inference.
- Supported stable comparisons open source for multiple files and render the intended base/head content after initial PR load.
- Real head/selection replacement still rejects stale content; unsupported or missing comparisons explain the actual reason and provide an actionable recovery state.
- Verify history and source loading together on recorded GHES/CLI fixtures; preserve credentials/private repository content outside committed fixtures and logs.

## Status

Not started. This intake records feedback and investigation/repair scope only;
no reproduction, root cause, runtime fix or live-service verification is claimed.
