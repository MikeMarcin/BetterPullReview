---
id: BPR-0613
title: Model whole-review drafts and application submission contracts
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0610]
---

# Model whole-review drafts and application submission contracts

BPR-0610 supplies the provider wire contract. The existing core and application
publication types require a source-line anchor and cannot represent an empty
Approve summary. Add sibling whole-review policy and provider-free contract
types; do not fake line data or reinterpret existing source-line drafts.

## Ownership

New `src/core/include/bpr/core/review_submission.hpp`,
`src/core/src/review_submission.cpp`, and
`tests/core/review_submission_tests.cpp`, with their core/test CMake registration.
New `src/application/include/bpr/application/review_submission_contract.hpp`,
`src/application/src/review_submission_contract.cpp`, and
`tests/application/review_submission_contract_tests.cpp`, with their
application/test CMake registration. Existing anchored publication code stays
unchanged. The integrator owns cards, documentation, staging and commits.

## Acceptance

- A whole-review draft owns its typed draft ID, immutable semantic PR/head
  target and acting account, summary, chosen disposition, state and typed
  monotonically increasing attempt identity. No source anchor/path/line exists.
- Core commands edit summary, choose disposition, discard, begin, complete or
  fail submission. Edit/choice/discard are allowed only while drafting;
  successful submission/discard are terminal. Invalid states, commands,
  dispositions, identities and counter exhaustion fail closed with typed reasons.
- Comment and Request Changes require nonblank summaries; Approve permits an
  empty summary. Validate bounded UTF-8 text deterministically in the core,
  including the fixed Unicode whitespace set used by the current adapter.
  Editing may temporarily leave an empty summary. Invalid/oversized edits
  preserve the previous draft. Keep the 64 KiB semantic body ceiling distinct
  from the adapter's encoded JSON ceiling.
- Begin creates a new nonzero attempt. Completion/failure must name the exact
  active attempt and draft; a delayed result from an earlier failed attempt
  cannot finish a later attempt. Failure returns the exact summary/disposition
  to drafting. Duplicate begin/terminals and stale completions are refused.
- A proposal exists only for a valid submitting draft and carries its draft,
  attempt, semantic target/account, summary and disposition. The target/account
  cannot be rebound by edit, disposition choice or completion commands.
- Application types carry that proposal, a bounded idempotency key and optional
  complete live binding reusing existing target/actor binding types, without
  an anchor. Validation requires exact proposal/live target and account equality.
  A distinct review receipt names positive uint64 review identity, bounded node
  identity, target, account and disposition; validate it against its request.
  Never reuse a comment receipt or erase the distinction between review/comment IDs.
- Focused core and application fixtures cover every transition, blank/Unicode
  and size boundary, invalid enum/identity, stale attempt, target/account mismatch,
  and request/receipt validation without Qt, HTTP, credentials or persistence.

## Verification and continuation

Build focused targets in `core-only`; run `core_review_submission`,
`core_review_round_trip`, `application_review_submission_contract` and
`application_review_mutation` plus both architecture boundary tests. Run scoped
formatting, documentation structure and `git diff --check`. No full matrix.

This slice defines deterministic contracts. Subsequent BPR-0144 work must add
provider execution with exact target/actor re-observation, review-list baseline
and durable indeterminate recovery, then PR-scope UI and public composition.
The private Glaze DTOs remain in the GitHub adapter. BPR-0612's broad Qt JSON
migration stays backlogged; no live review or new credential action is included.
