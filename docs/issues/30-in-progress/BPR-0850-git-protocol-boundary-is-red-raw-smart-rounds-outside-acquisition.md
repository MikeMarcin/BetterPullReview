---
schema: "implement-rfc.card.v1"
id: "BPR-0850"
title: "git_protocol_boundary is red: two production files call raw smart rounds outside complete acquisition"
status: "In Progress"
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

## Decision and fix (dsp-004)

Neither file bypasses complete acquisition (`acquire_git_protocol_v2_pack` in
`src/application/src/git_protocol_v2.cpp`), so the rule is narrowed and no
code changes.

- `github_provider.cpp`: `observed_git_transport` is a
  `git_smart_transport_port` decorator. Each of `discover`, `ls_refs` and
  `fetch` forwards through `perform_guarded`, which adds only stop, revoke and
  observed-account checks. The class is private to
  `cli_observed_account_transport`, and only that class's factory `acquire`
  constructs it, so acquisition still reaches every round.
- `github_public_path_fixture.cpp`: `verify_history_fetch_refusals` is a
  refusal probe compiled only under `BUILD_TESTING`, and the composition
  contract keeps it out of the tests-disabled graph. It sends deliberately
  invalid fetch bodies and requires `invalid_request` with no consumer calls.
  It must call `fetch` directly. The pinned fixture is unchanged, so there is
  no reseal.

`tests/scripts/git_protocol_boundary_tests.py` now carries a two-entry
allowance table, and each entry gives its reason. Each allowed call must
appear exactly once inside its one brace-balanced definition, which the rule
finds after blanking comments and literals. All of these still fail:

- any other raw round;
- a repeat;
- a different receiver or round;
- the same text in another file;
- a missing or repeated scope.

The private-helper check ignores the allowances.
`docs/verification/fixture-catalog.md` names both exceptions.

**Evidence.**
- The script passes 18/18 at `156fde6b` and on a clean export of that
  commit's `src/`. The previous rule fails there.
- The synthetic matcher tests fail as they should, and so do the negative
  controls on scratch copies of both real files and the five matcher mutations
  (`build/r2-logs/dsp4-0850-{before,after,head-export,negative-control,mutations}.log`).
- The integrator reran the script and the ctest row at `65dfc318`: both pass
  (`dsp4-0850-integrator-run.log`, `dsp4-0850-ctest.log`).
