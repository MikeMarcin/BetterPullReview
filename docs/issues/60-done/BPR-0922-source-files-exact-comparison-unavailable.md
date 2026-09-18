---
schema: "implement-rfc.card.v1"
id: "BPR-0922"
title: "Fix source files refusing to open with unavailable exact comparison"
status: "Done"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0003", "RFC-BPR-0015", "RFC-BPR-0029"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
acceptance: [{"criterion":"Establish the failing boundary with a composed reproducer and exact request/comparison identity; distinguish observation from root-cause inference.","outcome":"pass","revision":"5b95485c9432cd365e8cedffc3aee0ea7d52b164","evidence":{"kind":"artifact","detail":"milestone-source-update-review-entry-evidence.md records the CLI-typed GHES smart-acquisition refusal, exact evidence limits, and no claim about the owner's unobserved live cause."}},{"criterion":"Supported stable comparisons open source for multiple files and render the intended base/head content after initial PR load.","outcome":"pass","revision":"5b95485c9432cd365e8cedffc3aee0ea7d52b164","evidence":{"kind":"command","detail":"application_setup_destination_public_path passed and rendered exact base/head first lines for review.cpp and secondary.cpp from the recorded Git packs."}},{"criterion":"Real head/selection replacement still rejects stale content; unsupported or missing comparisons explain the actual reason and provide an actionable recovery state.","outcome":"pass","revision":"5b95485c9432cd365e8cedffc3aee0ea7d52b164","evidence":{"kind":"command","detail":"application_workbench_host and qt_workbench_window_scale_100 passed the unavailable-context and genuine replacement-stale distinctions; the composed refusal retained no source and offered Refresh."}},{"criterion":"Verify history and source loading together on recorded GHES/CLI fixtures; preserve credentials/private repository content outside committed fixtures and logs.","outcome":"pass","revision":"5b95485c9432cd365e8cedffc3aee0ea7d52b164","evidence":{"kind":"observation","detail":"The composed course selected a discovered github_cli profile on the recorded GHES contract, retained two epochs, used no vault/bootstrap path, and contacted no live target."}}]
---

# Fix source files refusing to open with unavailable exact comparison

This work belongs to [RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md),
criterion AC-05. Earlier RFC references retain their governing contracts.

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

Accepted at source revision `5b95485c9432cd365e8cedffc3aee0ea7d52b164`.
The composed CLI-typed GHES course distinguishes unavailable comparison from
genuine stale replacement, retains no source on refusal, and renders both
recorded files on the supported path. The owner's live cause remains
unobserved; evidence and limitations are recorded in the RFC plan directory.
