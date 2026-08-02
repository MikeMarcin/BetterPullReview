---
name: betterpullreview-implement-rfc
description: Implement one or more BetterPullReview RFCs end to end with bounded portable delegation. Use when the user asks to implement a BetterPullReview RFC, implement an RFC by ID, work through an ordered RFC set, import and implement a downloaded BetterPullReview RFC pack, break RFCs into tracked tasks, delegate exact utility work, semantic scouting, one defined issue slice, or independent closeout review, commit each completed slice, keep affected agent and product documentation current, simplify the finished change set, directly verify the claimed GitHub/GHE, diff, UI, build, package, and runtime surfaces, and mark an RFC Auto-Verified only when code, tests, documentation, tracker state, delegated-review findings, and direct claim verification agree.
---

# Implement BetterPullReview RFC

Carry requested BetterPullReview RFCs from live RFC text or an imported RFC pack through verified closeout. Keep the user's order authoritative: complete, verify, and close one RFC before beginning the next.

## Operating Rules

1. Work from the live repository, not assumptions inherited from another project or stale RFC-pack paths.
2. Preserve unrelated user changes. Never reset, discard, rewrite, or silently absorb work outside the current RFC task.
3. Keep planning, implementation, tests, documentation, simplification, and closeout changes narrowly staged and intentionally committed.
4. Treat issue-board location as workflow state when the repository has a board. Do not claim completion while required work remains in an active board lane.
5. Do not mark an RFC Auto-Verified because a plausible build or test passed. Every material acceptance claim needs a command, interaction, or inspection that would fail or visibly disagree if that exact claim were false.
6. Never expose or commit GitHub credentials, GHE tokens, cookies, private repository content, or secret-bearing logs. Redact secrets from command output and evidence.
7. Use fixtures, mocks, recordings, or a designated test repository for GitHub mutations. Do not create reviews, comments, approvals, dismissals, labels, pushes, branch updates, or other remote side effects on a real pull request unless the RFC requires live validation and the user has already identified the safe test target.

## Start

1. If goal tools are available, create or reuse one goal for the complete requested RFC set, such as `Implement RFC-BPR-0001 through verified closeout`. Goal-tool failure must not block repository work.
2. Find the repository root and verify that it is BetterPullReview from the live README, project manifest, package metadata, or Git remote. Stop rather than editing a different repository.
3. Run `git status --short` before editing. Record the immutable `RFC_BASE_SHA` before any planning or pack-import commit. Retain that exact value for later impact and closeout commands. When resuming existing work, identify the earliest in-scope commit and use its first parent without folding unrelated commits into the RFC range.
4. Read the repository map and instructions before scanning source: root `AGENTS.md`, every applicable root-to-target `AGENTS.md`, `README.md`, `CONTRIBUTING.md`, architecture documentation, the nearest affected agent documentation, build/package manifests, and CI workflows. Report missing required coverage instead of inventing local semantics.
5. Discover the repository's canonical RFC and issue-tracker roots. Prefer existing indexed locations and never create a parallel tracker merely because a familiar path is absent. Check likely locations such as `_rfcs/`, `docs/rfcs/`, `code/_rfcs/`, `_issues/`, `docs/issues/`, and `code/_issues/`, then follow the live indexes and repository instructions.
6. Record the discovered paths as `RFC_ROOT`, `ISSUE_ROOT`, the active board lanes, the terminal done lane, and any canonical documentation-impact tooling. Use those resolved locations consistently for the current run.
7. If no RFC or issue convention exists yet and the requested pack requires one, default to `docs/rfcs/` and `docs/issues/`, create explicit indexes, and use the repository's existing status vocabulary. Use `Auto-Verified` as the RFC closeout status unless the repository defines a stricter equivalent.
8. If the user supplied a downloaded RFC-pack directory, complete Pack Folder Intake before planning from the imported live files.
9. Read every requested RFC and confirm its current status in the canonical RFC index. Read the issue-board index, the relevant next-ID source, dependencies, and all active cards linked to the RFC before creating tasks.

## Portable Delegation

For any nontrivial RFC, read `references/delegated-execution.md` before deciding whether to delegate. Read `references/model-routing.md` when selecting or changing role-to-model routing assignments, and `references/runtime-compatibility.md` when runtime behavior or generated adapters are uncertain.

1. Keep the primary agent as the sole RFC integrator. It owns RFC interpretation, `RFC_BASE_SHA`, issue-ID allocation, `docs/issues/NEXT_ID`, board transitions, task order, staging, commits, verification sufficiency, RFC status, and `Auto-Verified` closeout.
2. Delegate only bounded work that replaces primary-agent exploration, isolates noisy output, adds an independent quality check, or safely shortens elapsed time. Do not create duplicate same-prompt agents merely to obtain a vote.
3. Prefer the generated `rfc-utility` for exact locating, inventory, prescribed command execution, schema or drift checks, and log condensation; `rfc-scout` for semantic ownership, execution-path, build-selection, verification-surface, or root-cause analysis; `rfc-worker` for one already-defined current issue slice; `rfc-reviewer` for a fresh ordinary closeout review; and `rfc-critical-reviewer` for a capability-critical closeout review.
4. Before every spawn, restart, or follow-up task, recursively inventory every nonterminal descendant and classify it as read-only or writing. Treat an unknown or initializing descendant as occupying a lane until it finishes or its role is proven. Schedule no more than five concurrent child lanes across the complete descendant tree, excluding the primary agent; the repository Codex ceiling is intentionally higher to leave runtime headroom.
5. Permit at most one writer across the complete tree. Every writing delegation must enumerate explicit allowed files or path scopes, and a worker must stop before editing if ownership is absent, overlaps unexplained changes, or would require work outside that scope. The primary agent does not edit while a writing child runs; other read-only lanes may continue only when they cannot overlap or redirect the settled writing assignment. The primary reviews all changes, reproduces focused verification, updates tracker state, and commits.
6. Permit `rfc-scout`, `rfc-worker`, `rfc-reviewer`, and `rfc-critical-reviewer` to spawn only read-only `rfc-utility` descendants for exact locating, inventory, prescribed command or test execution, schema/drift checks, and log condensation. Encourage this for noisy build/test matrices. A parent must audit the recursive five-lane cap before each nested spawn, remain responsible for the result, and never delegate architecture, root cause, edits, verification sufficiency, tracker authority, commits, or another utility generation. `rfc-utility` cannot spawn descendants.
7. Use the external role-to-model routing assignments rather than naming vendor models in this skill. When the runtime exposes primary-session model choice, keep the RFC integrator on the deep route or stronger. Preserve the quality-biased delegated defaults: the mechanical route only for directly checkable utility work, the balanced route for semantic scouting, the deep route for implementation and ordinary review, and the deep-max route only for critical review.
8. Put no credential material, tokens, cookies, secret-bearing evidence, or unnecessary source bodies into delegation messages. Delegated agents may inspect the repository within the same local trust boundary, but they must not quote, persist, or export private repository content beyond the exact file, symbol, and evidence references needed for the assignment. Every delegated role remains subject to the root and nested `AGENTS.md` files and the remote-side-effect rules above.
9. When the current runtime cannot provide a requested agent, model override, or safe permission boundary, execute the same role serially in the primary agent. Never weaken acceptance, documentation, verification, security, or closeout gates as a fallback.

## Pack Folder Intake

When the user gives a directory such as `/Users/jaeger/Downloads/betterpullreview_review_epochs_rfc_pack`, treat it as source material to inspect and import, not as reference-only text.

1. Inventory the pack with `find` or `rg --files`. Read its README, manifest, implementation-order documents, RFCs, issue cards, architecture notes, UX specifications, test plans, assets, and scripts before deciding what belongs in the live repository.
2. If thread-title tools are available, set a concise title derived from the pack basename or RFC family, such as `BetterPullReview Review Epoch RFC Pack`.
3. Copy version-control-worthy files into destinations implied by the live repository: RFCs, issue cards, product or architecture documentation, source, tests, scripts, fixtures, UI assets, and verification material.
4. Do not copy a top-level implementation-order document as a second mutable tracker. Encode sequencing in live issue dependencies and preserve the original order file only as immutable RFC-pack source material when repository policy calls for retaining the pack.
5. Exclude `.DS_Store`, `__MACOSX/`, editor files, build trees, dependency caches, generated package output, screenshots created only for local inspection, transient logs, credentials, and secret-bearing configuration unless the RFC explicitly requires sanitized checked-in evidence.
6. Re-read the imported files from their live repository paths. Reconcile IDs, indexes, next-ID values, dependencies, board state, and destinations against the live tracker. Live next-ID and board state override stale IDs embedded in the pack.
7. Keep import, tracker registration, dependency alignment, and any conservative path adaptation in a distinct planning slice. Commit that slice before implementation begins.
8. If multiple plausible destinations would change ownership or checked-in semantics, infer from neighboring repository conventions first. Ask only when the ambiguity remains material after repository inspection.

## Plan Each RFC

1. Derive the area prefix from the live RFC ID and tracker convention rather than a hard-coded project-specific lookup table. For example, `RFC-BPR-0001` maps to `BPR`, `RFC-UI-0004` maps to `UI`, and `RFC-GHE-AUTH-0002` preserves the tracker-defined multi-token area when applicable.
2. Break the RFC into issue-card tasks using the canonical next-ID mechanism. Include:
   - a breakdown or planning task;
   - coherent implementation slices;
   - focused unit and integration tests;
   - explicit UX/UI verification when user interaction or rendering changes;
   - an explicit agent/product-documentation impact task;
   - a simplification task or closeout checkpoint;
   - verification and RFC closeout.
3. The documentation-impact task is mandatory even when the RFC omits documentation. It may conclude `no-impact` only when repository policy permits that disposition and the changed surfaces genuinely leave no semantic documentation obligation.
4. Classify the RFC's delegation needs. Use `rfc-utility` for exact mechanical inventories or prescribed command/evidence collection, and `rfc-scout` for broad or ambiguous ownership, public-path selection, root-cause, or verification planning. Run only genuinely independent lanes and keep the recursively active total at five or fewer. Classify closeout as ordinary or capability-critical using `references/model-routing.md`, and record any delegated lane and its expected evidence in the planning or closeout task.
5. Create or update issue files in the correct board lanes, advance the next-ID source atomically, link every task to the RFC, and encode task dependencies in front matter or the repository's canonical format.
6. Build the initial verification matrix during planning. Do not postpone deciding how each acceptance claim will be proven until closeout.
7. Commit the complete planning slice before implementation.

## Work Tasks

For each task in the RFC task set:

1. Move the issue card through the board as work progresses. Folder placement or the repository's canonical board field is the source of truth.
2. Implement only the current task's slice. Avoid opportunistic refactors unless they are necessary for the RFC; create a separate tracked task when newly discovered work is material.
3. Add narrowly scoped tests that exercise the changed behavior through the same public or runtime surface named by the RFC.
4. Update architecture, product, developer, operator, and agent documentation whenever the RFC or actual changed surfaces require it.
5. Run focused verification for the slice before moving it to done.
6. Move the issue card to the terminal done lane, update status metadata, and commit the completed task before beginning the next task.

If the current slice is delegated to `rfc-worker`, first finish any semantic
scout whose result could redirect the assignment, prove that no other writer
remains nonterminal, and confirm the recursive five-lane cap. Give the worker
the issue card, relevant RFC acceptance criteria, applicable instruction
files, an explicit exclusive list of allowed files or path scopes, explicit
non-goals, and focused verification. The worker may use read-only
`rfc-utility` descendants for prescribed tests and other mechanical evidence;
the primary agent must refrain from editing while the worker runs. Afterward,
wait for the worker to leave Working status, inspect the complete diff, reject
out-of-scope changes, reproduce focused verification, and retain sole
ownership of `docs/issues/NEXT_ID`, board moves, staging, and commits.

If implementation exposes missing requirements, create additional issue cards and continue the same loop instead of folding untracked work into a later closeout commit.

For agent documentation, use a BetterPullReview-local documentation skill or canonical script when one exists. Otherwise perform the review manually from live source, build, test, and runtime evidence. Never invoke another project's documentation skill, copy foreign scope names, or make a dummy documentation edit merely to satisfy a checklist.

## Verification Planning

Before implementation gets close to completion, refine the verification matrix from the RFC's actual claims. Each row must contain `claim`, `direct surface`, `command or interaction`, `what proves the claim`, `result`, and `evidence`. Use bounded `rfc-utility` lanes proactively for prescribed build/test commands, exact inventories, and compact log collection when slots permit, including as descendants of the worker or reviewer. `rfc-scout` may map live selection and verification surfaces, but the primary agent owns the matrix and confirms its evidence.

Extract every material user-visible, architectural, integration, security, and operational claim, including applicable items from the following BetterPullReview domains:

1. **Repository and provider selection**: GitHub.com versus GitHub Enterprise Server, REST versus GraphQL path, configured hostname, API-version behavior, authentication mode, capability detection, permissions, and failure handling.
2. **Pull-request identity and epochs**: repository/PR selection, merge base, branch base, latest head, prior review epochs, left/right epoch selection, refresh semantics, stale-head behavior, and commit-to-commit comparison identity.
3. **Diff correctness**: file status, rename/copy handling, binary and generated files, hunks, line numbers, whitespace policy, syntax classification, intra-line changes, moved/grouped blocks, center connectors, selected-line mapping, and the line-over-line detail pane.
4. **Review workflow**: previous/next file, previous/next change, viewed state, draft and submitted comments, conversation mapping, thread resolution, review summary, keyboard commands, and navigation behavior.
5. **Workbench UI**: collapsible file and conversation drawers, focus order, command enablement, selection persistence, loading/error/empty states, theme behavior, high-DPI rendering, accessibility surfaces, and layout at supported window sizes.
6. **State and caching**: local persistence, cache invalidation, repository switching, refresh, crash/restart recovery, schema migration, and protection against cross-repository or cross-account data leakage.
7. **Performance and scale**: large PRs, large files, many comments, many epochs, incremental loading, cancellation, memory bounds, UI responsiveness, and API rate-limit behavior.
8. **Build and distribution**: selected feature flags, platform variants, package exports, installers or application bundles, configuration defaults, update/migration behavior, and clean consumer or launch paths.
9. **Remote-side-effect safety**: mutations occur only through the intended command path, retries are idempotent where required, test runs cannot modify production PRs, and logs/evidence contain no secrets.

For every claim:

1. Name the direct public, packaged, or runtime surface that exercises it. Prefer the actual BetterPullReview application, provider facade, diff pipeline, persisted state path, or installed package over a lower-level helper API.
2. Inspect live configuration before trusting names. Read feature flags, dependency-injection bindings, provider factories, environment gates, package manifests, generated configuration, test fixtures, and launch arguments. A target named `ghe` is not proof that the GHE implementation was selected.
3. Add or strengthen tests when no existing command can fail for the claim. Provider selection, auth/config routing, epoch selection, public diff behavior, persisted defaults, package consumption, and remote mutation boundaries require direct smoke or integration coverage.
4. For visual or interaction claims, require an actual rendered BetterPullReview state plus deterministic UI automation, component integration evidence, or a documented manual interaction check. A static mockup, screenshot of unrelated markup, or unit test of a view model is not sufficient by itself.
5. For diff claims, use representative fixtures that include ordinary edits and relevant edge cases. Verify displayed line identity and navigation against known expected data, not only that rendering completed.
6. For GitHub/GHE integration claims, use mocked/recorded contract tests by default. A live test is supplementary unless it is executed against an explicitly designated safe repository and PR.
7. Ask of every row: `Could this pass while the claim is false?` If yes, add a stronger check or narrow the claim.

Auto-Verified is blocked when a mandatory claim is covered only indirectly, only through a helper API, only by generated text, only by a disabled code path, only by a static mockup, or only by a test double that does not validate the relevant contract.

## Documentation Ledger

Maintain a documentation ledger in the closeout task. For each affected scope, record:

- changed-file classes and public surfaces;
- required review documents;
- disposition: `updated`, `generated-refreshed`, `no-impact`, or `unresolved`;
- source/build/test/runtime evidence;
- validation command or inspection;
- claims intentionally left unverified.

At minimum, review documentation impact for changes to public APIs, GitHub/GHE integration, authentication and security, PR/epoch identity, diff semantics, review mutation semantics, persistence, lifecycle, concurrency/cancellation, UI architecture, keyboard commands, package/build configuration, and application launch behavior.

## Simplify

After all implementation tasks for the current RFC are complete:

1. Run a conservative simplification pass over the RFC change set. Use `code-simplifier` when available; otherwise perform a focused manual simplification review.
2. Preserve behavior and public semantics. Do not use simplification to broaden scope or conceal unfinished work.
3. Re-run affected focused and boundary verification.
4. Commit simplifier changes separately when they exist. Record an explicit no-change result when the pass finds nothing safe to improve.

## Agent And Product Documentation Gate

After implementation, tests, product documentation, and simplification are committed, but before RFC status closeout:

1. Discover any canonical BetterPullReview documentation-impact, validation, coverage, or generated-index tooling from repository instructions. Read its help and use the documented invocation over `RFC_BASE_SHA..HEAD`; do not guess command paths or flags copied from another repository.
2. If no canonical impact checker exists, inspect `git diff --name-status "$RFC_BASE_SHA"..HEAD`, map every changed path to its owning `AGENTS.md` and product/developer documentation, and complete the ledger manually.
3. Treat automated classification as a routing lower bound, not proof of semantic completeness. Add obligations found through source and runtime review.
4. Update or scaffold the owning semantic reference, refresh deterministic companions through the canonical generator, or record an approved no-impact rationale only when no sensitive semantic surface changed.
5. A matching documentation-path edit proves routing only. It cannot waive public API, build/package, lifecycle, threading, identity, persistence, provider, GitHub mutation, security, integration, generator, verification, or application obligations.
6. Run every available structural validator, scope coverage check, generated-index check, documentation test, and documented focused/boundary command for affected scopes.
7. Commit resulting documentation changes, then repeat the gate until no obligation remains unmet.

Any unmet obligation, invalid agent reference, unexplained ownership gap, stale generated fact, secret-bearing evidence, unresolved semantic claim, or unexecuted required command blocks Auto-Verified. Create or reopen issue cards and return to the task loop.

## Verify And Close

Before marking an RFC Auto-Verified:

1. Confirm the Agent And Product Documentation Gate passes and the ledger agrees with committed code, build metadata, tests, runtime behavior, and documentation.
2. Re-read every RFC acceptance criterion and compare it against the actual implementation, UI, provider behavior, docs, examples, and tests.
3. Re-run the complete verification matrix after the final simplification and documentation commits. Use clean configure/build/test/package state when build inputs, dependencies, generated configuration, feature flags, provider selection, or packaging changed. Do not count stale build output as proof.
4. Add an explicit selected-surface check for every introduced or changed provider, option, backend, feature flag, migration, default, or package path. Prove selection through the public BetterPullReview path and inspect resulting configuration when useful.
5. Exercise claimed workbench behavior through the real application or its closest deterministic integration harness: epoch selection, file/change navigation, drawer behavior, line mapping, diff grouping, comment/thread behavior, persistence, error handling, and theme/layout where applicable.
6. Re-run GitHub/GHE contract and remote-mutation-boundary tests. Confirm no command can target an unintended repository or PR under the tested configuration and no evidence contains credentials.
7. Run relevant builds and tests plus `git diff --check`. Include dependency and boundary scans when the RFC claims optional integration, package isolation, provider independence, or absence of a dependency.
8. Check for overclaiming. If the result is a shell, adapter stub, direct-helper prototype, static mockup, evidence wrapper, or parity harness that does not exercise the claimed public/default path, leave the RFC open or record the narrower truth.
9. Run a final independent review gate before changing status. For a substantial ordinary RFC, delegate to `rfc-reviewer`; for a capability-critical RFC as classified in `references/model-routing.md`, delegate to `rfc-critical-reviewer` instead. Do not run both by default. Give the selected reviewer the live RFC, `RFC_BASE_SHA..HEAD`, verification matrix, documentation ledger, commands run, and concise evidence locations, but not the implementation transcript or rationale. Require it to identify claims that could remain false while recorded commands pass. Reproduce every blocking finding directly. When no independent agent is available, perform a separate skeptical review pass in the primary agent. If review finds a gap, create or reopen issue cards before status changes.
10. Review the final committed-or-to-be-committed diff as the integrator. List the highest-risk claims and ensure each has a direct failing-if-wrong command, interaction, or inspection. Reopen work when any gap remains.
11. Confirm all issue cards required by the RFC are in the terminal done lane and no active lane contains required work.
12. Update RFC front matter to `status: Auto-Verified` when that is the live BetterPullReview closeout status, update implementation-status text, update the canonical RFC index, and update any combined RFC-set document that embeds the same status.
13. Commit the verification and closeout slice.

If review finds a gap, do not mark the RFC Auto-Verified. Create or reactivate tasks and return to the task loop until the direct evidence supports closeout.

## Finish

Begin the next requested RFC only after the current RFC is committed and Auto-Verified. At the end of the requested set:

1. Verify the worktree and identify any intentional unrelated changes still present.
2. Summarize planning/import commits, task commits, simplification, closeout, verification commands and interactions, UI evidence, GitHub/GHE contract coverage, documentation updates, generated facts refreshed, approved no-impact results, delegated roles used, serial fallbacks, and omitted or deferred lanes.
3. Mark the goal complete only when every requested RFC has actually closed, every documentation obligation is satisfied, every required issue card is done, and every material verification-matrix claim has direct passing evidence.
4. If a later review exposes a blocking gap, reopen or add issue cards and correct the RFC status before continuing dependent work.
