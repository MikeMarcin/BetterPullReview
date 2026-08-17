---
id: BPR-0224
title: A pass regex made two gates unable to fail, and hid a broken Release seam
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# A pass regex made two gates unable to fail

Found while trying to demonstrate [[BPR-0222]]'s fix. The demonstration could
not be made, and the reason turned out to matter more than the fix.

## Measured

`application_composition_contract` was registered with

```cmake
PASS_REGULAR_EXPRESSION "OK"
```

When CTest is given that property it **ignores the process exit code** and
passes whenever the output matches. The suite prints a source excerpt
containing the literal `REVIEW_LEDGER_OK`, so the regex always matched:

```
$ <the exact command ctest runs>
Ran 2 tests in 65.775s
FAILED (failures=2)
TRUE_EXIT=1

$ ctest -R application_composition_contract
1/1 Test #125: application_composition_contract ...   Passed
```

A failing suite, reported as passing, every time.

`live_mutation_isolation_contract` — added five commits earlier by [[BPR-0221]]
— carried the same property. It happened to fail correctly when broken
deliberately, but only because its failure output contains no `OK` anywhere.
That is luck, not a design, and the next assertion added to it could have
removed the luck without anybody noticing.

## What it was hiding

With the property removed, `application_composition_contract` failed
immediately and honestly: **the tests-disabled Release build of `main.cpp` has
not compiled**. Three separate seam breaks, each revealed by fixing the one in
front of it.

| Break | Cause |
|---|---|
| `aRun.Fixture` and the review-ledger fields | the block sat outside its `BPR_BUILD_TESTING` guard, referring to members that only exist inside one |
| `notification_permission_name` | *defined* inside the guard, *used* outside it |
| `fixtureFactsPassed` | declared outside the guard with every use inside, so a tests-off build had an unused local under `-Werror` |

This predates the work that found it. The same three unguarded references exist
at `1639b9e`, the commit this session started from — verified rather than
assumed, by reading that revision of the file.

So `test_tests_off_release_graph_omits_public_path_fixture` has been unable to
prove anything: it builds a tests-disabled Release tree and asserts the fixture
is absent, and the build it depends on was failing before the assertion was
reached. The Release seam contract has been unverified for as long as that has
been true.

## Fixed

The property is removed from both tests. A `unittest` suite already exits
non-zero on failure, and that exit code is the verdict; overriding it with a
substring match can only lose information. Other tests in the tree pin specific
markers — `COMPOSED_HOSTS_OK`, `LIVE_MUTATION_ISOLATION_OK`,
`PUBLIC_PATH_OK` — which is a different and legitimate use: those are programs
whose marker *is* their contract, not suites with an exit code.

The three seam breaks are repaired so the contract can run to its assertion.

### Both directions

| Probe | Before | After |
|---|---|---|
| A genuinely failing composition suite | `Passed` | fails, naming the assertion |
| The tests-off Release build | never completed, unreported | compiles; the contract reaches its assertion and passes |

## What must not be done instead

Restoring the regex to keep the board green. The gate turning red is the gate
starting to work, and the failure behind it was real.

Pinning a tighter regex such as `^OK$` on a `unittest` suite. It would work
today and reintroduce the same class of defect the moment the suite prints
anything that happens to match. The exit code is already exact; nothing is
gained by re-deriving it from text.

Treating the Release seam repair as cosmetic. It is the contract that keeps
fixture and public-path seams out of a shipped Release binary, and it has not
been enforced.
