---
id: BPR-0405
title: Author the anonymous contract recorder and its authority packet
status: In Progress
rfc: RFC-BPR-0017
depends_on: [BPR-0297]
---

# Author the anonymous contract recorder and its authority packet

[[BPR-0298]] was parked under `awaiting-live-github`, and about half of it never
needed the network. Its own required final state names the tooling before it
names the capture:

> This card creates `scripts/record_rfc0017_anonymous_contracts.py` with the
> exact packet/response/manifest/output interface … validates the digest-bound
> authority and designated targets before any request … refuses unlisted hosts,
> repositories, operations, or outputs. Its `--validate-existing` mode performs
> no traffic.

None of that requires a live call, and the live call cannot be *requested*
without it: `docs/verification/RFC-BPR-0017.md:78` gates the course on the
operator recording a statement from an `operator-packet.md` that does not exist
yet. So the ordering is packet-before-response-before-traffic, and the first two
are ordinary unattended work sitting behind a lane that says a service is
missing.

This card takes that half. [[BPR-0298]] keeps the capture and now depends on
this.

## Required final state

- `scripts/record_rfc0017_anonymous_contracts.py` exists and exposes the exact
  interface the ledger already commits to at `docs/verification/RFC-BPR-0017.md:78`
  and `:81` — `--packet`, `--response`, `--course-manifest`, `--output-root`, and
  a `--validate-existing` mode that performs no traffic.
- It validates the digest-bound authority and the designated-target allowlist
  **before** any request, and refuses an unlisted host, repository, operation, or
  output path.
- Response bodies and headers are minimized, and the minimization is proven
  against forbidden byte patterns rather than asserted.
- `docs/verification/reviews/rfc0017-anonymous-probe-v001/operator-packet.md`
  exists, authorizing exact credential-free, read-only, bounded requests against
  named public targets, and separately naming any disposable private-repository
  creation or deletion authority.
- A directly runnable test file exercises the schema, allowlist, sanitizer, and
  `--validate-existing` path against synthetic fixtures.
- No live call is made and no captured byte is fabricated.

## The precedent to follow

RFC-BPR-0016 already solved this exact shape, and the split is worth copying
rather than reinventing:

| file | role |
|---|---|
| `scripts/capture_rfc0016_device_contracts.py` | offline validator/sanitizer. Its own header says it "deliberately contains no network client and never writes files" |
| `scripts/run_rfc0016_device_capture_attended.py` | the live runner, which imports the offline module and does the HTTP |
| `tests/scripts/rfc0016_device_capture_tests.py` | plain `unittest`, runnable as `python3 <file>` with no CMake involvement |
| `docs/verification/reviews/rfc0016-github-app-registration-v001/` | `operator-packet.md`, `operator-response.md`, `validation-record.json` |

The mechanisms to carry across: a digest-bound `AUTHORITY_SUBJECT_SHA256`
constant checked in every provenance validation; an `ALLOWED_URL_HOSTS`
allowlist whose validator also rejects non-`https`, embedded credentials, ports,
and stray fragments; a `FORBIDDEN_BYTE_PATTERNS` table enforced over every
persisted byte; exact-key schema enforcement so no undeclared field can leak
through; and a runner-digest binding so a mixed-runner assembly cannot pass.

## Three questions this card must answer before it writes much code

These are design decisions, not unknowns to be discovered, and picking wrong is
expensive. They are recorded here rather than assumed.

1. **One script or two.** The ledger prescribes a single
   `record_rfc0017_anonymous_contracts.py` entry point; RFC-0016 uses an
   offline-validator/live-runner pair. These are compatible — the prescribed
   entry point can be the offline validator, with the runner separate — but the
   choice should be made explicitly, because the runner digest has to bind
   something.
2. **`course-manifest.json` has no precedent anywhere in this repository.** The
   ledger names it as an input and nothing defines it. Its schema is new work,
   informed by RFC-0016's `outcomes/*.json` and `registration.json` shapes but
   not dictated by them.
3. **Where the synthetic rate-exhaustion fixture is seeded from — settled.**
   [[BPR-0298]] says it is "shaped from the recorded stable headers", and the
   ambiguity is real enough to have been worth naming: "recorded" could mean
   recorded by the live capture, or could mean the stable publicly documented
   header names.

   It means the capture's. Two things in the card decide it. The bullet directly
   above names "**observed** primary/search budget headers" as part of what the
   live records resolve, and "recorded" is the same verb that bullet uses for
   live output. And the clause that follows gives the reason the fixture is
   synthetic at all — "the live course never exhausts or intentionally evades
   the shared GitHub quota" — which is only coherent if the real headers were
   observed at some non-exhausted value and the exhausted one is then
   constructed from their shape.

   So the split is: **this card owns the injection mechanism and its validator
   schema**, seeded with the documented header names (`X-RateLimit-Limit`,
   `-Remaining`, `-Reset`, `-Used`, `-Resource`, `ETag`, `Last-Modified`), and
   **[[BPR-0298]] owns the fixture's values**, which come from its capture. This
   card can reach its own required final state without the network, which was
   the thing at risk.

   Recorded as an interpretation, not a measurement. If the implementing session
   finds a reading that defeats it, correct it here rather than working around
   it.

## Non-goals

- No live call, no credential, no network request, no repository creation or
  deletion. Every captured byte is [[BPR-0298]]'s.
- Do not fabricate a response body, a header value, or a course record. An
  invented capture is worse than a missing one.
- Do not register a CTest entry. `docs/verification/RFC-BPR-0017.md:78` invokes
  the script directly, the card's final state does not ask for one, and adding
  one forces a CMake reconfigure — which the dev-binary hold in the board README
  currently forbids. Direct `python3` invocation satisfies the requirement.
- Do not touch anything under `src/`.
- Do not close [[BPR-0298]] or make any RFC-BPR-0017 closure claim.

## Verification

```sh
python3 tests/scripts/record_rfc0017_anonymous_contracts_tests.py
python3 scripts/record_rfc0017_anonymous_contracts.py --validate-existing ...
python3 scripts/verify_documentation.py --root .
git diff --check
```

The documentation gate matters more than it looks here: `verify_documentation.py`
rejects a review directory whose name does not match `<gate>-vNNN`, and rejects
an `operator-packet.md` with no bolded `Status:` line. A new
`rfc0017-anonymous-probe-v001` trips neither rule as long as both are respected.
