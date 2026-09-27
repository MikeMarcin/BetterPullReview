# RFC-BPR-0022 Verification Plan

Status: **implementation active under mandate r1; milestone A in progress; no full RFC criterion yet satisfied**

RFC base: `5a616354ae6ecd7ebbea608a9523a8388f590ab0`.

## Architecture allocation ledger

| Region | Planned proof | Status |
|---|---|---|
| Core | Provider-free decision table, transition invariants, body compatibility, stale/invalid observations | pending |
| Application host | Exact authority binding, capability evidence, serialized orchestration, cancellation and projection | pending |
| GitHub adapter | Native/file wire bytes, URL grammar, response/error classifier, normalization | pending |
| SQLite adapter | Atomic migration and parent/child recovery invariants | pending |
| Qt adapter | One representation-blind interaction, accessibility, focus, incremental projection | pending |
| Composition | Existing designated mutation route is the only selected write path | pending |

## Claim matrix

| # | Canonical acceptance criterion | Direct verification surface | Command or interaction | Durable evidence | Gate class |
|---:|---|---|---|---|---|
| 1 | Every materialized current/right source line in a text file already changed by the pull request exposes the ordinary comment gesture; file eligibility, exact head, path, line, and acting identity are revalidated before any write. | Shipped workbench composition | pending | pending | automated+rendered |
| 2 | A selected anchor accepted by the legacy line-comment operation publishes as one native line review comment on GitHub.com and GHES, independent of the off-hunk capability state. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 3 | An off-hunk anchor on a repository whose exact operation is proven supported publishes as one native line review comment and records scoped positive evidence. | Designated GitHub web/API interaction | pending | pending | external-authority |
| 4 | An off-hunk anchor proven unsupported publishes as one ordinary file-level review comment whose first paragraph is the canonical immutable permalink and whose remaining prose is byte-preserved. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 5 | An unknown capability or placement attempts the intended native write; only a fully received exact unsupported rejection can authorize fallback. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 6 | Timeout, cancellation, connection loss, created-but-unparsed response, generic 422, permission failure, rate limiting, malformed response, 5xx, or ambiguous reconciliation never starts a fallback POST. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 7 | One logical publication produces at most one GitHub comment across retries, process death, schema migration, and restart; a possibly transmitted native attempt is reconciled GET-only and can never coexist with an eligible fallback POST. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 8 | Capability evidence is repository-, actor-, profile-, server-contract-, operation-, API-, and classifier-revision scoped, freshness-bounded, and never inferred from host name, broad GHES version, readable comments, or status code alone. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 9 | BetterPullReview retains both the semantic source anchor and the honest GitHub remote subject; it renders a valid compatibility thread at its source line without claiming GitHub stored a line subject. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 10 | A GitHub-only collaborator can see the linked code, read the complete prose, reply through ordinary GitHub UI, and retain that conversation after BetterPullReview is uninstalled. | Designated GitHub web/API interaction | pending | pending | external-authority |
| 11 | Deleting the local database and refreshing from GitHub reconstructs a canonical compatibility anchor and reviewer prose; invalid, ambiguous, or foreign permalinks remain honestly file-level. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 12 | External edits, replies, resolution changes, and deletion reconcile by canonical GitHub identity. Removing or changing the permalink updates or removes the semantic placement according to server truth and never revives stale local placement. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 13 | A body requiring line-only semantics is not silently degraded through the file representation; it receives a typed incompatibility with no POST. | Focused core/host/adapter/recovery course | pending | pending | automated |
| 14 | The complete gesture follows Qt intent -> core decision -> application host -> mutation coordinator -> GitHub adapter -> durable recovery -> server refresh -> incremental Qt projection, with no provider/widget bypass. | Shipped workbench composition | pending | pending | automated+rendered |
| 15 | Automated tests cannot reach a non-designated live target, and live native and fallback evidence is produced only through the existing attended, identity-bound mutation authority. | Mutation-isolation gate and designated-live course | pending | pending | security+external-authority |

## Required hostile courses

- Native 201 received and parsed exactly.
- Native 201 response lost or malformed after transmission witness.
- Native exact unsupported rejection and atomic fallback authorization.
- Generic 422 validation error.
- 422 spam/secondary-rate response.
- 403 permission failure and 404 non-disclosure.
- Timeout before dispatch, timeout after dispatch, cancellation at both phases,
  TLS/proxy/redirect failure, and 5xx.
- Zero, one, and multiple exact reconciliation candidates.
- Death at every durable state boundary.
- Schema-v2 unresolved/succeeded migration, corrupt digest, capacity exhaustion,
  and conflicting parent/child rows.
- Canonical fallback, Markdown fallback, Unicode path, encoded separator,
  foreign host/repository, changed OID, out-of-range line, multiple links,
  removed prefix, edited prose, and reply body containing a link.
- Database loss and reinstall-shaped refresh.
- Legacy diff line with capability supported/unsupported/unknown.
- Off-hunk and unknown placement with capability supported/unsupported/unknown.
- Line-only suggestion body with a required fallback.

## Direct public path

The final automated path must begin at the real diff workbench line gesture and
end at the refreshed real conversation projection. A helper-only call to the
permalink compiler, provider session, or coordinator does not satisfy this.

The live path must use the existing designated safe-target authority and be
inspected from GitHub without BetterPullReview. Record the exact host contract,
repository, pull request, commit, path, line, operation, sanitized response
classification, comment ID, representation, acting identity, and web-UI
verdict. Do not commit credentials or private code.

## Suggested focused replay

Reconcile actual test names at import and again at closeout:

```sh
ctest --preset dev -R '^(core_review_round_trip|application_review_mutation|github_mutation_contract|sqlite_review_mutation_recovery_storage|qt_review_draft_editor|qt_github_thread_map_scale_(100|125|200)|qt_composed_github_scale_(100|125|200))$' --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```

Full `dev`, `core-only`, and `coding-tidy` matrices follow the repository's
normal risk/explicit-request policy; they are not silently substituted for the
direct criterion checks.

## Closeout rule

The live closeout card copies these fifteen criteria verbatim, records a final-
head result and commit for each, and remains outside `60-done/` while
RFC-BPR-0008 is not `Auto-Verified` or any external-authority row is pending.

Known-unsupported first attempts must also prove atomic persisted evidence
admission with no native attempt. Unresolved native attempts cannot use this route.

## Import verification scope

Only pack integrity, mapping/dependency/status consistency, documentation checks,
and diff whitespace are verified by this import. No build, CTest, UI, migration,
agent, GitHub, or GHES feature evidence was run. See [import record](../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md).

## Milestone A readiness — 2026-09-26

The [authored four-milestone plan](../rfcs/plans/rfc-bpr-0022-off-hunk-comments/MILESTONES.md)
is adopted; BPR-0561 records its current-source audit. The retained pack and
legacy planning snapshot remain immutable. Recovery is now schema 3, so BPR-0567
must preserve v1/v2 inline origins and v3 whole-review tables. Existing left-side
replies and ordinary exact user-Publish grants remain supported.

No product/API/UX/security statement changes in the readiness slice: the new
policy and permalink behavior remain pending until their code and tests land.
The fifteen canonical claim rows above remain the final acceptance authority.
