---
schema: "implement-rfc.card.v1"
id: "BPR-0481"
title: "Close the review gaps in the RFC-0016 identity gate"
status: "Done"
rfcs: ["RFC-BPR-0016"]
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

## Closed — 2026-08-29

Every reproduction now refuses, each on its own diagnostic rather than on a
neighbour's:

```
course-execution WITHOUT capture_runner_sha256   course-execution:not-produced-by-the-recorded-course-runner
course-execution with FABRICATED digest          course-execution:not-produced-by-the-recorded-course-runner
every probe request digest fabricated            route-probes:probes-not-the-recomputed-designated-table
graphql_document naming a FOREIGN repository     route-probes:graphql-document-not-the-designated-query
undeclared extra + nested outcome files          course-tree:undeclared-file:[...]
valid course                                     ADMITTED probes=10
```

Two decisions inside the fix are worth recording.

The recomputed-probe-table check runs **after** the per-probe loop, not before.
Placing it first would have made every specific diagnostic — which probe, which
field — unreachable, and turned the whole loop into dead code that the next
mutation sweep would have reported as survivors. Last, it is an exhaustive
backstop over checks that still say what went wrong.

The test fixture now uses the real `GRAPHQL_DOCUMENT` and the real
`expected_route_probes()` rather than lookalikes it defined itself. That was the
root of finding 3: a fixture that invents its own copy of the thing it stands in
for cannot detect substitution of it, even in principle, so the gap was
invisible to a suite that otherwise looked thorough.

Sweep after: 84 of 106 refusals turn the 122-test gate red. The 22 survivors are
shape and existence guards plus the offline-loader guard and
`revision.returncode`, which the review adjudicated as non-load-bearing because
the length check on the next line already covers it. Reordering `check_committed`
briefly uncovered `git status`'s return code — previously killed by the
not-a-repository case, which now stops at `git ls-files` — so that path is
covered by a mocked failure, the only way to make the second Git call fail while
the first succeeds.

The two tools still agree: the runner's stub-driven publish is admitted end to
end, and both BPR-0276 suites remain green.

## Reopened by its own re-review, and closed again — 2026-08-29

The re-review verified all four fixes close their findings, confirmed the
recomputed-table check is a strict superset of the per-probe loop (it also pins
order and exact key sets, which the loop cannot see), and — the useful part —
empirically checked the risk the ordering creates. Because equality subsumes the
loop, the loop's own checks could have become mutation survivors, silently
non-load-bearing. They did not: nine were neutralized individually and every one
still turns the gate red, because the tests assert the specific diagnostic
strings. That outcome was not guaranteed and is now known rather than assumed.

It also found the real gap. Requiring a course to name this runner's digest
makes **this** runner evidence the moment a course is committed — but nothing
said so, because the identity fixture recomputes the digest from the live file
and therefore always agrees with itself. An edit would have left every ctest
gate green and broken only the ledger's manual run: precisely the failure
[[BPR-0480]] paid a debugging session to learn and then pinned for BPR-0276's
runner.

The recommendation was a follow-up card for after the sitting. A self-arming
test is better and needs no one to remember: it reads the *recorded* course
rather than recomputing, so it is inert while no course exists and load-bearing
the moment one lands. Its disarmed branch asserts that no course directory
exists at all, so a half-written course cannot quietly disarm the guard it is
supposed to arm. Verified in all four states — no course passes, a directory
without an execution record fails, a course naming another runner fails with the
self-explaining message, and a course naming this runner passes.

Also from the re-review: the verifier's docstring now says that the one other
file it loads opens no socket either, and the ledger row no longer cites a sweep
count it cannot keep current — that number had gone stale twice, so the measured
figures live here instead.

Final measurements at this commit: the verifier, 84 of 106 refusals turn its
122-test gate red; the runner, 15 of 20 turn its 39-test gate red. Survivors in
both are shape and existence guards, plus the offline-loader guard and
`revision.returncode`, whose next line already refuses.
