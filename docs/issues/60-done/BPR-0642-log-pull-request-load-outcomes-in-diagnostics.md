---
schema: "implement-rfc.card.v1"
id: "BPR-0642"
title: "Log pull request load outcomes in diagnostics"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"A failed load records the failure class and the last paged operation in the diagnostics log; a successful load records one succeeded entry.","outcome":"pass","revision":"13b1dc534e5cd2885cb31388614ddd79705e96c9","evidence":{"kind":"command","detail":"dev CTest application_github_window_read_host, github_paged_read, github_provider, quill_logging and application_logging passed; covers typed final attempted operation, real pager validation error, provider terminal propagation, one success and persisted failure field."}},{"criterion":"The diagnostics privacy contract tests pass unchanged; no new text fields carry user or repository content.","outcome":"pass","revision":"13b1dc534e5cd2885cb31388614ddd79705e96c9","evidence":{"kind":"command","detail":"dev CTest quill_logging, quill_logging_backpressure and application_logging passed; new operation, status, session, request and failure fields use closed enums or numbers, and composed log excludes fixture path/repository and credential canaries."}},{"criterion":"The recorded-fixture load courses show the new entries without changing their pinned outcomes.","outcome":"pass","revision":"13b1dc534e5cd2885cb31388614ddd79705e96c9","evidence":{"kind":"command","detail":"dev CTest application_logging inspects composed diagnostics.log for PR started, paged response and single succeeded entries; application_github_public_path_github_com and application_github_public_path_uiforetw_74 retained their existing successful outcomes."}}]
---

# Log pull request load outcomes in diagnostics

## Current frontier — bpr0642-r1 / d1 / mandate r1

Milestone: one fixture-demonstrated `github.pull_request` load diagnostic course
for this card. Source candidate `13b1dc53` is committed on main. The provider
pager emits correlated typed operation/status observations;
the application load host emits one start and terminal outcome, including validation
failure names. Scope is the application load host, provider response observation,
typed diagnostic port/adapter, focused tests, and this live record. The existing
unrelated `.gitignore` and `.mask.json` edits remain outside ownership.

Accepted evidence at `13b1dc53`: `application_github_window_read_host`,
`github_paged_read`, `github_provider`, `quill_logging`,
`quill_logging_backpressure`, `application_logging`, and recorded public paths
`github-com`/`uiforetw-74` pass under the dev preset. Format, documentation,
scoped board, and diff gates pass. Independent review found three gaps in the
first candidate; focused tests now prove the final attempted operation on a
transport failure, real pager `duplicate_identity` validation propagation, and
distinct numeric window sessions for colliding request numbers. The composed
logging fixture still refuses the unsafe cache path while showing the PR
entries; both public paths retain their pinned outcomes. No live GitHub or
account action occurred. Worker, reviewer, and commands are quiescent. Pending
only the acceptance-driven board finish for this card; BPR-0643 and full RFC
closeout remain outside this milestone.

## Acceptance evidence — `13b1dc53`

- Failure class and last paged operation: `github_paged_read` exercises a first
  network failure, later wire failure, and a final real snapshot validation
  refusal; `github_provider` checks terminal propagation; host and Quill tests
  check the `failure` and `page_operation` fields. One accepted load produces
  one `succeeded` record, and two windows retain distinct joined identities.
- Privacy: the unchanged sanitization and logging contracts pass. New fields
  are closed enums, numeric status/correlation, and typed failure values; no
  new text field accepts provider, repository or review content.
- Recorded fixtures: `application_logging` inspects the composed file for PR
  entries and preserves its unsafe-cache refusal. The `github-com` and
  `uiforetw-74` public-path courses pass with their existing pinned outcomes.

`logs/diagnostics.log` records `application`, `github.viewer`,
`github.capability` and `epoch` operations, but not the pull request load
itself. When the private alpha's owner-named read-only check on a large 2015
pull request ended in `The pull request could not be loaded.`, the log showed
the viewer and capability probes succeeding and then nothing, so the failure
class (`malformed_response`, `response_limit`, `rate_limit`, a mapped provider
failure, or a snapshot validation error) could not be recovered offline. The
observation is recorded on [BPR-0641](../40-parked/BPR-0641-enable-normal-inline-publication-for-private-alpha.md).

## Scope

Emit one structured `github.pull_request` diagnostic per load attempt with the
started, response_received (per paged operation and HTTP status), succeeded and
failed outcomes, carrying the `github_failure` and, for validation refusals,
the `github_read_validation_error` name in the existing `failure` field. Keep
the privacy rules of RFC-BPR-0027: no bodies, paths, logins, tokens or URLs
beyond the operation name. Owned paths: the application host's pull request
load path, the diagnostics adapter's operation vocabulary, and their focused
tests.

## Acceptance

- A failed load records the failure class and the last paged operation in the
  diagnostics log; a successful load records one succeeded entry.
- The diagnostics privacy contract tests pass unchanged; no new text fields
  carry user or repository content.
- The recorded-fixture load courses show the new entries without changing
  their pinned outcomes.
