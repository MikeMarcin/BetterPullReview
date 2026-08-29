---
id: BPR-0481
title: Close the review gaps in the RFC-0016 identity gate
status: In Progress
rfc: RFC-BPR-0016
depends_on: []
---

# Close the review gaps in the RFC-0016 identity gate

An independent review of [[BPR-0318]]'s verifier and [[BPR-0480]]'s runner
returned no blocking findings and four real gaps in what the verifier can
detect. All four were reproduced against the live tool before this card was
written; each admitted a course it should have refused:

```
course-execution WITHOUT capture_runner_sha256               ADMITTED probes=10
course-execution with FABRICATED capture_runner_sha256       ADMITTED probes=10
every probe request_contract_sha256 fabricated               ADMITTED probes=10
graphql_document naming a FOREIGN repository                 ADMITTED probes=10
undeclared extra + nested outcome files                      ADMITTED probes=10
```

The review's most useful observation is why the mutation sweep did not find
these: none of them is a condition that exists to be neutralized. A clean sweep
is strong evidence about the checks that are there and silent about the checks
that are missing, so the two instruments are complementary and neither
substitutes for the other.

## Required final state

- `capture_runner_sha256` is required, well formed, and equal to the digest of
  `scripts/run_rfc0016_production_course_attended.py`. The runner writes that
  field and both its docstring and [[BPR-0480]]'s card present it as the thing
  that makes the file attributable; the verifier must actually read it. This
  deliberately freezes the runner once a course is recorded, which is the same
  property that makes [[BPR-0276]]'s runner immutable and is why it is being
  established now, before the sitting rather than after.
- The probe list is compared for equality against
  `capture_rfc0016_device_contracts.expected_route_probes()`, which is what
  BPR-0276's own sanitizer does, so `request_contract_sha256` is recomputed
  rather than prefix-checked. The independently rebuilt designated catalog stays
  as a second derivation: two tables that must agree also detect drift in
  either.
- `graphql_document` equals the offline module's `GRAPHQL_DOCUMENT`. Pinning the
  variables is not enough, because a document may name a repository in
  hardcoded literals and never consult a variable.
- The course tree is bounded to its exact declared file set, so an undeclared
  file — including one nested under `outcomes/` — is refused rather than merely
  byte-scanned.
- The user-code shape is scanned in every evidence file, not only JSON. The
  attended prose files are where a transcribed code would plausibly land.
- `--validate-existing` proves each evidence path is tracked, not merely that
  `git status` is quiet about it, so a course written under an ignored path
  cannot pass.
- The test suite's GraphQL fixture is the real document. It currently differs
  from it, so the suite could not have detected substitution even in principle.

## Verification contract

- Every gap above gets a case that fails before the fix and passes after, driven
  through `check_course_evidence` on a tree that is valid except for the one
  defect.
- The four reproductions above are rerun and must all refuse.
- `rfc0016_production_identity_security` and `rfc0016_production_course_security`
  both still pass, and the runner's published output is still admitted by the
  verifier — the two tools must not drift apart while one is tightened.
- The whole-file mutation sweep is rerun on the verifier; new conditions must be
  killed by the new cases, and the survivor set must stay shape guards only.
