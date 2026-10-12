---
schema: "implement-rfc.card.v1"
id: "BPR-1386"
title: "Prepare the next Ubuntu 26.04 release route"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
platforms: ["win", "linux"]
milestone: "Next-release Ubuntu preparation"
metadata: {"milestone_plan":{"id":"release-next-ubuntu2604","path":"docs/rfcs/plans/release-next-ubuntu2604/milestone-plan.json"}}
---

# Prepare the next Ubuntu 26.04 release route

## Mandate and boundary

The owner selected Ubuntu 26.04 x86_64 for the next release on 2026-10-11,
replacing Ubuntu 24.04 and removing RHEL from the selected release scope:
"I dont care about redhat and i dont care about 24.04, you can update all the
scripts/configs you need to." Linux is an optional convenience distribution.
Available WSL2 may produce ordinary Linux binaries; WSL/WSLg checks do not
establish clean Ubuntu desktop, installation or Secret Service qualification.
This is mandate revision `release-next-20261011-ubuntu2604-r1`, separate from
observations and historical acceptance. The inherited M8/M9 plan and evidence
remain unchanged; this issue has a separate bounded preparation plan.

Prepare existing DEB and per-user archive mechanics, retaining source assets,
SBOM, final digests and approved signing/trust checks. Existing optional RPM
mechanics may remain, but RHEL is not a required/current release target.
No version selection, full release build, package installation, signing/key
enrollment, tag, push or publication is authorized by this slice. Do not close
RFC-BPR-0038, BPR-0961 or BPR-0962.

## Acceptance

- AC1: Admit Ubuntu 26.04 x86_64 through the public packaging baseline/target path; use distinct `linux-ubuntu-26.04-x64` binary and source names and enforce its glibc 2.43 ceiling. Refuse obsolete Ubuntu 24.04, wrong architectures and over-ceiling ELF requirements.
- AC2: Exercise native manifest and installed source binding contracts for the new target, including wrong target/generator, stale producer, digest/source substitution and signing-receipt rejection without weakening other platforms' security checks.
- AC3: Update current release/operator/skill documentation and record the owner's Linux scope amendment separately from actual evidence. Preserve historical scans and explicit clean-desktop/vault/crash/trust/publication gaps; run focused contracts and documentation gates, reporting Windows skips and any isolated WSL evidence truthfully.

## Execution

Owner: `/root/ubuntu2604_release_prep`, sole engineering leaf on `main`.
Entry revision: `960b03bc3`; source baseline before allocation:
`b67d720dd72323e126d9b1db0073fde2bfb257a4`. The reserved ID receipt is
`docs/issues/_reservations/BPR-1386--BPR-1386--cfa5ce12359a.json`.
The coordinator owns ignored wizard state and Azure/read-only prerequisites.
Durable continuation: `.mask/sessions/release-next-20261011/ubuntu2604-handoff.json`.
