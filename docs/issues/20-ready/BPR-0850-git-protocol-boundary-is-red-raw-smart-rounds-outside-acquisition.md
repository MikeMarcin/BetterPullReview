---
schema: "implement-rfc.card.v1"
id: "BPR-0850"
title: "git_protocol_boundary is red: two production files call raw smart rounds outside complete acquisition"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-004 BPR-0846 worker","found_at":"a5168c6d","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0850: git_protocol_boundary is red: two production files call raw smart rounds outside complete acquisition

## Defect

`git_protocol_boundary` (`tests/scripts/git_protocol_boundary_tests.py`,
`test_no_other_production_file_calls_raw_smart_rounds`) fails at a clean
export of `a5168c6d`
(`build/r2-logs/dsp4-0846-protocol-boundary-at-head.log`). It names two files:

- `src/adapters/github/src/github_provider.cpp` ~677-687: a guarded wrapper
  forwards `Inner->discover`, `Inner->ls_refs` and `Inner->fetch`. This
  arrived in `cdf6175c` ("Bridge opted-in gh reads to bounded Git objects",
  2026-09-13).
- `src/app/src/github_public_path_fixture.cpp` ~1478: `aTransport.fetch(...)`.
  This arrived in `1681994a` ("exercise default cache byte pressure on public
  path", 2026-09-12).

No card records the failure, so the row has been red since then.

## Scope

- Decide for each file whether it really bypasses complete acquisition or is
  a transport decorator or fixture that the rule should name. Then fix the
  code, or narrow the rule with a stated reason. Do not weaken the rule for
  other production files.
- The fixture file is pinned by the round-trip contract. A change there
  reseals with a recorded review.

## Acceptance

- `git_protocol_boundary` passes on macOS, and the decision is recorded here.
