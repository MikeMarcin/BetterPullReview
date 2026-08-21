---
id: BPR-0364
title: The composed-host verifier has no Windows symbol inspector
status: Backlog
rfc: RFC-BPR-0013
depends_on: [BPR-0412]
---

# The composed-host verifier has no Windows symbol inspector

Split out of [[BPR-0361]], which fixed the same defect class in the
live-mutation guard and deliberately did not fix it here.

`scripts/verify_composed_hosts.py:582` raises:

```
no supported demangling symbol inspector is available
(llvm-nm, nm plus c++filt/llvm-cxxfilt, or objdump/llvm-objdump)
```

Its candidate list is POSIX-only. Unlike [[BPR-0361]]'s case this is not a wrong
selection but an absent one, and it does fail closed rather than reporting a
clean binary it never read.

Only `dumpbin` exists on this host. `llvm-nm`, `llvm-readobj`, `llvm-pdbutil`,
`llvm-objdump`, `nm`, `objdump`, `c++filt`, and `llvm-cxxfilt` are all absent
from a Visual Studio 18 developer environment, so any answer has to be built on
`dumpbin` or on adding a toolchain dependency.

## Why it is not simply [[BPR-0361]]'s fix again

[[BPR-0361]] reads the objects that composed each binary, taken from a
CMake-generated manifest keyed by `$<TARGET_OBJECTS:...>`. That works because
the live-mutation guard's subject is a set of CMake targets.

This verifier's subject is different in two ways:

- Its real gate, `composed_application_hosts` in
  `tests/end_to_end/CMakeLists.txt:958`, audits
  `$<TARGET_FILE:app_better_pull_review>` — the composed Qt application. That
  gate lives in the Qt graph, and per [[BPR-0347]] `BPR_BUILD_QT=ON` has never
  been run under the AppContainer boundary. There is no way to prove a fix here
  end to end until it has.
- The failing test in `core-only` is not the gate but the unit case
  `test_real_inspector_output_contains_defined_symbols`
  (`tests/scripts/verify_composed_hosts_tests.py:868`), which deliberately
  inspects `sys.executable` to prove the instrument really works against a real
  binary rather than a fixture. A Python executable has no object manifest and
  no COFF table, so that particular case needs its own answer — an artifact that
  does have a symbol table on the platform under test.

Also unlike the live-mutation guard, this verifier audits **defined** symbols
against both required and forbidden anchors (`audit_symbols`). Reading objects
rather than the image is a superset, which is the safe direction for a forbidden
anchor and the **unsafe** direction for a required one: an object may define a
symbol the linker then discards, so "required symbol present in the objects"
does not prove it is present in the shipped application. That asymmetry has to
be resolved deliberately, not inherited from [[BPR-0361]].

## Required final state

- `verify_composed_hosts.py` selects an inspector that returns a real symbol
  table for an MSVC artifact, or fails closed naming what is missing.
- The required-anchor direction is answered explicitly: either the inspection
  reads the linked image, or the narrower truth is recorded where the claim
  lives rather than a superset being presented as proof of presence.
- `test_real_inspector_output_contains_defined_symbols` inspects something that
  has a symbol table on the platform it runs on, and still fails if the
  inspector reads nothing.
- `composed_host_verifier_contract` passes in `core-only`, and
  `composed_application_hosts` passes in the Qt graph under the boundary.
- macOS and Linux selection is unchanged.

## Verification

```sh
ctest --preset core-only --output-on-failure -R '^composed_host_verifier_contract$'
py -3 tests/scripts/verify_composed_hosts_tests.py
```

Plus `composed_application_hosts` once the Qt graph runs under the boundary,
which [[BPR-0347]] owns.

`dumpbin` is only on `PATH` inside a Visual Studio developer environment. Any
selection that depends on it must say so, because a gate that silently degrades
outside `vcvars64.bat` is the same defect in a new place.

## The core-only half needed no decision — 2026-08-18

This card said its `core-only` failure could not be fixed without answering the
required-anchor question. That was wrong, and it was wrong the same way
[[BPR-0376]] was: by reading the defect and not the code around it.

The one failing case, `test_real_inspector_output_contains_defined_symbols`,
asserts that the instrument returns defined symbols for a real linked binary. On
Windows nothing can supply them — the MSVC linker consumes the COFF table, which
[[BPR-0361]] measured, and Visual Studio ships no `llvm-nm`, `nm`, or `objdump`.
`inspect_symbols` therefore raises `verification_error` and **fails closed**,
which is the correct behaviour and was never in question.

So the case now asserts the refusal where the capability does not exist and the
table where it does. It asserts a real property on both platforms rather than
skipping, and it does not touch what the gate proves.

`tests/scripts/verify_test_network_isolation_tests.py` already takes exactly
this position for `os.access(X_OK)`, and says so in almost these words:
"Asserting the refusal only where the permission exists keeps this suite honest
instead of asserting a property the platform cannot express."

```
py -3 tests/scripts/verify_composed_hosts_tests.py
Ran 42 tests
OK
```

`composed_host_verifier_contract` passes.

## What is left, and it is smaller

Only the real gate. `composed_application_hosts` audits
`$<TARGET_FILE:app_better_pull_review>`, lives in the Qt graph, and that graph
has never been run under the AppContainer boundary at all. It will fail closed
on Windows with `no supported demangling symbol inspector is available`, which
is honest but is not coverage.

The required-anchor asymmetry stated above is unchanged and still real: reading
objects is a safe superset for a forbidden anchor and an unsafe one for a
required anchor. That question only has to be answered when the Qt graph runs,
which [[BPR-0347]] owns, and it is the only reason this card is still open.

## The edge pointed at a card that disclaims the work — 2026-08-20

This card declared `depends_on: [BPR-0347]` because its last unmet bullet needs
the Qt graph running under the AppContainer boundary. [[BPR-0347]] does not do
that, and says so twice: "`BPR_BUILD_QT=ON` still has never been run under this
boundary, unchanged by any of this." So BPR-0347 could have closed exactly as
scoped with this card still waiting on something nobody owned.

The dependency now names [[BPR-0412]], which owns that run. The chain is one
step longer and one step truer.

Two other facts worth recording while the audit is fresh:

- **The core-only half of this card already landed**, at `bf12fa0`, while the
  card sat in `10-backlog/`. `test_real_inspector_output_contains_defined_symbols`
  now branches on platform capability and fails closed where it cannot read, and
  `composed_host_verifier_contract` is not blocked.
- **`scripts/verify_composed_hosts.py` still has no Windows candidate at all** —
  `inspect_symbols` offers only `llvm-nm`, `nm` with a demangler, and
  `objdump`/`llvm-objdump`. Bullet 1 is therefore met today by fail-closed
  behaviour rather than by an MSVC-aware selection, which is the honest reading
  and not what the bullet asks for.

Naming `dumpbin` explicitly — as tried and rejected, per [[BPR-0361]]'s
measurement that it "returns a section summary and no symbols" for a linked
image — is writable from any host and needs no relink. It is left here rather
than split out, because a candidate nobody can run end to end is the kind of
half-evidence this program has been correcting all week.
