---
schema: "implement-rfc.card.v1"
id: "BPR-0432"
title: "The manifest gate lists no tests under a multi-configuration generator"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The manifest gate lists no tests under a multi-configuration generator

In the first Windows Qt-graph run ([[BPR-0412]]):

```
test_network_isolation_manifest ... Failed
TEST_NETWORK_ISOLATION_FAILED:CTest JSON contains no tests
```

`scripts/verify_test_network_isolation.py` reads the configured manifest
with `ctest --test-dir <build> --show-only=json-v1`. Under a
multi-configuration generator — `windows-dev` is Visual Studio — CTest lists
nothing until it is told which configuration, so the gate saw an empty
manifest and, correctly, refused. Every other graph this gate has run in is
single-configuration, where the omission is invisible.

The gate failed closed rather than open, which is the right side to be wrong
on, but a gate that cannot read the Qt graph on Windows is not verifying it.

## Required final state

- The verifier accepts `--build-config` and passes `--build-config <config>`
  to `ctest --show-only` when given; the registration supplies `$<CONFIG>`
  only when the generator is multi-configuration, so single-configuration
  trees register exactly what they did.
- The command assembly is a function with a unit case for both shapes.
- `test_network_isolation_manifest` passes in the Qt graph on Windows with
  the configured test count, which [[BPR-0412]] records, and still passes in
  `core-only`.

## Verification

```sh
py -3 tests/scripts/verify_test_network_isolation_tests.py
ctest --preset core-only --output-on-failure -R '^test_network_isolation_manifest$'
ctest --preset windows-dev --output-on-failure -R '^test_network_isolation_manifest$'
```

## Result

`manifest_command` builds the `ctest --show-only` invocation, adding
`--build-config <config>` when one is given, and the registration appends
`--build-config $<CONFIG>` only under a multi-configuration generator: the
`windows-dev` manifest shows the gate's command ending in `Debug`, and the
`core-only` one is unchanged. In the Qt graph under the boundary:

```
test_network_isolation_manifest ......   Passed    2.04 sec
TEST_NETWORK_ISOLATION_OK:configured=187:…:backend=windows-appcontainer
```

Both unit suites pass: 32 cases for the isolation verifier, including the
new command case, and the live-mutation guard's suite with its new header
case.
