---
id: BPR-0460
title: Five new Windows-uncontained registrations were never pinned
status: Done
rfc: RFC-BPR-0016
depends_on: []
---

# Five new Windows-uncontained registrations were never pinned

`test_network_isolation_manifest` fails on the first fresh Windows configure
after the device-authorization program landed:

```
TEST_NETWORK_ISOLATION_FAILED:application_device_authorization_public_path uses bpr_windows_uncontained but is not in the pinned Windows set
```

and the same three-line family for `application_device_authorization_unavailable`,
`application_device_authorization_registered_course`,
`application_device_authorization_release_contract`, and
`application_agent_invocation_launcher_construction_cut_contract`.

The registrations are deliberate: the four RFC-BPR-0016 selectors drive the
shipped `app_better_pull_review` self-test
(`tests/end_to_end/CMakeLists.txt`), and [[BPR-0420]] added the
`launcher-construction-cut` course to the agent-invocation foreach whose every
course is `WINDOWS_UNCONTAINED`. What no slice did is add the names to
`WINDOWS_UNCONTAINED_TESTS` in `scripts/verify_test_network_isolation.py` —
the exact drift the gate exists to catch, in the direction it warns about:
"the exemption can neither grow quietly nor outlive its reason." All five
slices were implemented from macOS sessions, where the pinned-set branch never
runs, so every matrix those cards ran stayed green. [[BPR-0452]] recorded the
same class of drift for the loopback table two days earlier.

The cause is inherited, not newly measured: every member is the shipped
application (or its composition public path) launched the same way as the
already-pinned `application_*` family, whose refusal [[BPR-0362]] measured as
`dos-volume-name` — the lowbox token cannot translate a volume device into a
drive letter. No new mechanism is involved.

## Required final state

- The five names are pinned in `WINDOWS_UNCONTAINED_TESTS` with the
  `dos-volume-name` cause.
- `test_network_isolation_manifest` passes on the Windows host at the
  repaired tree.

## Non-goals

- Not whether the five tests themselves pass on Windows; their rows belong to
  the matrix and the cards that own those surfaces.
- Not [[BPR-0446]]'s fixture registration, which the same manifest run
  already validates.

## Closed — 2026-08-26

The five names are pinned at `f8799ca9` with the inherited
`dos-volume-name` cause, and `test_network_isolation_manifest` passed on
the Windows host at that commit in 1.89 seconds — the same run that
validated BPR-0446's fixture registration. Closing promoted nothing: no
card names this one in `depends_on`.

Implemented and closed by the integrator.
