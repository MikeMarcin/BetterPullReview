---
schema: "implement-rfc.card.v1"
id: "BPR-0910"
title: "Support gh 2.45 account discovery"
status: "Done"
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

## Implementation

- Version parsing selects the bounded legacy text inventory for `gh` 2.45
  through 2.95 and retains the active-host JSON inventory for 2.96 and newer.
- The legacy request is exactly `gh auth status`: it never requests a token,
  merges the command's human-status stderr into one bounded result only on a
  clean exit, and admits only one active successful login per canonical host.
  Token masks, scopes, protocol, credential source and all other text are
  discarded; actual-token shapes, controls, host drift and ambiguity fail
  closed.
- CLI account discovery and `gh api` transport creation now accept 2.45 or
  newer. The Git-object acquisition gate remains exactly `gh` 2.100.0 plus
  Apple Git 2.50.1 and therefore makes no full-comparison claim for 2.45.
- Signed-out presentation distinguishes unsupported version, process failure
  and malformed inventory.

## Evidence

- The official macOS arm64 `gh` 2.45.0 archive matched its published checksum.
  A write-denied, empty-environment probe returned the documented token-hidden
  human status shape successfully; no hostname, login, token, scope or private
  repository value from the live probe was retained in this card or test data.
- `ctest --preset dev --output-on-failure -R
  '^(github_cli_process_runner|gh_cli_transport|qt_workbench_window_scale_100|application_setup_destination_public_path)$'`
  passed all four focused tests.
- `cmake --build --preset dev --target coding-format-check` passed across 699
  files.
- `python3 scripts/verify_documentation.py --root .` returned
  `DOCUMENTATION_OK`; `python3 tests/scripts/verify_documentation_tests.py`
  passed 64 tests (its expected negative fixtures emitted validation errors).
