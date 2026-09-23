# Implement the BetterPullReview review-tour feature

Use the installed, project-configured `implement-rfc` workflow to import and implement this pack in the current BetterPullReview checkout. Begin with `README.md`, `supporting/DECISIONS-AND-PRECEDENCE.md`, `supporting/SOURCE-AUDIT.md`, the five RFCs in `repository-overlay/docs/rfcs/`, and `MILESTONES.md` / `milestone-plan.json`.

## Import, do not reconstruct the plan

The supplied source snapshot is September 22, 2026 and contains RFCs through 0038. Proposed 0039–0043 may have collided since then. Inspect live HEAD and current ownership once; allocate/remap RFCs if necessary without overwriting any existing RFC. Allocate real issue IDs for `PACK-TOUR-01` through `PACK-TOUR-26` using BPR's current `scripts/allocate_issue_id.py`; commit its returned counter/reservation receipts. Preserve the original source archive and a complete alias/ID mapping. Do not edit existing counter files by guessing the next ID.

Import the **authored eight-milestone plan** as the default execution seed using the installed skill's actual schema/commands. Remap issue/RFC references, scope, dependency edges and documentation consistently. The pack's local validator is not a substitute for native plan/board validation. Do not discard the supplied grouping and run a new open-ended planning exercise. Record any justified live adjustment and preserve plan lineage.

Copy only the new RFC overlay files after collision checks. Retain supporting docs, examples, cards and the plan under the project's normal source-pack/plans locations and repair relative references in the live imported docs accordingly. Do not bulk overwrite current product/architecture documents with an old snapshot; apply the narrow amendments in `supporting/INTEGRATION-MAP.md`.

## Preserve these outcomes

- Complete the manual Problem → Solution → ordered tour stops → preview/publish/reopen/edit lifecycle before optional AI engineering.
- The shared tour is **normal GitHub comments** with real working links. The root index is preferably a review-summary comment; stops are ordinary inline/file/compatibility comments. No required BPR markers, private shared manifest, bot, hosted tour or install requirement for reviewers.
- Use the current real core/application/Qt/provider/store/agent/composition seams. Extend the existing summary/source-line publisher and recovery infrastructure; it is not yet evidence of inline batch staging. Keep current RFC-0036 Markdown and RFC-0037 placement rather than rebuilding or reviving retired UI.
- Stage a finite exact plan, resolve actual comment URLs, verify complete pending membership and submit that review as COMMENT. Use fixed API operations, including admitted `gh api`/fixed GraphQL where needed. Preserve account/credential/platform/one-use authority boundaries. Separate description edits and standalone fallbacks from the single-review publication guarantee.
- Reopen from GitHub deterministically without AI. Preserve website edits and conflicting local drafts. Reorder published tours without replacing existing comment identities or deleting discussions. Maintain explicit partial/unknown/recovery states.
- Optional AI is a **local editing command**. A selection limits the exact rewrite; otherwise enhance the field, or recover/draft when empty. Replace in place as one normal undoable edit. **Do not add Use suggestion / Apply / Dismiss or before-after panels.** Redo never invokes a model. Stale responses never overwrite newer text.
- Ship four independent configurable writing hooks. Optional outline generation produces titles/destinations/private prompts only, no publishable bodies. Each retained generated stop requires explicit visible author attention and current-content completion. No generate-all/bulk-Ready/auto-publish shortcut.
- Jira/issue integration, general AI orchestration, broader PR management and non-tour workshop UX changes are not part of this pack.

## Execution and evidence

Follow current AGENTS/coding-standard/nearest-directory rules, current model/topology choices and actual test isolation. Use focused tests per slice and one relevant integration pass per milestone. Do not hardcode new model routes, fleet paths or supervisors. Reuse unchanged evidence and avoid repeated exhaustive matrices.

The pack author ran no BPR build, native application, real `gh` process or GitHub/GHES mutations. Treat all 43 product acceptance rows as unverified until direct evidence exists. The supplied API examples are synthetic and use a non-routable host. Use existing fake-server and real-SQLite seams by default; actual host/website tests require the separately authorized designated safe test path. No routine implementation task may mutate a company PR or fabricate a human verdict to close a gate.

Keep formal RFC status dependency-ordered according to the current index. Do not mark a predecessor Auto-Verified merely to unblock this work. Missing native/service qualification remains named and unavailable while independent implementation proceeds. Finish with direct requirement-to-evidence traceability, current documentation, a behavior-preserving simplification pass and truthful closeout.
