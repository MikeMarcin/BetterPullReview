---
id: BPR-0352
title: Bind supervisor and inventory identity into the approval subject
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0351]
completed: 2026-08-18
---

# Bind supervisor and inventory identity into the approval subject

[[BPR-0262]] fact_5: the approval subject binds the stage-one supervisor and the
in-image inventory/resource identities.

`designated_review_approval_subject` is a free function rather than a struct
(`src/application/src/review_mutation_execution_authority.hpp:116-120`, defined
`src/application/src/designated_review_authority_bootstrap.cpp:1461-1535`); it
renders a `key=value` text subject. It emits `schema_name`, `schema_version`,
`approval_statement`, `application_sha256`, `source_sha256`, `approval_id`,
`build_configuration`, `build_testing`, the three replay fields, and the fixed
target identity. None of them names a supervisor or an inventory.
`designated_review_bootstrap_request`
(`review_mutation_execution_authority.hpp:83-93`) has no field to carry one.

So an approval authorizes an application digest without saying which supervisor
or which capsule contents it was granted against.

## Required final state

- The subject binds supervisor identity and in-image inventory/resource
  identity, and the request carries both.
- `schema_version` advances, and an approval bound to the old subject is
  refused rather than silently reinterpreted.
- A raw image pathname hash may be recorded as release evidence but is not
  claimed as an FD-atomic DiskImages attach identity — fact_5 is explicit, and
  the packet records the `/dev/fd/N` limit that forced it.
- `application_designated_review_authority_bootstrap` covers the new binding.

## Result — 2026-08-18

The approval subject now names which supervisor and which capsule contents it
was granted against. `designated_review_bootstrap_request` carries both, and
`schema_version` advances from 2 to 3.

The version bump is the part that matters. An approval bound to the two-field
subject answered a question that said nothing about a supervisor; reading it
against a subject that now does would be reinterpreting an answer rather than
refusing a mismatch. Three makes it a mismatch.

### Where each digest comes from, and why they differ

**The supervisor digest is computed, not accepted.** The application derives the
supervisor's path from the fixed position it occupies inside its own capsule —
the same derivation [[BPR-0351]]'s handshake already uses to decide whose reply
to believe — and hashes it. Nothing tells the application this value, because
being told would make it a claim.

**The inventory digest is supplied by the supervisor.** It cannot be otherwise:
the inventory describes every file in the capsule, so it cannot live inside the
capsule without having to cover itself. The application therefore cannot find
it, and stage one — which verifies it — hands it over.

That asymmetry is safe in the only direction that matters. A supervisor that
supplied a wrong inventory digest would render a subject the operator never
approved, and the approval would be refused. It can cause a refusal; it cannot
cause an acceptance. Recorded here because the shape invites the opposite
assumption.

fact_5's other half is respected by omission: no raw image pathname hash is
claimed as an attach identity, because none is recorded at all. The packet's
`/dev/fd/N` limit stands unchallenged.

### Evidence

`application_designated_review_authority_bootstrap` covers both bindings
through the existing digest-bound identity matrix, which mutates one field of
the approval subject at a time and requires refusal before any observation. Two
rows were added for the new fields and the schema row advanced to 3.

Mutation-checked: removing `supervisor_sha256` from the emitted subject fails
"approval mismatch field exists". The binding is load-bearing rather than
decorative.

Ten selectors pass, including both architecture boundary checks, the
provenance contract, the SERVICE_SLOT handshake, the capsule contract, the
round-trip and ledger composition contracts, and the isolation manifest.
`coding-format-check` passes; `verify_documentation.py` returns
`DOCUMENTATION_OK`; the tests-disabled Release graph compiles the production
branch.

### Two pinned things this moved

The supervisor's launch call gained an argument, and the architecture boundary
checker pinned that call's exact text. Loosening the pin to
`launch_attended_application(` was wrong and the contract caught it within one
run: the bare name also matches the function's *definition*, which sits above
the observation, so the ordering check it exists for was inverted while still
appearing to pass its own fragment test. It now pins the call's argument list,
which occurs only at the call.

`architecture_boundary_tests.py` rewrites that same call in its reorder case,
and its literal had to follow. A stale literal there does not fail loudly — it
silently stops rewriting anything, and the case then proves nothing.

Both application digests were re-pinned, per the procedure [[BPR-0383]] left
next to them.

## Correction from the v005 critical replay — 2026-08-24

[[BPR-0448]] records a false-green check/use boundary this slice did not test.
The verifier and the subject-bound digest reopen the inventory pathname at
different times, so they can consume different inodes. Binding the digest as a
field remains correct; binding verification and hashing to one retained object
is additional required work.
