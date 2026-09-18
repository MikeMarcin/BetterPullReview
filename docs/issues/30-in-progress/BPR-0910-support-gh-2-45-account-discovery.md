---
schema: "implement-rfc.card.v1"
id: "BPR-0910"
title: "Support gh 2.45 account discovery"
status: "In Progress"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"owner private v0.1.1 GHES test","milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"},"organization_standard_gh":"2.45.0"}
---

# BPR-0910: Support gh 2.45 account discovery

## Defect

The organization-standard GitHub CLI is `gh 2.45.0`. Its successful `gh auth
status` reports the active GitHub Enterprise account, but it predates the
`--active --json hosts` flags used by BetterPullReview v0.1.1. The application
therefore reports a generic discovery failure even though the configured CLI
credential is healthy.

## Scope

- Keep the bounded JSON inventory for `gh` versions that support it.
- For `gh 2.45`, use the exact token-hidden human status command under the same
  macOS file-write denial. Parse only a successful active host/login pair and
  discard masked-token, scope, protocol and source lines.
- Fail closed on color/control sequences, malformed or ambiguous sections,
  inactive-only accounts, failure/timeout entries, size limits and unsupported
  versions. Never pass `--show-token`.
- Report an unsupported CLI version distinctly from a process or malformed
  inventory failure.
- Keep the separately qualified full-comparison Git-object version gate
  explicit; account discovery compatibility must not silently claim that `gh
  2.45` satisfies the `gh 2.100.0` clone qualification.

## Acceptance

- Official `gh 2.45.0` token-hidden status output for GitHub.com and GHES is
  admitted through a bounded, fail-closed parser without retaining token or
  scope text in the typed result.
- Newer CLIs keep the JSON request and parser; neither path uses
  `--show-token`, changes active accounts or reads a token.
- The signed-out application distinguishes unsupported version, inventory
  process failure and malformed inventory, while a valid 2.45 inventory creates
  the same exact-host CLI card.
- Focused adapter, Qt and setup public-path checks, formatting and documentation
  verification pass.
