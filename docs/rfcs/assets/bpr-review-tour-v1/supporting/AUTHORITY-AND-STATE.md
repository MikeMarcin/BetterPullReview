# Authority, state and transaction design

This is normative implementation detail for RFCs 0039–0043. It extends current exact one-use mutation authority rather than silently weakening it.

## Parent plan and one-use child operations

One author-facing Publish command may authorize a finite immutable plan rather than showing a modal for every navigation-link patch. The plan binds host, application profile, account, repository and PR IDs, expected head/context, draft generation, every narrative body, exact source destinations, operation kinds, a count/byte ceiling, and a deterministic navigation template. New URLs/IDs can only fill typed unresolved slots from successfully reconciled child creations belonging to that same plan.

The application issues one-use child grants for the exact operation/body/remote identity resolved at each step. Each is journaled before transmission and consumed once; creation grants cannot be reused for edits and a source-line grant cannot substitute for a review-submit grant. A model response, raw URL, broad read lease, generic action flag or persisted JSON file cannot mint a grant. The plan does not authorize arbitrary caller-supplied endpoints or new narrative text arriving after preview.

This grouped confirmation is a **narrow new tour operation**. Update the current CLI/product documentation that describes per-write confirmation so the change is explicit. Preserve current process/account/platform safety constraints and tests; a finite plan must not become a general session-wide mutation capability. Bound plan lifetime by the existing session/authority lifetime. Change of narrative, destination, included comments, account or relevant generation invalidates the remaining plan and requires fresh preview/authorization. Deterministic insertion of already-approved template links is the only late body construction allowed.

After restart or loss of the active grant, read-only reconciliation is permitted under current read authority. Remaining writes require an explicit Resume action against the reconciled immutable plan. A stored retry key does not recreate a live grant. A known completed child stays completed even when the parent is cancelled or a later stage fails.

## Suggested typed states

Names may follow local C++ conventions; meanings cannot be collapsed:

- Local draft: editing / ready / needs attention / conflict. It has never been uploaded merely by autosave.
- Initial publication: planned → observed → pending creation uncertain or pending owned → staging → verified pending → submission uncertain → submitted. Definitive rejection and conflict are distinct terminal/repair states.
- Maintenance: planned → destinations published → comment bodies partly/fully updated → index updated → optional description updated → complete. Uncertain/partial records retain exact completed child steps.
- AI: idle → running for captured editor generation → replaced once, no-op, cancelled, failed or stale. None is a publication state.

“Pending” in the existing local summary workflow may mean an in-flight write; do not confuse it with a real GitHub PENDING review. Use distinct types and UI wording. “Saved locally,” “Staged on GitHub (visible to you),” “Publishing,” “Outcome unknown,” “Published,” and “Partially updated” describe different facts.

## Durable record contents

Extend admitted schema/version migration with: local tour identity; stable host/repository/PR/account identity; plan/attempt generation; original expected head/comparison; ordered stop IDs; exact desired narrative/derived template; remote review/comment IDs and returned URL associations; baseline entity membership; before/desired edit versions; operation phase; transmission witness; confirmed receipt; conflict candidates.

Use the existing uint64/opaque-ID types and Glaze numeric handling without narrowing through a JSON double. URLs are display/navigation records after validation, not keys for credential acquisition. Sensitive text stores are private, bounded and excluded from normal diagnostics. The 4 MiB proposal covers aggregate editable prose/baselines; honor smaller existing effective limits, page observations and expire obsolete bounded baselines according to documented retention. Unresolved write records must not be evicted as ordinary cache entries.

Preserve previous schema rows. Migration/open failure leaves the database intact and author text accessible through the established recovery path; no “reset database” auto-repair. Autosave uses revision-checked transactions; dual-window conflict is not solved by file timestamps. Profile purge is local only and cannot delete staged/published GitHub content. An unresolved server-pending review may remain after purge; make that consequence visible before local deletion.

## Selection replacement transaction

Input captures canonical raw bytes B, interval [begin,end), editor revision g and exact identities. A result contains replacement R, not offsets, target IDs or other edits. Validate UTF-8 boundary, bounds and the captured generation, then compute B[:begin] + R + B[end:]. A whole-field replacement is the same operation with the full interval; empty drafting replaces the captured empty/whitespace-only field.

Emit one authorized local edit delta. The native editor performs one undo transaction and the application persists the resulting version. The acknowledgement must not rebuild the document or apply the delta a second time. A duplicate completion is ignored by request ID. Qt UTF-16 cursor offsets are bridged to raw UTF-8 with tested newline and surrogate handling. Invalid/interior byte positions are refused; never silently widen a range.

Undo and redo advance document generation even if bytes return to an earlier value. They restore the stored pre/post selection appropriately and invalidate in-flight requests; redo performs no inference. One history mechanism owns the operation; do not maintain two independent undo stacks that disagree. Source code selection for a tour destination and prose-editor selection for AI are distinct concepts and types.

## Service races and guarantees

A complete preflight does not lock another website client out. Treat the absence of documented compare-and-swap for body edits and pending-review membership as a real residual race. Before and after reads detect many conflicts but do not prove no intervening write was lost. Preserve explicit observations, never invent success, and halt rather than blindly replay an uncertain step. This pack requires bounded safe handling, not an impossible promise of exactly-once operations under arbitrary asynchronous failure.
