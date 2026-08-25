---
id: BPR-0450
title: Implement the device refresh wire contract and production adapter
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0279]
---

# Implement the device refresh wire contract and production adapter

Done [[BPR-0279]] shipped refresh orchestration against a
`device_authorization_refresh_port` driven entirely by test doubles, because
compiling and parsing the refresh-token exchange is a wire-contract change
that was an explicit non-goal of that slice. The gap was surfaced by the
implementing lane rather than absorbed silently, and this card owns closing
it.

## Required final state

- `github_device_authorization_contract` gains the refresh-token exchange:
  an exact bounded form request (`client_id`, `grant_type=refresh_token`,
  `refresh_token`) against the token endpoint, and typed parsing of the
  refresh success and failure bodies under the same recorded-truth
  discipline as the existing courses. The refresh token is a validated
  runtime input that never appears in any diagnostic.
- A production adapter implements [[BPR-0279]]'s refresh port over
  `perform_device_authorization`, and the host's refresh lifecycle runs
  against it in a focused contract test registered as
  `github_device_authorization_refresh_contract` — the selector the
  RFC-BPR-0016 final command ledger already allocates.
- The recorded-fixture question is answered honestly: no attended refresh
  capture exists, so the wire shapes bind to GitHub's documented refresh
  contract and are labelled documented-not-recorded, or a future bounded
  attended capture is proposed as its own card. No fabricated
  recorded-GitHub claim is admissible.
- Hostile parser and native-transport coverage extend to the refresh
  exchange to the same standard as the existing device courses.

## Verification

```sh
ctest --preset dev --output-on-failure -R 'github_device_authorization_refresh_contract|application_device_authorization_refresh'
python3 scripts/verify_documentation.py --root .
```
