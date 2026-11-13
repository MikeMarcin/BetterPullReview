---
schema: "implement-rfc.card.v1"
id: "BPR-2103"
title: "Account for the two-hour clean release target"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2057", "BPR-2058", "BPR-2100"]
priority: "P1"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"b2df13b08c1cc5ea13a1e165d261042cbea1b2d3","evidence":{"kind":"observation","detail":"Overlap union, explicit wait CLI, invalid target/interval, pending uncertainty and locked terminal wait controls pass; no inferred human wait or acceptance."}},{"criterion":"AC2","outcome":"pass","revision":"b2df13b08c1cc5ea13a1e165d261042cbea1b2d3","evidence":{"kind":"observation","detail":"Actual macOS invalid-request refusal occurs before credentials/builds; empty arrays/forwarding and existing variant/build boundaries pass. Runbook retains complete native requirements and independent-host proof boundaries."}},{"criterion":"AC3","outcome":"pass","revision":"b2df13b08c1cc5ea13a1e165d261042cbea1b2d3","evidence":{"kind":"observation","detail":"38 runner,36 release-source,67 documentation tests; targeted documentation gate,board,Bash,Python,diff and independent review pass. No product build/benchmark/current-release write or measured speedup."}}]
---

# Two-hour clean release accounting

## Scope

Owner, 2026-11-11: aim for two hours on future clean releases. Preserve the
instruction to leave the active frozen release alone, improve main best effort,
and avoid a lengthy measurement campaign. Two hours is a target, not a measured
result or permission to narrow required qualification.

## Acceptance

- AC1: Extend the existing phase ledger with overlap-aware machine execution,
  explicitly recorded human/service/capacity waits and unattributed elapsed time.
  Open/failed attempts remain visible; accounting neither infers wait causes nor
  grants acceptance. Report the two-hour budget without cancelling work.
- AC2: Source-only request validation refuses invalid scopes/heavy IDs/regex
  before credentials/builds, while full native selection remains mandatory.
  The runbook distinguishes bounded eligible qualification from broad work,
  useful work during waits, verified independent native lanes and evidence needed
  before further concurrency. Retain required coverage, all failure receipts,
  physical-host exclusion and source/artifact/trust obligations.
- AC3: Focused accounting, CLI and documentation checks pass. No product build,
  benchmark campaign, active-release write, credentials move or unsupported
  performance claim is made.

## Result and focused verification

Implementation `b2df13b08c1cc5ea13a1e165d261042cbea1b2d3`:

- Source-only request admission rejects unknown heavy IDs, contradictory scopes
  and malformed regression syntax before Mac credential lookup or compilation.
  Full native inventory, matching, schedule, bootstrap and result checks remain.
- The existing ledger unions completed-command wall intervals, explicit
  human/service/capacity waits and their overlaps. Missing observation stays
  unattributed; open phases remain unreconciled. Wait closure is locked and
  repeat/unknown closure is refused. The 7,200-second budget is reporting only.
- The procedure prioritizes genuinely bounded releases under existing profiles,
  early native feedback, verified independent hosts and work during actual
  waits. Broad/comprehensive obligations and native performance locks remain.
- `python3 tests/scripts/test_suite_runner_tests.py`: 38 tests pass, including
  source-only no-child controls, overlapping intervals, explicit wait CLI,
  invalid intervals/targets and the existing native scheduling controls.
- `python3 tests/scripts/release_source_assets_tests.py`: 36 tests pass,
  including actual Mac invalid-request refusal before build/diagnostic creation,
  empty-array Bash compatibility, forwarding, all build boundaries and variants.
- `python3 scripts/verify_documentation.py --root .`: pass against this checkout
  using the unchanged main helper deployment; 67 documentation fixture tests pass.
- Scoped board validation, Bash syntax, Python compilation and `git diff --check`
  pass. Independent final review found no actionable correctness regression.

No product build, full native qualification, timing campaign, current-release
write, credential transfer or new host was performed. The retained Windows full
reports include failures and are not a clean-release benchmark. Two hours remains
a target, not an achieved result; no measured speedup is claimed.
