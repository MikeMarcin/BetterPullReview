---
schema: "implement-rfc.card.v1"
id: "BPR-1238"
title: "The defect composition contract fails to seed its epoch fixture"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"red at baseline in the Review Cycles snapshots integration pass (fails identically at 8374ea995); filed by the authoring milestone implementer on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1238--BPR-1238--1bcc75d6c30d.json"}
---

# BPR-1238: The defect composition contract fails to seed its epoch fixture

## Observation

At `main` 95a30c5b0 on macOS arm64 (dev preset, Homebrew LLVM),
`ctest --test-dir build/dev -R "^application_defect_composition_contract$" --output-on-failure`
fails. The probe reaches a ready pull request and an exact epoch pair, then
reports `GITHUB_FIXTURE_FAILED:github.com` with
`PUBLIC_PATH_VERDICT:presentation=0` and
`GITHUB_FIXTURE_EVIDENCE:01111110:9:10:9`, followed by
`DEFECT_COMPOSITION_SEED_FAILED` and
`DEFECT_COMPOSITION_EVIDENCE:11111100011`
(`tests/end_to_end/application_defect_composition_contract.cmake:95`, called
from line 134). The snapshots milestone recorded the same failure at the
pre-milestone base 8374ea995, so no Review Cycles change introduced it.

## Outcome

The contract passes at the resulting `main` head, with the cause of the
presentation and seed failure fixed where it lives rather than the expected
evidence relaxed.

## Direct acceptance

- `ctest --test-dir build/dev -R "^application_defect_composition_contract$" --output-on-failure` passes on macOS at the candidate revision.
- The card records which step of the seed failed and why.
- `python3 scripts/verify_documentation.py --root .` passes.
