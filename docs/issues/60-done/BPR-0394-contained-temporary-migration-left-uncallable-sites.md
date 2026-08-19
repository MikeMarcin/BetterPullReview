---
id: BPR-0394
title: The contained-temporary migration left call sites that cannot execute
status: Done
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

## Result — 2026-08-19

`unix_loopback` passes `parent=configured_root`. One character of substance.

### The audit

Ten call sites pass an argument at all; the rest pass none and cannot be wrong.
Of those ten, nine pass only `prefix=`, which is accepted. The
`network_isolation_counterexamples.py` site was the only signature disagreement
in the tree.

`tempfile` under `tests/`: one directory-creating use, BPR-0373's, now repaired
by that card. `verify_gh_cli_trust.py:44` calls `tempfile.gettempdir()`, which
reads a path and creates nothing.

Two uses outside `tests/` are deliberately left alone, and it is worth saying why
rather than listing them as exceptions. `scripts/run_network_isolated_test.py:176`
is the launcher that *builds* the sandbox: it runs outside the container it
creates, so the constraint does not apply to it. `scripts/designated_live_macos_capsule.py:656`
is a tests-disabled macOS packaging path, and the AppContainer is a Windows
object. `scripts/dif_assets.py` carries its own contained implementation and
documents the same reasoning at `:1111`.

### The gate

`contained_temporary_usage_contract` parses every `tests/**/*.py` with `ast` and
fails when a call passes a keyword the helper does not accept, or when a test
creates a temporary directory through `tempfile`. It reads the accepted keywords
**out of the support module** rather than restating them, so adding a parameter
there cannot leave the check stale, and it asserts that its own scan found more
than twenty call sites so a scan that silently matched nothing cannot pass
vacuously.

It answers the card's fourth bullet directly. Both defects are runtime errors in
untaken branches, so nothing short of executing that exact branch reports them —
`compileall` would not. Checking the shape costs 0.27 s and runs everywhere.

Mutation-checked against the two real defects, re-introduced one at a time:

| Seed | Gate output |
|---|---|
| `contained_temporary_directory(dir=configured_root)` | `network_isolation_counterexamples.py:254: contained_temporary_directory(dir=...) — accepted: ['parent', 'prefix', 'suffix']` |
| `tempfile.TemporaryDirectory()` in the dependency tests | `dependency_modernization_tests.py:472: tempfile.TemporaryDirectory — use scripts/contained_temporary_support instead` |
| both reverted | 4 tests, OK |

It names the file, the line, and what was accepted instead — which is what would
have turned either of these from a matrix archaeology exercise into a one-line
read.

### Evidence

| Check | Result |
|---|---|
| `ctest --preset dev -R '^test_network_isolation_(loopback\|unrelated)$'` | 2/2 passed |
| `ctest --preset dev -R '^contained_temporary_usage_contract$'` | passed |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
