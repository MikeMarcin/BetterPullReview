---
id: BPR-0248
title: Reconcile the composed-host verifier with the shipped application
status: In Progress
rfc: RFC-BPR-0013
depends_on: [BPR-0225, BPR-0228, BPR-0229, BPR-0230, BPR-0231, BPR-0247]
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
  fixture profile must require the test-only review mutation coordinator and
  reconciliation path; the production profile must not require or permit those
  fixture-only authorities. Both profiles must continue to forbid the live
  review mutation provider until BPR-0142 composes it.
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
ctest --test-dir build/dev -R '^(composed_application_hosts|documentation_structure|documentation_issue_graph_contract)$' --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```

The card remains In Progress until the previously reproduced
`composed_application_hosts` failure is green on the shipped development
binary, every fail-closed fixture is green, documentation is reconciled, and an
independent closeout review finds no blocking or high-risk gap.
