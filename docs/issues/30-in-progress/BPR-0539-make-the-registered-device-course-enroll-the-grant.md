---
id: BPR-0539
title: Make the registered device course enroll the grant, not the connect credential
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0536]
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

Claimed straight into `30-in-progress/` by the integrator under the
transition exception; [[BPR-0536]]'s remaining work at claim time is its
documents and card. The slice runs on `bpr-0539-work` in a worktree and
follows the refresh course's own pattern — no connect before the grant,
delete by authority — extended to the registered course, with a marker
that proves the stored material is the grant and a negative control that
fails the row when the grant's enrollment is skipped.
