---
id: BPR-0062
title: Preserve review database identity in the read contract
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0040, BPR-0045]
completed: 2026-08-01
---

# Preserve review database identity in the read contract

Repair the normalized GitHub read contract exposed by BPR-0050's fresh Linux
slow matrix and reproduced after a fresh macOS relink. The production
normalizer correctly retains the recorded review database ID `4001`, which the
epoch host requires as the stable core review identity, but the independent
C++ expected read still default-initializes that field to zero. The paired
host manifests therefore agree with each other while the supposedly
independent expected object is stale.

Set the exact recorded identity in the independent expectation and add an
explicit nonzero/exact assertion so aggregate defaults cannot hide this
semantic field again. Rerun the GitHub read, provider, host, epoch-host, and
architecture contracts under Clang and GCC; run documentation/workflow/diff
gates; and resume BPR-0050 only after the remediation is committed.

## Result

The independent normalized-read expectation now carries the exact recorded
review database ID `4001`. Before whole-object equality, the contract
separately requires the GitHub.com result, GHES 3.18 result, and independent
expected object to contain exactly one review with that nonzero identity.
This makes the epoch host's stable review-identity input visible rather than
allowing aggregate default initialization to hide it.

## Evidence

- Fresh macOS arm64 Clang, ASan/UBSan, and Ubuntu 26.04 x86_64 GCC 15.2
  builds passed `github_read_contract`, `github_provider`,
  `application_github_host`, and `application_epoch_host` 4/4.
- The surrounding Linux slow group passed `gh_cli_transport`,
  `github_cli_process_runner`, `github_paged_read`, `github_provider`,
  `github_host_profile`, `github_capability_probe`, and
  `epoch_history_fixture_contract` 7/7; only the now-repaired independent
  read expectation had failed.
- The macOS and Ubuntu GCC architecture contracts each passed 2/2:
  `architecture_boundaries` and `architecture_boundary_contract`. The
  target-native QEMU run completed in 444.02 seconds.
- The focused simplification review retained the local exact-identity
  predicate because it keeps the semantic assertion adjacent to paired-host
  and whole-object equality without adding a production helper.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; tracked and on-disk GitHub Actions workflow counts are
  both zero; `git diff --check` passes.
