---
id: BPR-0440
title: The composition contract's digest pin and nested builds on Windows
status: Ready
rfc: RFC-BPR-0013
depends_on: []
---

# The composition contract's digest pin and nested builds on Windows

`application_composition_contract` fails on the Windows host uncontained, at
`22b991c`, on two distinct things.

**The digest pin did not follow BPR-0421.** The contract pins the content
digest of `src/app/src/github_application.cpp`, and [[BPR-0421]] reordered
two designators in that file from this branch without running the Qt graph,
which is the only graph that checks the pin:

```
CMake Error: github_application.cpp digest drifted
```

[[BPR-0436]] changes the same file again. The pin is re-recorded once, after
both, against the file as it then stands.

**The nested builds do not run here.** Three cases configure or compile
something under the test — `test_language_seal_assertion_rejects_what_does_not_prove_a_seal`
(`seal-control-compiles.cpp`, exit 2), `test_production_mutation_authority_issuers_are_language_sealed`
(`production-baseline.cpp`, exit 2), and
`test_tests_off_release_graph_omits_public_path_fixture`, which builds a
tests-disabled Release tree with Ninja and stops after
`[1/252] Scanning … appearance_contrast.cpp for CXX dependencies`. Exit 2
from a compile step that has not been read is not a cause; each needs its
first diagnostic captured and attributed, and the Release build's stop may
be the same Smart App Control wait [[BPR-0424]] records for `cmTC` probes.

## Required final state

- The `github_application.cpp` digest is re-pinned against the committed
  file and the contract passes that case.
- Each of the three nested-build failures is attributed from its own
  diagnostic; whatever is a test-harness assumption about the compiler
  environment is corrected, and whatever is the host's policy is recorded
  on [[BPR-0424]].
- `application_composition_contract` passes on Windows as registered and is
  unchanged elsewhere.

## Verification

```sh
ctest --preset windows-dev --output-on-failure -R '^application_composition_contract$'
```
