---
schema: "implement-rfc.card.v1"
id: "BPR-1403"
title: "Repair and verify the v0.1.9 release gates"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"release-v019-20261012-rfc-implementer-r2","reservation":"docs/issues/_reservations/BPR-1403--BPR-1403--d8f83f6e0ba0.json","milestone_plan":{"id":"release-v019-gate-repairs","path":"docs/rfcs/plans/release-v019-gate-repairs/milestone-plan.json"}}
---

# Repair and verify the v0.1.9 release gates

The owner requests a new prerelease patch build, full packages for macOS,
Windows x64 and Ubuntu 26.04, and amends execution: "if there's actual fixed
to be done use an rfc implementer dont do them yourself". The coordinator
assigns this bounded repair milestone to `/root/release_gate_repairs` on main.
The release source starts at `0a35a94f263599d84da871aa7129a625cafd5b57`;
entry candidate is `c9a4164756bcd8ce3601115c0912845cb3ef5bcd`.
Existing repair commits are adopted and checked, preserving failed-run evidence.

## Acceptance

- AC1: Collect the running macOS normal gate and disposition every failed row against current source; fix actual causes without weakened assertions, disabled rows, broad ignores or bypasses.
- AC2: Rebuild repaired targets and pass failed plus materially affected public-path checks, including provider/replay, large review, rate/scaling, offline account isolation and attended replay. Preserve unchanged passing evidence and explicit platform skips.
- AC3: Account for normal Windows and Ubuntu 26.04 native gates on the final product/test source before packaging handoff. Required native evidence remains pending until supplied by the coordinator's producers.
- AC4: Pass relevant boundary, format and documentation checks, commit explicit repair paths and upkeep, and leave a durable checkpoint with candidate identities, commands, verification and pending scope.

## Ownership and limits

Engineering and board/plan/evidence ownership belongs to the nondelegating
implementer; coordinator owns producer dispatch, packaging and source syncing.
Preserve unrelated .gitignore, .mask.json, .mask.lock.json and RFC draft work.
No signing, notarization, tag, push, publication, credential change, owner
desktop/browser inspection or live GitHub mutation is authorized. RFC0038 and
BPR0958–0962/BPR0973 remain open with their wider native, vault, crash/privacy,
trust, licensing and vulnerability obligations. This card accepts only repairs
and their release gate evidence.

Durable continuation: `.mask/sessions/release-v0.1.9/repairs-handoff.json`.
