---
schema: "implement-rfc.card.v1"
id: "BPR-0911"
title: "Implement GitHub release auto-updater"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: []
milestone: "Post-Pack v2"
platforms: ["mac"]
---

# Implement GitHub release auto-updater

## Scope and entry

Implement [RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md) only after a later
implementation request. The 2026-09-18 mandate authorized RFC authoring in direct
mode without sub-agents. No runtime changes or install/publication were requested.

Split the RFC's four milestones into bounded cards before source work. Preserve
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

Not started. AllTheThings was inspected read-only for the RFC; no implementation
or test result is claimed. Windows/Linux self-installation is outside this card.
