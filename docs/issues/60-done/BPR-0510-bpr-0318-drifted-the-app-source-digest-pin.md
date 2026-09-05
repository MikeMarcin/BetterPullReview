---
schema: "implement-rfc.card.v1"
id: "BPR-0510"
title: "BPR-0318's selection flip drifted the app-source digest pin"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# BPR-0318's selection flip drifted the app-source digest pin

## Why

Found by `bpr-linux` on 2026-09-02 while working [[BPR-0472]] at `6fff0a53`,
and reproduced on the integrator's macOS host in 0.06 s:

```text
application_review_round_trip_composition_contract ***Failed
  full LF-normalized github_application.cpp digest drifted:
    expected 068b4334ce3eac2fe536e68f517f35b5009e49d5372ada0e43d4d0dc1c957bb6
    found    1be908db8c0aedfc00584c8660e81dfeb963e3c3f4f92ed42c23c194bdbfda84
```

`application_composition_contract` fails on the same pin after its full
course. The pin at `tests/end_to_end/application_review_round_trip_composition_contract.cmake:42`
was last repinned by [[BPR-0479]] at `fc46b5d5` on 2026-08-29; [[BPR-0318]]'s
macOS selection flip (`8c39ca40`, later the same day) changed
`src/app/src/github_application.cpp` and did not repin. The same class of
drift is recorded on [[BPR-0477]]: the pin is a test-infrastructure guard
that legitimate `src/app` edits move, and the repair is to repin to the
committed tree's value once the edit is known to be legitimate — which the
BPR-0318 flip is, being the RFC-BPR-0016 enablement the owner accepted.

## What this slice delivered — 2026-09-02

Two pins in the same file had drifted, and the second was visible only once
the first was repinned: `bpr_expected_full_application_digest` (the
LF-normalized `github_application.cpp`, `068b4334…` → `1be908db…`) and
`bpr_expected_production_app_manifest_digest` (the production app-source
manifest, `6da4ef7b…` → `ae1a89c1…`). Both are repinned to the values the
committed tree at `6fff0a53` produces; `bpr_expected_mutation_production_digest`
did not move. The station saw only the first because the contract stops at
the first drifted pin.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The round-trip contract is green again | `ctest --test-dir build/dev -R '^application_review_round_trip_composition_contract$' --output-on-failure` | Passed 30.02 s (after the first repin alone it failed at the second pin in 0.07 s) | this commit |
| The full composition course is green again | `ctest --test-dir build/dev -R '^application_composition_contract$' --output-on-failure` | see the record below | this commit |
