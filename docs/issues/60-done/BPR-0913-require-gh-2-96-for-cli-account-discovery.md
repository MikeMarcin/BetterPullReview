---
schema: "implement-rfc.card.v1"
id: "BPR-0913"
title: "Require gh 2.96 for CLI account discovery"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"supersedes_behavior_from":"BPR-0910","milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"},"owner_decision":"force GitHub CLI upgrade instead of parsing legacy human status"}
---

# BPR-0913: Require gh 2.96 for CLI account discovery

## Decision

The owner accepted the private `v0.1.2` probe but withdrew the `gh` 2.45
compatibility requirement. BetterPullReview should require the structured active
account inventory introduced in `gh` 2.96 and tell users with older versions to
upgrade.

The already-published private `v0.1.2` release remains immutable historical
evidence unless the owner separately asks to delete it. This card changes the
source after that release.

## Scope

- Remove the legacy human-status parser and the process runner's merged-stderr
  exception.
- Accept only the bounded JSON `gh auth status --active --json hosts` inventory
  on `gh` 2.96 or newer.
- Keep unsupported-version, process-failure and malformed-JSON presentation
  distinct. The unsupported message must state that `gh` is too old and require
  an upgrade to 2.96 or newer.
- Restore architecture, threat, development and RFC documentation to the hard
  2.96 discovery/API floor without rewriting BPR-0910 or the `v0.1.2` release
  notes.

## Acceptance

- A 2.45 version line is refused as unsupported before account status runs; a
  2.96 or newer version retains the JSON request and parser.
- No production request can merge or expose stderr, and no legacy account-status
  parsing code remains.
- The signed-out UI tells an older-CLI user to upgrade to `gh` 2.96 or newer and
  still distinguishes process and malformed-response failures.
- Focused adapter and Qt checks, formatting and documentation verification pass.

## Implementation

- Account discovery and API transport creation now reject every `gh` release
  below 2.96 before running account status. Supported versions use only the
  bounded `gh auth status --active --json hosts` request and JSON parser.
- The legacy human-status parser, format selector, merged-stderr request field,
  runner behavior and focused compatibility tests were removed. Process output
  is again separate-channel only.
- Signed-out presentation explicitly says the CLI is too old and requires an
  upgrade to `gh` 2.96 or newer while preserving distinct process-failure and
  malformed-response states.
- Current product, architecture, security, development and RFC documentation
  records the 2.96 floor. The completed BPR-0910 card and private `v0.1.2`
  release notes remain unchanged as historical evidence.

## Evidence

- `gh_cli_transport` covers refusal of 2.45 and 2.95, acceptance of 2.96 and
  2.100, the exact token-free JSON request and fail-closed JSON parsing.
- `ctest --preset dev --output-on-failure -R
  '^(github_cli_process_runner|gh_cli_transport|qt_workbench_window_scale_100|application_setup_destination_public_path)$'`
  passed all four focused tests.
- The focused build completed and `coding-format-check` passed across 699 files.
- `python3 scripts/verify_documentation.py --root .` returned
  `DOCUMENTATION_OK`; `python3 tests/scripts/verify_documentation_tests.py`
  passed 64 tests (its expected negative fixtures emitted validation errors).
