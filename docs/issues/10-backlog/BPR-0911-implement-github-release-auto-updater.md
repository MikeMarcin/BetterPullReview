---
schema: "implement-rfc.card.v1"
id: "BPR-0911"
title: "Implement GitHub release auto-updater"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0946", "BPR-0947", "BPR-0948", "BPR-0949"]
milestone: "Post-Pack v2"
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Implement GitHub release auto-updater

## Scope and entry

The 2026-09-20 mandate requests implementation of
[RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md).
The earlier 2026-09-18 authoring-only mandate remains historical provenance.
Current implementation permits local source/board edits and disposable checks;
live private-service access, release publication and replacement of the developer's
running application are not authorized.

The authored milestones are [[BPR-0946]], [[BPR-0947]], [[BPR-0948]] and
[[BPR-0949]] in order. Preserve
its native authentication boundary, fixed private release source, typed core/host/
Qt allocation, application-wide ownership and signed macOS package constraints.
Canonical dependency and closeout order remains unchanged.

## Acceptance

- Implement AC-01 through AC-08 using the RFC's milestone order; track any split
  card and its exact criterion ownership on the closeout.
- Demonstrate the real menu/Settings-to-host path and native disposable signed
  upgrade, failures, recovery, shutdown and retained review state.
- Record actual results and unresolved target-native evidence on [[BPR-0912]].
  Fixtures alone cannot establish private-service or native package success.

## Current evidence

Milestone 1 planning is in progress. AllTheThings remains read-only reference
provenance. Windows/Linux self-installation is outside this card.
