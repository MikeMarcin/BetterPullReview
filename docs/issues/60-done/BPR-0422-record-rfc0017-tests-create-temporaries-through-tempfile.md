---
id: BPR-0422
title: Two RFC-0017 recorder tests create their temporaries through tempfile
status: Done
rfc: RFC-BPR-0017
depends_on: []
---

# Two RFC-0017 recorder tests create their temporaries through tempfile

The first fresh Windows suite run of the `windows-lane` worktree failed
`contained_temporary_usage_contract`:

```
FAIL: test_no_test_creates_a_temporary_through_tempfile
tests/scripts/record_rfc0017_anonymous_contracts_tests.py:209: tempfile.TemporaryDirectory — use scripts/contained_temporary_support instead
tests/scripts/record_rfc0017_anonymous_contracts_tests.py:238: tempfile.TemporaryDirectory — use scripts/contained_temporary_support instead
```

This is not a Windows defect and not a container defect. The contract is an
AST scan over every test, it gives the same answer on every host, and the two
offending `setUp` methods were added by [[BPR-0405]] at `c161bf9` on
2026-08-20 — after [[BPR-0350]] had moved every other test onto the shared
helper and after the contract that enforces it existed. The contract did its
job; it had simply not been run since.

Measured uncontained on this host to make sure the attribution is not a
container symptom: the contract fails the same way outside the launcher.

## Required final state

- Both `setUp` methods hold a `ContainedTemporaryDirectory` from
  `scripts/contained_temporary_support`, which has the same `name` and
  `cleanup()` shape the tests already use, and `tempfile` is no longer
  imported.
- `record_rfc0017_anonymous_contracts_tests.py` still passes, so the
  replacement changed where the directory comes from and nothing else.
- `contained_temporary_usage_contract` passes.

## Verification

```sh
py -3 tests/scripts/record_rfc0017_anonymous_contracts_tests.py
py -3 tests/scripts/contained_temporary_usage_tests.py
ctest --preset core-only --output-on-failure -R '^(contained_temporary_usage_contract|rfc0017_anonymous_contract_recorder)$'
```

## Result

```
py -3 tests/scripts/record_rfc0017_anonymous_contracts_tests.py    36 tests, OK
py -3 tests/scripts/contained_temporary_usage_tests.py             6 tests, OK
```

The drop-in was exactly that: two assignments and one import changed, with
the `addCleanup(self.temporary.cleanup)` lines untouched.
