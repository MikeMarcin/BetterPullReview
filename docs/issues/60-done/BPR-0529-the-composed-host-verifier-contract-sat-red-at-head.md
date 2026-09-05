---
schema: "implement-rfc.card.v1"
id: "BPR-0529"
title: "The composed-host verifier's own contract sat red at HEAD for a day"
status: "Done"
rfcs: []
depends_on: []
---

# The composed-host verifier's own contract sat red at HEAD for a day

## Why

`composed_host_verifier_contract` (`tests/scripts/verify_composed_hosts_tests.py`)
drives `verify_composed_hosts.py` over a synthetic symbol listing whose
`SHARED_LINES` fixture must name every `SHARED_REQUIRED` anchor. [[BPR-0504]]
added five anchors, [[BPR-0514]] one, [[BPR-0516]] one, and none of their
reproductions ran this suite — its selector is not in any RFC's focused set
— so six of its cases failed on "required exact demangled anchor is absent"
against the synthetic binary while the real `composed_application_hosts`
stayed green. [[BPR-0526]]'s worker reproduced it red at the pre-slice head
before adding an eighth anchor.

## Required final state

- The fixture names every shared anchor and the suite passes.
- A guard case refuses a fixture that trails `SHARED_REQUIRED`, so the next
  anchor addition fails on this suite by the anchor's name in the same
  change rather than a day later.

## Repair — 2026-09-02

Eight anchor lines added to `SHARED_LINES`;
`test_the_fixture_names_every_shared_anchor` added.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The suite passes and the guard holds | `python3 tests/scripts/verify_composed_hosts_tests.py` | 52 tests OK | this commit |
| The real audit still passes on both profiles | `composed_application_hosts` in the BPR-0526 reproduction | Passed | this commit |
