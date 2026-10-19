# BPR-1620 verification

Assignment `external-determinism-v1/architecture-policy`, mandate
`external-determinism-r1`, leaf `/root/external_determinism_docs`.
Baseline `80f280ee8c9946b98c6527a79c65c878d0a6a4d1`; exact policy candidate `4773277f981d429c29862e297c0e31c3d5e90335`.

## Acceptance and self-review

- AC1 passes. The base architecture §2.1 is the authoritative semantic rule.
  Root/core instructions, README, CONTRIBUTING, product/system/GitHub-host
  architecture and build guidance link to it or its implementation inventory.
  20 new rule/inventory links resolve to the actual current headings.
  The shortest public reading path is README Architecture → external determinism;
  the rule then links to approved implementation constraints.
- AC2 passes. The inventory separately retains owner approval, pins, licenses,
  exact private include/link/use ownership, dtl's two bounded uses, SHA-1
  collision detection, xdiff dynamic replacement/relinking, RE2 no-worker and
  fatal-diagnostic limitations, and SHA-256 wrapper/failure behavior. Host
  effects, scheduling/cancellation/lifecycle and Qt independence remain.
  The general rule qualifies resource failure, stop observations, native lexical
  paths and stdlib random mappings. No library or API capability is adopted.
  Existing RFC assets and plans are byte-unchanged except the new additive plan;
  current RFCs link the adopted amendment without rewriting prior answers/evidence.
- AC3 passes. All focused commands below exit 0 at the exact candidate. The nine
  changed CMake files differ only in full-line comments; removing those comments
  yields identical executable content before/after. No C++ implementation,
  dependency pin, target, scanner pattern or linkage changed. `git diff --check`
  passes. Independent/native/human acceptance is not required for this prose scope.

## Focused commands

| Command | Result | Seconds |
|---|---|---|
| `python3 scripts/verify_documentation.py --root .` | DOCUMENTATION_OK | 1.358 |
| `python3 tests/scripts/verify_documentation_tests.py` | 67/67; printed fixture ERROR lines are expected negative controls | 2.079 |
| `cmake -DBPR_ROOT=/Users/jaeger/Documents/GitHub/BetterPullReview -P cmake/VerifyCoreBoundary.cmake` | verified | 0.897 |
| `cmake -DBPR_ROOT=/Users/jaeger/Documents/GitHub/BetterPullReview -P cmake/VerifyArchitectureBoundaries.cmake` | verified | 24.663 |
| `python3 tests/scripts/core_boundary_tests.py` | 11/11 | 1.175 |
| `python3 tests/scripts/architecture_boundary_tests.py` with the ten selectors below | 10/10 | 5.801 |

Focused `ArchitectureBoundaryTests` selectors:

```text
test_sha1_identities_have_one_collision_detecting_owner
test_sha256_has_one_hacl_owner
test_line_pair_similarity_has_one_rapidfuzz_owner
test_linked_xdiff_has_one_private_core_owner
test_dtl_has_exactly_two_owners
test_platform_sha256_has_exact_native_and_adapter_owners
test_core_application_and_qt_reject_process_and_network_types
test_core_and_qt_reject_raw_provider_data
test_core_rejects_application_provider_headers
test_core_and_qt_reject_credential_vault_contract_types
```

Existing core contracts cover private RE2 ownership, approved links, arithmetic,
explicit seeded/synchronized state and lexical paths, plus negative controls for
clock/entropy/filesystem/provider/OS effect leakage. Architecture contracts cover
the retained exact implementation ownership. These source/build guards do not
prove all upstream internals or universal resource-failure equivalence.

No application build, exhaustive mutation matrix, native/release qualification,
remote action or desktop observation is needed or claimed for this documentation
slice. Original Windows/Linux evaluation and broader package/vault/crash/security
obligations remain on their existing cards. Useful command receipts are retained
under `/tmp/bpr-external-determinism`; this summary is the durable evidence.
