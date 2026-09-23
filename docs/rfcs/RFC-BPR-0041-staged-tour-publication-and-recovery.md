---
id: "RFC-BPR-0041"
title: "Staged Tour Publication and Recovery"
status: "Proposed"
area: "BPR"
milestone: "Review tours"
depends_on: ["RFC-BPR-0008", "RFC-BPR-0015", "RFC-BPR-0013", "RFC-BPR-0040"]
pack_version: "review-tour-v1"
metadata: {"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# RFC-BPR-0041: Staged Tour Publication and Recovery

## Decision and existing seam

Implement an author-triggered publication coordinator that stages a tour as one native pending review, resolves real comment URLs, wires navigation, and submits the exact prepared review with disposition **COMMENT**. Extend the existing review-submission application/contract/recovery/provider family and its fixed operation catalogs. Do not run a shell script as the product publisher and do not introduce an unrelated generic GitHub mutation client.

The snapshot's `review_submission_draft` is a whole-PR summary/disposition draft without a collection of inline stops. Its existing recovery/session code is valuable infrastructure, not evidence that a linked pending batch already works. The existing CLI transport also has GET/POST-oriented compilation paths; new PUT/PATCH/DELETE operations require explicit typed admission and tests, not a permissive “all non-GET means POST” extension. See [SOURCE-AUDIT](assets/bpr-review-tour-v1/supporting/SOURCE-AUDIT.md).

## Publication guarantees

Call the successful native path **one review submission**. This is a reviewer-visible publication boundary for the staged review, not an ACID transaction encompassing description edits, other comments, network requests or notifications. No exact notification-count guarantee is made. Raw pending content may be visible to its author from other clients. Another client can edit, delete, add to, or submit that pending review; BPR cannot lock GitHub's web UI.

Local preparation and autosave send no remote mutations. The explicit final Publish action authorizes staging and submission of the previewed immutable plan, including deterministic insertion of returned URLs. It does not authorize arbitrary later edits, reviewer assignment, draft-status conversion, or other PR actions. Do not stage unfinished skeletons just because the author navigates away.

A bounded parent plan may issue exact one-use child grants for its staged operations, as specified in [AUTHORITY-AND-STATE](assets/bpr-review-tour-v1/supporting/AUTHORITY-AND-STATE.md). This is an explicit tour-specific grouped-confirmation extension to current per-write authority, not reuse of one grant for unrestricted writes. The author-facing flow does not require a modal for every deterministic navigation patch.

PR-description updating is a separately named operation. The tour must be complete without its description backlink; add that link after the index exists. Copying the brief into the description requires exact body-update permission and conflict checks, and cannot be described as part of the review's atomic boundary.

## Typed operation catalog

The adapter implements a finite set of parameterized operations, bound to the exact host/repository/PR/account and intent. REST paths below are API-relative; the provider profile supplies the correct GitHub.com or GHES root and API-version header.

| Operation | Documented REST shape / alternative |
|---|---|
| Read PR/body/head | `GET /repos/{owner}/{repo}/pulls/{number}` |
| Enumerate/get reviews | `GET .../pulls/{number}/reviews` and `/reviews/{review_id}` |
| Create pending line batch | `POST .../pulls/{number}/reviews`, exact `commit_id`, `body`, `comments`; omit `event` |
| Enumerate staged comments | `GET .../pulls/{number}/reviews/{review_id}/comments` |
| Edit stop body | `PATCH /repos/{owner}/{repo}/pulls/comments/{comment_id}` with `body` |
| Edit index/review summary | `PUT .../pulls/{number}/reviews/{review_id}` with `body` |
| Submit prepared review | `POST .../reviews/{review_id}/events` with `event: COMMENT` and final summary body |
| Discard owned pending review | `DELETE .../reviews/{review_id}`, separately authorized and only if still pending/uncontaminated |
| Update PR description | `PATCH .../pulls/{number}` with only `body` |
| Compatibility PR comment | Existing/narrowly added issue-comment create/read/update operations, bound to this PR |

API basis: [GH-REVIEWS], [GH-COMMENTS], [GH-PR], [GH-ISSUES], [GHES-REVIEWS]. The REST batch exposes `path`, `body`, `line`, `side`, `start_line`, `start_side`, and legacy `position`; select an evidenced shape and never confuse file line numbers with diff positions. Do not add an undocumented `pull_request_review_id` or `subject_type` to that batch shape.

For a needed file-level fallback, the provider may use the fixed GraphQL `addPullRequestReviewThread` operation with `pullRequestReviewId` and `subjectType: FILE`, on a host where this exact pending operation is qualified. A fixed GraphQL comment-body update may also be used if it is the verified pending-edit route. This is a narrow extension to the current GraphQL allowlist, not permission for arbitrary supplied GraphQL. Returned REST numeric IDs and GraphQL node IDs are different types; use returned identifiers or verified cross-API associations, not decoded node strings. [GH-GRAPHQL]

## Native single-review sequence

1. **Freeze.** Capture the exact draft generation, bodies, ordered stop IDs, anchors, requested representation, expected PR head/base context, acting account, host/profile revisions, and allowed deterministic navigation template. Locally unfinished stops block this plan, not local saving.
2. **Observe.** Read authoritative actor/PR/head, permissions, existing own pending reviews, and complete baseline collections needed for correlation. A foreign/unknown pending review is a conflict; do not adopt it, discard it or add the tour into it automatically. A previously journaled owned pending review can be resumed after exact reconciliation.
3. **Journal before transmission.** Persist the immutable operation and pre-write baseline using the existing private admitted recovery store. Failure to persist refuses the write. Track Prepared versus Possibly transmitted distinctly.
4. **Create pending.** Submit the supported line/range comments together with `event` omitted. For mixed file-subject staging, create the pending review and add only the separately admitted file threads to its exact review ID. Each additional write has its own durable step/witness. Do not create one publicly submitted review per stop.
5. **Bind returned identities.** Read the pending review and all its comments, match expected bodies/anchors/actors as a complete multiset, and bind local stops to actual remote IDs and URLs. Creation response order is not a stable mapping. If two candidates cannot be distinguished, halt as ambiguous instead of guessing. Human-readable ordinals in the initial bodies may disambiguate otherwise identical titles.
6. **Wire navigation.** Patch each owned pending comment to include the final actual Previous/Next/index links. Set the summary's final index via the supported pending update or directly in the submit request; the latter avoids an unnecessary summary write. No placeholder, guessed ID, temporary BPR marker or private draft token may enter the completed tour.
7. **Verify exact prepared state.** Re-read pending state and complete membership, bodies, anchor subjects, actor and PR. Check for third-party/client edits, extra comments, deleted entries, expired authority, head movement or changed plan generation. Any mismatch stops submission and surfaces reconciliation. A pre-submit read narrows a race; it is not a server compare-and-swap guarantee.
8. **Submit exact review.** Send COMMENT for the recorded review ID, with the final index body. Never call a convenience API that submits “whatever review is currently pending.” Persist transmission and reconcile the exact submitted review and membership before showing success.
9. **Optional description update.** Refresh, merge/check and publish the separately previewed Problem/Solution/backlink edit. If this fails, report “Tour published; description update needs attention,” retain the review IDs, and do not repost the tour.

Staging may display a brief ordinary warning in the pending summary that preparation is incomplete. It is not private metadata and must not be used as the sole recovery identifier. Final submission replaces it. If another client submits prematurely, show the actual resulting state and an explicit repair plan; do not pretend it remained private or auto-create another copy.

## Compatibility publication

The normal target remains a staged single review, including qualified file-level fallbacks. A historical/untouched-file stop or unsupported staging primitive can require standalone normal PR/review comments. Detect the mismatch before the first visible write when possible.

Offer two explicit outcomes: **Keep single-review publication** (resolve/remove/relocate the incompatible draft destinations deliberately), or **Publish with compatibility comments** (preview the non-atomic operation sequence). Do not silently select the latter. In compatibility mode, create the individual comments with valid source/index-or-PR fallback navigation, retain every returned identity, wire remaining links, and publish/update the final index only after its destinations are known. Until complete, partial publication is stated as partial. Never delete already published discussion as an automatic rollback.

No scratch comment is created/deleted merely to discover write capability on a real company PR. Use recorded contracts and a designated safe fixture repository for qualification. An actual intended operation's definitive unsupported response may be evidence, but an indeterminate write, generic 422, permission denial or rate limit cannot authorize a second representation.

## Failure, retries and external writers

Use a durable per-tour plan with ordered steps and exact local retry keys. The local key is not a GitHub server idempotency guarantee; `clientMutationId` is not treated as one either. Reuse the existing journal's before-send witness and read-only reconciliation logic, extended to known pending/submitted states and comment membership.

For each step record entity IDs, before/desired body versions, expected source/actor, baseline identities, current phase and returned receipt. Retain only the bounded data actually needed for recovery. No credentials, raw transport logs or repository source snapshots belong in diagnostics.

On an ambiguous response, timeout, process death or account switch after possible transmission, do not repeat a create/submit or invoke a fallback. Reconcile by known IDs, or by one uniquely matching new review/comment against the complete pre-write baseline. An incomplete page, no currently visible match, or eventual-consistency delay is not proof that the first write failed. Multiple plausible matches remain ambiguous. Exactly-once publication across arbitrary network/server failure is not promised.

A known current body equal to the desired body resolves that update without another write. A body still equal to the baseline does not alone prove an earlier request can never arrive; ambiguous writes remain constrained by the recovery protocol. A body different from both baseline and desired is a conflict. Automatic retries are limited to documented safe conditions and existing rate-limit policy; never blindly retry an unknown write.

If a staged review now contains a website-added comment, BPR must not submit it under the tour's old authorization. Show the unexpected content and let the author resolve ownership deliberately. Restart does not silently resume remote writes merely because a plan was persisted: reconcile reads first, then require an explicit Resume publication command for remaining effects.

## CLI transport

Use `gh api` for structured REST operations and `gh api graphql` only for admitted fixed GraphQL operations. `gh pr review` does not provide the required structured batch/ID-controlled staging interface. Retain current CLI opt-in, executable/version rules, account pre/post observations, filesystem-write confinement and platform eligibility. Do not use `gh auth token`, `--show-token`, token extraction, auth switching, shell interpolation or global config writes. [GH-CLI-API], [GH-CLI-REVIEW]

Compile fixed argv with an exact method allowlist and explicit hostname. Serialize JSON using the project's existing Glaze paths. Send bodies on bounded stdin through `--input -`, not argv or a shared temp file. Drain stdout/stderr while writing stdin; bind cancellation/deadlines to existing process supervision. Normalize responses identically to native HTTPS, including status/body limits, safe redirects, pagination and numeric IDs without floating-point truncation. A CLI read lease does not authorize a write. A switched account never authorizes retry under the replacement actor.

Do not weaken RFC-BPR-0015 or current release/test mutation admission to make tour tests pass. Qualify new operations separately per eligible transport/platform. Native HTTPS remains first-class; the feature does not mandate installing `gh` for that transport.

## Acceptance criteria

- **AC41-01:** Exact native line/range batches remain pending until one explicit COMMENT submission; no per-stop visible publish loop substitutes for this path.
- **AC41-02:** Returned comment IDs/URLs drive every link, and complete staged membership/bodies/anchors are verified before submission.
- **AC41-03:** Existing/contaminated pending reviews, head/actor changes and external early submission are handled without unauthorized inclusion, deletion or duplication.
- **AC41-04:** Native HTTPS and admitted `gh api` operations share normalized semantics and preserve their existing credential/authority boundaries.
- **AC41-05:** Kill/timeout tests at every durable step prevent blind reposts and recover exact known pending/submitted identities; unresolved outcomes stay unresolved.
- **AC41-06:** File-subject staging is qualified per exact operation; unbatchable destinations require an explicit compatibility plan and never an atomicity claim.
- **AC41-07:** Description/backlink failure is separate from tour success; retries do not recreate published comments.
- **AC41-08:** Pagination, rate limits, denied/unsupported operations, wire budgets, redirects, ambiguous responses and cancellation are exercised on real production ports with fixtures.
- **AC41-09:** Final Preview/Publish and Resume flows are composed in the running application and remote writes occur only after their explicit author commands.

## Verification boundary

The offline pack contains request examples and a test plan, not captured live API results. Pending comment editability, file-subject staging, comment web links, author identity and visibility on GitHub.com/GHES require designated live evidence. Keep each capability unavailable until qualified, while allowing manual preparation and independent supported routes to proceed. No live mutations are authorized by a fixture test or by this pack's validation script.
