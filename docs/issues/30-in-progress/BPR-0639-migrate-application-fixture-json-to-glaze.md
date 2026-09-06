---
schema: "implement-rfc.card.v1"
id: "BPR-0639"
title: "Migrate application fixture JSON to Glaze"
status: "In Progress"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0638"]
---

# Migrate application fixture JSON to Glaze

## Bounded acceptance and ownership

Own src/app/src/github_application.cpp and github_public_path_fixture.cpp plus
focused composition fixtures/manifests if affected. Replace remaining Qt JSON
with Glaze. Preserve exact fixture documents, fault injection, accepted source
identities and production/test boundaries. Verify normal composition and
selected synthetic public replay without live network or production sessions.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.
