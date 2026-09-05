---
schema: "implement-rfc.card.v1"
id: "BPR-0480"
title: "Write the bounded production-course runner"
status: "Done"
rfcs: ["RFC-BPR-0016"]
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

## The runner exists, and the reason it is separate got sharper — 2026-08-29

`scripts/run_rfc0016_production_course_attended.py` and its isolated
`rfc0016_production_course_security` gate are written, registered in
`tests/CMakeLists.txt`, counted by `scripts/verify_rfc0016_test_inventory.py` in
both `dev` and `core-only`, and named in the ledger row they serve.

This card argued the separation from provenance shape. The real reason turned
out to be stronger, and it was found the direct way: the parameterization was
attempted first, and `rfc0016_device_capture_security` immediately failed with
`course execution identity mismatch`. BPR-0276's admitted `course-execution.json`
records `capture_runner_sha256`, and that value is the hash of
`run_rfc0016_device_capture_attended.py` itself. **That file is evidence.**
Editing it by any amount invalidates an attended capture the owner has already
accepted. The change was reverted, both BPR-0276 suites are green again, and the
digest is now pinned by a test whose failure message says so, so the next session
to reach for that file learns it in one run instead of one debugging session.

What is shared is what can be: `HttpsWire`, `ReadOnlyProbeWire`,
`_probe_requests`, `_validate_probe_reply`, and the JSON/field validators are
imported and run unchanged, so route closure keeps one definition. Only the
device grant, the token poll, and the token-success loop are reimplemented,
because their client pin cannot be widened in place. Each tool refuses the
other's identity, and a test asserts both directions.

The self-check is the part worth keeping. Evidence is built in memory, staged,
and then admitted by `verify_rfc0016_production_identity.py` before anything is
renamed into place; a rejected course leaves neither output nor staging behind.
It paid immediately: the verifier was requiring HTTP 200 for
`authorization_pending`, which GitHub is entitled to send as 400, so the gate
would have failed on a correct sitting. That is fixed and pinned in both
directions — a pending error may be 200 or 400, a token success may only be 200.

Falsification: each of the runner's 21 refusals was neutralized in turn and 15
turn the 38-test gate red. The survivors are an importlib loader guard and four
redundant deadline checks where an adjacent guard fires first, which the two
deadline tests demonstrate. Hostile-response cases cover what the real service
could send — a non-200 grant, an extra field, a redirected verification URI, a
malformed user code, an unknown token error, a wrong error schema, a refused
status, and a clock that jumps past the grant's life mid-poll.

Remaining: nothing unattended. The sitting is [[BPR-0318]]'s.

## A residual the closeout reviewer should know — 2026-08-29

`route-probes.json` is not an observation. `run_read_only_probes` performs all
ten probes through `ReadOnlyProbeWire` and validates every reply, and only then
returns `expected_route_probes()` — a constant table in
`capture_rfc0016_device_contracts.py` whose `http_status: 200` and
`result: "success"` are literals, not recorded values.

Nothing about that is wrong: the table is returned only if every real probe
succeeded, because `ReadOnlyProbeWire.perform` raises on any non-200, wrong
final URL, or wrong media type, and `_validate_probe_reply` raises on unexpected
content. The run proves the claim. But the *file* does not, and two consequences
follow that are worth stating rather than discovering later:

- A reader of `route-probes.json` alone cannot distinguish "these ten probes ran
  and passed" from "this table was written unconditionally". The proof lives in
  the execution path.
- `verify_rfc0016_production_identity.py`'s `result == "success"` and
  `http_status == 200` checks are therefore not falsifiable by a real run. They
  catch hand-edited, foreign, or fabricated evidence, which is what they are for,
  and they will never catch a failed probe — a failed probe stops the course
  before any file is written.

This is inherited from [[BPR-0276]]'s accepted design; its own admitted
`route-probes.json` has the same property. It is recorded here so that
[[BPR-0285]]'s capability-critical closeout review adjudicates it deliberately
rather than reading the file as an observation.
