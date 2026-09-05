---
schema: "implement-rfc.card.v1"
id: "BPR-0539"
title: "Make the registered device course enroll the grant, not the connect credential"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: ["BPR-0536"]
---

# Make the registered device course enroll the grant, not the connect credential

## Why

While composing the refresh course, [[BPR-0536]]'s third part traced the
vault under `application_device_authorization_registered_course` and found
that its `DEVICE_FLOW_VAULT_STORES:1` counts the fixture's own connect
credential — 32 random bytes from `make_fixture_credential`, stored when the
course's `stage::start` triggers the connect action before the grant — and
that the grant's own `enroll_profile` then arrives as a second
establishment whose superseding delete the fixture vault refuses with
`invalid_request`, because its delete matched the whole operation context
(request and identity generation) where a real vault matches the authority.
The row's headline claim, that the recorded grant reaches `enroll_profile`
through the composed application and the vault records exactly one store,
is therefore satisfied by coincidence: the store it counts is not the grant.
The refresh course fixed all of this under its own flag only, so the
committed course's regex and semantics did not change beneath it.

## Required final state

- Under the registered course the grant is the first session: the course
  does not connect before the grant, or the fixture vault's delete matches
  by authority for the registered course too; the one vault store the row
  counts is the grant's own material, proven by the stored token's identity
  in the course's evidence.
- `DEVICE_FLOW_VAULT_STORES:1` keeps its meaning and `DURABLE_COPIES:0`
  stays; the row's regex changes only if the evidence it names changes, and
  the ledger's "Registered-contract vault course" row is corrected to say
  what the store is.
- A negative control proves the count is the grant: with the grant's
  enrollment skipped, the row fails rather than passing on the connect
  credential.

## Non-goals

- Changing the refresh course, which already enrolls the grant as its first
  session under its own flag.

## Lane — 2026-09-03

Claimed into `30-in-progress/` by the integrator under the transition
exception once [[BPR-0536]] closed (a first claim while it was still in
progress was refused by the board's rule and returned). The slice runs on
`bpr-0539-work` in a worktree and follows the refresh course's own pattern
— no connect before the grant, delete by authority — extended to the
registered course, with a marker that proves the stored material is the
grant and a negative control that fails the row when the grant's
enrollment is skipped.

## What this slice delivered — 2026-09-03

At `4785bafd`. No course that runs the recorded grant connects first:
`src/app/main.cpp`'s start stage widened its skip-connect gate from the
refresh course to every registered course, so the grant is the first
session and its enrollment is the establishment, not a superseding one.
The fixture vault (`src/app/src/github_public_path_fixture.cpp`) takes
two flags — `RefreshSlot`, unchanged and refresh-course only, and
`AuthorityKeyed`, true for both device courses — and matches `retrieve`
and `delete_credential` by authority under the second, as a real vault
does; off both flags every guard is what it was. `store` records a
truncated SHA-256 of the bytes it accepted, taken at the store so a
superseding delete cannot erase the evidence, and the fixture's evidence
compares it with the transport's issued access token.

The course prints, ahead of the counts and only outside the browser
probe, `DEVICE_FLOW_VAULT_STORED_TOKEN:<sha256 prefix>` and
`DEVICE_FLOW_STORED_IS_GRANT:<bool>`; `DEVICE_FLOW_ENROLLED_OK` now
requires the grant wherever the marker is printed, and the registered
row's regex pins `DEVICE_FLOW_VAULT_STORED_TOKEN:f5505a4b89c07e21`
(`printf '%s' fixture-access-token-0276 | shasum -a 256`) and
`DEVICE_FLOW_STORED_IS_GRANT:true` between `REQUESTS:0` and
`ENROLLMENTS:1`. `VAULT_STORES:1` and `DURABLE_COPIES:0` keep their
values and, now, their meaning; the panel rows read from `ENROLLMENTS`
on and pass unedited.

Negative control, applied and restored: with the connect restored and
the grant's enrollment dropped, the row prints
`DEVICE_FLOW_VAULT_STORED_TOKEN:1be63db5c4f8af3a`,
`DEVICE_FLOW_STORED_IS_GRANT:false`, then the old evidence
(`ENROLLMENTS:1`, `VAULT_STORES:1`, `TRANSPORT_SUBMISSIONS:2`) and
`DEVICE_FLOW_ENROLLED_FAILED` — the store a random per-run connect
credential, refused by the new evidence and accepted by the old.

The ledger's registered-contract row now says what its store is and
since when.

| Claim | Command | Result | Commit |
| --- | --- | --- | --- |
| The one vault store is the grant | `ctest --preset dev -R '^application_device_authorization_registered_course$'` | `DEVICE_FLOW_VAULT_STORED_TOKEN:f5505a4b89c07e21`, `DEVICE_FLOW_STORED_IS_GRANT:true`, `VAULT_STORES:1`, `DURABLE_COPIES:0`, `DEVICE_FLOW_ENROLLED_OK` | this commit |
| The count alone no longer passes | the negative control above | `STORED_IS_GRANT:false`, `DEVICE_FLOW_ENROLLED_FAILED` on the old evidence | 4785bafd |
| The refresh course and every other device row are unchanged | `ctest --preset dev -R '^(application_device_authorization_(host\|refresh\|refresh_course\|registered_course\|unavailable\|release_contract\|public_path\|ghes_refusal\|browser_handoff\|browser_absent\|panel_course\|panel_absent\|browser_address)\|qt_device_authorization_scale_(100\|125\|200)\|native_https_device_authorization_(contract\|fixture)\|application_github_host\|application_github_public_path_.*\|application_public_path\|composed_application_hosts\|rfc0016_test_inventory\|test_network_isolation_manifest\|application_review_round_trip_composition_contract\|application_composition_contract)$'` on the merged tree | 28/28 device, GitHub host, public-path, audit, inventory, manifest, and composition rows | this commit |
| Boundaries, tidy, and format hold | `check_boundaries`; `tidy.py --paths` over the three touched sources; `clang-format --dry-run --Werror` | exit 0; passed; `FORMAT_OK` | this commit |
| The pins were recomputed in two passes | the round-trip contract | held at the slice's recomputed values (full `6b854b2816f7…`, manifest `a5a183bebea7…`); no drift on the merged tree | this commit |
| The ledger row and the documents agree with the tree | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` | this commit |
| No whitespace damage | `git diff --check` | clean | this commit |
