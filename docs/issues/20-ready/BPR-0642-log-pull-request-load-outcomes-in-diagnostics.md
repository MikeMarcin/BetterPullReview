---
schema: "implement-rfc.card.v1"
id: "BPR-0642"
title: "Log pull request load outcomes in diagnostics"
status: "Ready"
rfcs: ["RFC-BPR-0027"]
depends_on: []
priority: "P2"
---

# Log pull request load outcomes in diagnostics

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
