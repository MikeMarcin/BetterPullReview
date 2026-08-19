---
id: BPR-0394
title: The contained-temporary migration left call sites that cannot execute
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# The contained-temporary migration left call sites that cannot execute

`test_network_isolation_loopback` and `test_network_isolation_unrelated` fail at
`HEAD`. Found in a full `dev` matrix run, which is the only place they surface:
the focused selectors a card names do not include them.

```
File "tests/scripts/network_isolation_counterexamples.py", line 254, in unix_loopback
    contained_temporary_directory(dir=configured_root)
TypeError: contained_temporary_directory() got an unexpected keyword argument 'dir'
```

`contained_temporary_directory` takes keyword-only `prefix`, `suffix`, and
`parent` (`scripts/contained_temporary_support.py`). `dir=` is `tempfile.mkdtemp`'s
parameter name, not this one's. The call site is the `BPR_TEST_SOCKET_ROOT`
branch, so it fires whenever the isolation launcher supplies a socket root —
which both failing tests do.

## This is a second instance, not an isolated slip

[[BPR-0373]] carries the first: `dependency_modernization_tests.py:394` calls
`tempfile.TemporaryDirectory()` in a file that never imports `tempfile`, so its
`test_qt_provider_is_the_default_acquisition_path` has never executed.

Both are the same mistake in opposite directions — one kept the standard-library
call and lost the import, the other kept the standard-library *parameter name* and
lost the signature — and both were introduced by adopting the contained-temporary
convention without running the suites that use it. `contained_temporary_support.py`
exists because `tempfile`'s owner-only descriptor is unreadable inside a Windows
AppContainer ([[BPR-0350]]), so a site that still reaches for `tempfile` is not
merely inconsistent; it is the form that cannot work there.

Two wrong out of the call sites that pass an argument at all is enough to audit
the rest rather than fix the one the traceback named.

## Required final state

- `unix_loopback` passes its socket root through the parameter the function
  actually has, and `test_network_isolation_loopback` and
  `test_network_isolation_unrelated` pass.
- Every `contained_temporary_directory`, `ContainedTemporaryDirectory`, and
  `make_contained_temporary_directory` call site in the tree is checked against
  the real signature, and any other disagreement is repaired or recorded. A site
  that passes no arguments cannot be wrong; the ones that do are the population.
- A `tempfile.TemporaryDirectory` / `tempfile.mkdtemp` call anywhere under
  `tests/` that should be the contained form is named. The support module's own
  use of `tempfile` is correct and is not a finding.
- The failure would have been caught earlier: state where a check for this class
  belongs, or record why the full matrix is the honest answer.

## Not in scope

BPR-0373's own site — that card owns it, together with the Qt-policy assertions
it sits among.

## Verification

```sh
ctest --preset dev -R '^test_network_isolation_(loopback|unrelated)$' --output-on-failure
```
