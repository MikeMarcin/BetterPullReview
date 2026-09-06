---
schema: "implement-rfc.card.v1"
id: "BPR-0637"
title: "Migrate authentication and capability JSON to Glaze"
status: "Backlog"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0636"]
---

# Migrate authentication and capability JSON to Glaze

## Bounded acceptance and ownership

Own github_device_authorization_contract.cpp and github_capability_probe.cpp
under src/adapters/github/src, with their focused tests. Replace Qt JSON with
typed Glaze contracts; preserve secret handling, sanitized failures, exact
request bytes, nine-query capability probing, optional/required fields and
resource bounds. Verify device/capability/provider and synthetic public sign-in
composition paths without touching production credentials or sign-in.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.
