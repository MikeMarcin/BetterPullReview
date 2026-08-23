---
id: BPR-0437
title: The Git contract fixtures construct the adapter without the Windows system root
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# The Git contract fixtures construct the adapter without the Windows system root

Uncontained on the Windows host, at `22b991c`:

```
git_process_contract       FAIL: construct installed Git adapter, failure=0
git_cache_contract         diagnostic cache result: value=0 code=2 detail=[]
                           FAIL: cache open preserves the exact redacted Git bootstrap diagnostic
```

`make_installed_git_process` (`src/adapters/git/src/installed_git_process.cpp`)
requires, under `Q_OS_WIN`, a `WindowsSystemRoot` in its configuration and
returns `invalid_configuration` — failure code 0 — without one. The contract's
`make_adapter` (`tests/git_contract/git_process_contract_tests.cpp:897-903`)
passes only `GitExecutable` and `IsolationRoot`, the two fields a POSIX
adapter needs. The fixture was written where the third field does not
exist, and no run before [[BPR-0412]] could reach it.

`git_cache_contract`'s failure is in the same family by shape — the cache is
built on the same adapter — but its assertion is about a redacted bootstrap
diagnostic, and it is listed here to be separated, not assumed.

## Required final state

- The Git contract fixtures supply `WindowsSystemRoot` on Windows from the
  same source the application uses, and the adapter constructs.
- `git_cache_contract`'s failure is attributed on its own evidence; if it is
  the same omission it closes here, otherwise it gets its own card.
- Both contracts pass on Windows as registered, and are unchanged on macOS
  and Linux.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^(git_process_contract|git_cache_contract)$'
```
