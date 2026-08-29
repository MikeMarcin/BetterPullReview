---
id: BPR-0480
title: Write the bounded production-course runner
status: In Progress
rfc: RFC-BPR-0016
depends_on: []
---

# Write the bounded production-course runner

[[BPR-0318]] registered the distributable GitHub App identity and committed
`scripts/verify_rfc0016_production_identity.py`, which defines exactly what the
designated read-only course must leave behind. Nothing in the tree can produce
that evidence. This card writes the tool that can, so the attended sitting has
something to run.

## Why this is not a change to the existing runner

`scripts/run_rfc0016_device_capture_attended.py` and its offline module
`scripts/capture_rfc0016_device_contracts.py` are bound to BPR-0276's accepted
authority in four places, and three of them are about the evidence rather than
the client:

- `validate_client_id` admits only `Iv23lil08fH3933w9MkW`.
- `load_registration_observation` requires an observation byte-identical to a
  hardcoded private shape, fixing `application_visibility: private` and
  `installation_policy: owner_only` — the two fields the distributable identity
  must invert. It refuses before the client check does.
- `make_provenance`/`_validate_provenance` hardcode BPR-0276's
  `authority_subject_sha256` and emit no `gate` key;
  `validate_registration_artifact` pins private/owner_only again.
- `_course_execution_document` writes BPR-0276's authority and no `gate`.

Widening those means threading a second registration shape, a second authority,
and a new key through two files whose own negative tests assert BPR-0276's shape
is exact. That is [[BPR-0318]]'s "never silently widened" non-goal approached
from the inside, and it puts an already-admitted capability-critical fixture at
risk for a course that shares none of its edge cases.

The transport half is separable and needs no change: `HttpsWire`,
`ReadOnlyProbeWire`, `_probe_requests`, `run_read_only_probes`,
`request_device_grant`, `poll_token`, and `run_authorization_course` all take
`client_id`, `wire`, and `probe` as parameters and read no pinned identity
except through `validate_client_id`; `_write_private` and
`_read_private_canonical_json` are generic. The single `token-success` course is
a closed branch demanding no slow-down reservation, no settings transition, and
no five-course bundle.

## Required final state

- A new bounded runner performs exactly one course — real device-flow token
  success for public client `Iv23liitiYN1WSSbag50` followed by the ten read-only
  catalog probes — and writes evidence a clean
  `verify_rfc0016_production_identity.py --require-complete` run admits.
- It reuses the existing wire, probe, and course helpers by import rather than
  by copy, so route closure stays defined once in `ALLOWED_URLS` and the probe
  catalog stays defined once in `_probe_requests`.
- BPR-0276's runner, offline module, admitted fixture, course evidence, and
  authority are unchanged. The private verification client is never accepted by
  the new runner and the production client is never accepted by the old one.
- No client secret, access token, refresh token, device code, user code, or
  cookie reaches source, settings, logs, diagnostics, evidence, process
  arguments, or environment. Transient fields are omitted by projection exactly
  as BPR-0276's evidence omits them.
- The runner refuses to write anything for a client, repository, pull request,
  route, or authority subject other than the ones
  `rfc0016-production-app-registration-v001` approved. It performs no mutation
  and reaches no undesignated target.
- Nothing in this card contacts GitHub. Executing the course is the attended
  sitting that [[BPR-0318]] owns, under the already-accepted authority; this card
  ends with the tool and its offline evidence proven against fixtures.

## Verification contract

- A new isolated gate, registered in `tests/CMakeLists.txt`, counted in
  `scripts/verify_rfc0016_test_inventory.py`'s `SELECTORS` and its graph-
  independent set, and named in `docs/verification/RFC-BPR-0016.md` — the same
  three places `rfc0016_production_identity_security` was added to, since the
  inventory refuses a selector the ledger does not name.
- The gate drives the runner against a stub wire through its real course path
  and asserts that the evidence it writes is admitted by
  `verify_rfc0016_production_identity.py`, so the two tools are proven to agree
  rather than assumed to.
- Route closure and identity closure carry negative cases: an unapproved client,
  an undesignated repository or pull request, a URL outside `ALLOWED_URLS`, a
  mutating method, and a foreign authority subject each fail closed.
- `ctest -R '^(rfc0016_device_capture_security|rfc0016_device_live_runner_security)$'`
  still passes unchanged, proving BPR-0276's tools were not disturbed.
