---
id: RFC-BPR-0022-ISSUE-PLAN
rfc: RFC-BPR-0022
status: Planning snapshot
rfc_base_sha: 5a616354ae6ecd7ebbea608a9523a8388f590ab0
requested_position: post-pack-after-RFC-BPR-0021
---

# Issue Plan for RFC-BPR-0022

## Goal and stop condition

Implement the new semantic source-line publication contract without modifying
RFC-BPR-0008 and without creating a second collaborative protocol.

The work is complete only when an eligible off-hunk current-head line uses a
native GitHub line comment where the exact operation supports it, transparently
uses the ordinary file-comment/permalink representation after an exact
unsupported rejection, and cannot duplicate a comment under any ambiguous
transmission or restart course.

Import-only direction on 2026-09-04 defers implementation; every live card is
Backlog. RFC-BPR-0022 cannot become `Auto-Verified` before its dependencies.
Move a live-service card to Parked only after all prerequisites are done and
only the attended designated interaction remains.

## Architecture allocation

| Boundary | Planned work |
|---|---|
| Core | publication representation, placement/capability observations, pure planner, legal state transitions, line-only-body incompatibility |
| Application host | changed-file/current-head binding, capability cache, parent/child orchestration, cancellation, restart recovery, incremental projection |
| GitHub adapter | native and file-subject requests, exact error classifier, permalink compiler/parser, response normalization |
| SQLite adapter | schema-v2 migration to logical publication plus child attempts, atomic fallback authorization, baselines, receipts |
| Qt adapter | same line affordance/editor/pending/result flow, support-only representation diagnostic, no capability choice |
| Composition | one selected provider/recovery path under existing designated mutation authority |

## Planned issue graph

| Allocated card | Slice | Depends on |
|---|---|---|
| BPR-0561 | Import, reconcile, allocate, and record architecture | — |
| BPR-0562 | Core publication-placement and representation policy | BPR-0561 |
| BPR-0563 | Exact anchor eligibility and placement observations | BPR-0561, BPR-0562 |
| BPR-0564 | Canonical immutable permalink compile/parse contract | BPR-0562, BPR-0563 |
| BPR-0565 | Native/file GitHub wire contracts and unsupported classifier | BPR-0563, BPR-0564 |
| BPR-0566 | Repository-scoped write-capability evidence | BPR-0565 |
| BPR-0567 | Durable logical-publication and child-attempt recovery | BPR-0562, BPR-0564, BPR-0565 |
| BPR-0568 | Native-first coordinator negotiation and fallback transition | BPR-0566, BPR-0567 |
| BPR-0569 | Read normalization and semantic thread rehydration | BPR-0564, BPR-0565, BPR-0568 |
| BPR-0570 | Representation-blind Qt comment-anywhere UX | BPR-0562, BPR-0563, BPR-0569 |
| BPR-0571 | Composition and complete public-path integration | BPR-0568, BPR-0569, BPR-0570 |
| BPR-0572 | Automated security, recovery, and compatibility verification | BPR-0571 |
| BPR-0573 | Designated GitHub.com/GHES native/fallback evidence | BPR-0571, BPR-0572 |
| BPR-0574 | Documentation reconciliation and simplification | BPR-0572 |
| BPR-0575 | Final criterion replay and RFC closeout | BPR-0573, BPR-0574, RFC-BPR-0008 Auto-Verified |

This file is an immutable planning snapshot. Live issue folder placement and
front matter are the sole workflow authority after import.

## Slice details

### 01 — Import and architecture

- Reconcile the source paths, issue IDs, RFC status, and current mutation schema.
- Add RFC-BPR-0022 only to the post-pack index.
- Create the live verification ledger with all fifteen acceptance criteria.
- Record the exact live RFC base SHA.
- Confirm that RFC-BPR-0008 remains unchanged except for ordinary cross-links
  required by the index or architecture documentation.

### 02 — Core policy

- Add provider-free placement, capability, representation, and body-
  compatibility vocabulary.
- Implement the pure decision table and legal terminal transitions.
- Preserve the existing `source_line_ref` and current-head/right-side scope.
- Prove no URL, HTTP, filesystem, persistence, or Qt dependency enters core.

### 03 — Eligibility and placement observations

- Bind changed-file membership, exact head/path/blob/line, and acting identity.
- Represent provider placement as legacy/off-hunk/unknown.
- Refuse use of Qt hunk geometry as provider authority.
- Add a bounded exact provider placement observation only if its completeness
  and head identity can be proved; otherwise keep unknown and negotiate by
  write.

### 04 — Permalink contract

- Compile the same-host/same-repository full-OID URL and canonical visible body.
- Parse only the exact grammar and validate authority, path, commit, line, blob,
  and body.
- Cover Markdown `?plain=1`, encoded separators, dot segments, userinfo,
  alternate ports, fragments, Unicode paths, length limits, and malformed UTF-8.
- Keep reviewer prose byte-preserved after the generated prefix.

### 05 — GitHub wire and classifier

- Retain the current native-line request as one explicit representation.
- Add the file-subject request with line/side/start fields absent.
- Parse both response shapes honestly and retain remote subject.
- Add a bounded structured error model and exact classifier revision.
- Prove generic 422, spam/rate/permission, malformed, and unknown errors do not
  classify as unsupported.

### 06 — Capability evidence

- Add a narrow, bounded, repository- and operation-scoped tri-state cache.
- Use exact recorded contract, native success, or definitive rejection only.
- Bind profile, actor, repository, server/API/operation/classifier revisions.
- Expire observed evidence without turning expiry into unsupported.
- Keep the existing general read/schema capability cache coherent; do not
  force a repository dimension into unrelated facts unless the live allocation
  proves that is the narrower design.

### 07 — Durable recovery

- Migrate schema v2 transactionally.
- Represent a parent logical publication and deterministic native/fallback child
  attempts.
- Atomically authorize fallback from exact native rejection or the live RFC's
  no-native-attempt admission with persisted scoped unsupported evidence.
- Retain one baseline and one transmission witness per child.
- Preserve succeeded and unresolved legacy rows without fabricating fallback
  authority.
- Add corruption, capacity, conflict, and restart tests at every transition.

### 08 — Coordinator negotiation

- Combine the exact observations through the core planner.
- Perform the native attempt as the user-approved negotiation when required.
- Record positive/negative capability evidence only after the exact terminal.
- Create and execute fallback only after durable authorization.
- Preserve existing double target observation, head checks, acting identity,
  one-use production carrier, cancellation, and server refresh.

### 09 — Normalization and thread map

- Normalize native line and file-subject responses separately.
- Rehydrate canonical fallbacks from publication evidence or visible grammar.
- Retain raw remote body, projected reviewer prose, semantic anchor, remote
  subject, and representation.
- Make external edits and database loss deterministic.
- Keep replies attached through the root and never parse reply prose as an
  anchor.
- Do not duplicate RFC-BPR-0021's arbitrary historical revision join.

### 10 — Qt UX

- Present the ordinary comment gesture on every eligible current-side line in a
  changed file.
- Reuse the existing editor and semantic draft path.
- Keep capability and fallback mechanics absent from normal controls.
- Add an expandable diagnostic only for explanation/support.
- Prove focus, keyboard, accessibility, scale, theme, and no-layout-jump behavior.
- Present a typed incompatibility when line-only content cannot degrade safely.

### 11 — Composition

- Wire the new planner, capability evidence, provider variants, recovery schema,
  normalizer, and Qt projection through the shipped application.
- Extend the existing public fixture course rather than creating a second app or
  provider.
- Preserve tests-disabled mutation admission and default-deny isolation.

### 12 — Automated verification

- Exercise every placement/capability/body cell.
- Exercise death before and after each baseline, witness, rejection, fallback,
  receipt, and resolution transition.
- Exercise generic 422, timeout, cancellation, lost 201, ambiguous GET,
  corrupted baseline, duplicate candidates, and capacity failure.
- Exercise database loss, external link/prose edits, replies, resolution,
  deletion, and uninstall-shaped reconstruction.
- Run composition-oriented selectors in addition to subsystem selectors.

### 13 — Live evidence

- Use the existing attended designated-live mutation authority.
- On a safe GitHub.com repository, publish to an exact off-hunk line and record
  whether the exact API operation is native-supported.
- On a safe legacy GHES repository, or a separately approved exact recorded
  contract, demonstrate definitive unsupported classification and the ordinary
  file-comment/permalink result.
- Inspect both from the GitHub web UI without BetterPullReview: code rendering,
  full prose, author identity, reply, and thread state.
- Sanitize evidence and never retain credentials or private source content.

### 14 — Documentation and simplification

- Reconcile the documents listed in the RFC.
- Remove duplicated policy and representation leaks.
- Confirm incremental projection and bounded caches.
- Re-run focused checks affected by simplification.

### 15 — Closeout

- Wait for RFC-BPR-0008 Auto-Verified status.
- Replay all fifteen canonical criteria at one final head.
- Record the exact command/interaction, result, durable evidence, and commit per
  row.
- Run documentation structure gate and `git diff --check`.
- Mark RFC-BPR-0022 Auto-Verified only when code, docs, board, and evidence agree.

## Required verification families

- Core table, replay, property, and hostile-invalid-observation tests.
- Application-host manual-executor tests for ordering, cancellation,
  supersession, stale authority, and projection delivery.
- GitHub contract tests for byte-exact requests/responses and classifier
  counterexamples for GitHub.com and GHES 3.18 fixtures.
- SQLite migration/restart/corruption/capacity tests.
- Qt event-to-intent, model delta, focus/accessibility, and rendered interaction
  tests at the required scales.
- End-to-end tests through the real composition root and existing review draft
  gesture.
- Live attended GitHub-only collaborator evidence.
- Local and target-native evidence only; GitHub Actions remains out of scope.

## Likely focused selectors

Reconcile names against the live CTest inventory before recording them:

```sh
ctest --preset dev -R '^(core_review_round_trip|application_review_mutation|github_mutation_contract|sqlite_review_mutation_recovery_storage|qt_review_draft_editor|qt_github_thread_map_scale_(100|125|200)|qt_composed_github_scale_(100|125|200))$' --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```

The closeout card must also name one complete selected/public-path course. A
passing unit selector cannot substitute for the shipped interaction.

## Closeout blockers

- RFC-BPR-0008 is not Auto-Verified.
- The normal UI asks the user to choose native versus fallback.
- Any ambiguous native write can start fallback.
- Status 422 alone can mutate capability state.
- A compatibility comment is a PR timeline comment despite file review comments
  being available.
- The fallback anchor exists only in private state and cannot survive database
  loss.
- GitHub remote subject and semantic anchor are collapsed into one false field.
- Qt geometry or text search guesses provider placement.
- A widget calls the provider or builds the permalink.
- The recovery migration can expose two POST-eligible children.
- A line-only suggestion silently loses applyability.
- Live evidence uses an undesignated target or stores credentials/private code.
- Required documentation or final-head criterion evidence remains open.

## Import reconciliation

This is source-only planning material adapted at import; live RFC text and card
dependencies take precedence. The exact original remains in the retained pack.
See [the import record](../assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md) for identity mapping,
shared-contract corrections, and work deliberately left for later planning.
