---
schema: "implement-rfc.card.v1"
id: "BPR-0248"
title: "Reconcile the composed-host verifier with the shipped application"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0225","BPR-0228","BPR-0229","BPR-0230","BPR-0231","BPR-0247"]
---

# Reconcile the composed-host verifier with the shipped application

The standing `composed_application_hosts` check is red because its absence
inventory predates the shipped ledger, review, defect, and queue composition.
It still treats `review_ledger_application_host`,
`review_mutation_coordinator`, and `defect_correlation` as forbidden even though
their completed cards intentionally linked them. It also exits successfully
when `nm` is unavailable and has no focused instrument tests, so a missing or
unreadable symbol tool can turn the binary half into a false green.

## Required final state

- Replace translation-unit and incidental substring guesses with exact,
  demangled symbol anchors for the hosts and decision functions that the
  application intentionally links.
- Distinguish the development/fixture binary from a production binary. The
  shared inventory may require the review mutation coordinator definition
  without claiming it is constructed. The fixture profile must additionally
  require the exact test-only GitHub fixture-provider boundary and
  reconciliation decision; production must forbid those fixture-only symbols.
  Both profiles must continue to forbid the live review mutation provider until
  BPR-0142 composes it.
- Recognize the completed production composition for the review ledger,
  defect correlation, queue enrollment/refresh, queue notifier, appearance,
  workbench, diff, epoch, GitHub, syntax, and portable agent host without using
  a broad token that an unrelated symbol can satisfy.
- Fail closed when no supported inspector exists, the inspector exits nonzero,
  the binary is missing, or the symbol table is empty. Use a portable ordered
  inspector chain that includes target-native Windows-capable tools rather than
  assuming POSIX `nm`.
- Add focused Python tests with synthetic inspector output and fake tools for
  every composed/forbidden direction, both profiles, no-tool, nonzero, empty,
  demangling, and positive-control cases. The registered real-binary contract
  must select one explicit profile and remain non-vacuous.
- Reconcile every affected RFC ledger and development note with the exact
  distinction between symbol presence, construction/runtime evidence, fixture
  mutation composition, and the still-absent live provider.

## Focused verification

```sh
python3 -m unittest tests/scripts/verify_composed_hosts_tests.py
ctest --test-dir build/dev -R '^(composed_host_verifier_contract|composed_application_hosts|application_composition_contract|documentation_structure|documentation_issue_graph_contract)$' --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```

## Implemented state

The verifier now parses demangled, implementation-bearing symbol definitions
instead of searching raw tool output. Required anchors match only the exact
normalized symbol-name start; undefined, absolute, common, debug, malformed,
embedded-template, and unknown records cannot satisfy them. Its fail-closed
inspector chain uses demangling-capable `llvm-nm`, `nm` plus a real demangler,
or `objdump -t -C`. Realistic GNU, Mach-O, COFF, and MSVC fixtures cover return
types, access/member/calling-convention prefixes, balanced nested declarations,
all angle-bearing C++ operator names, ABI metadata, and hostile lookalikes.

The development binary requires fifteen shared/fixture anchors and forbids the
live provider. The existing tests-disabled Release composition contract runs
the production profile against a freshly built real executable, requiring the
shared linkage inventory while forbidding the fixture-provider boundary,
fixture reconciliation, and live provider. The registered development test no
longer uses `PASS_REGULAR_EXPRESSION`, so its exit status cannot be overridden
by a success token embedded in a failure message. Symbol presence remains
explicitly linkage evidence; executable public-path contracts remain the
construction and runtime evidence.

Final verification passed 41/41 focused Python cases, the real development
binary through `llvm-nm`, 92,567 accepted real `llvm-objdump` records with zero
audit failures, both registered development checks, and the fresh production
Release contract in 79.23 seconds. Documentation, repository formatting,
Python compilation, simplification, and `git diff --check` are green.
Independent critical review found undefined/embedded-symbol, CTest exit-status,
MSVC declaration, symbol-kind/section, nested-template, and arrow-operator
false positives across four correction rounds; the final rereview returned
CLEAR with no blocking or high-risk finding.

Target-native Linux and Windows execution remains deferred as directed. Their
symbol formats have fixture coverage here, but this card makes no native-proof
claim.
