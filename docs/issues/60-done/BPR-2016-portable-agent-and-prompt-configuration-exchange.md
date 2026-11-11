---
schema: "implement-rfc.card.v1"
id: "BPR-2016"
title: "Portable agent and prompt configuration exchange"
status: "Done"
rfcs: ["RFC-BPR-0107"]
metadata: {"criteria":["AC02","AC03","AC04","AC05","AC07"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0001","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0028-portable-agent-and-prompt-configuration-exchange.md"}
depends_on: []
acceptance: [{"criterion":"AC02","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Only selected portable material in the bytes; exact text shown","command":"ctest --preset dev -R 'core_agent_configuration|sqlite_agent_configuration_storage'"}},{"criterion":"AC03","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Whole-file refusals and per-item problems named; unbound program refused; final 16-agent bound","command":"ctest --preset dev -R 'core_agent_configuration|sqlite_agent_configuration_storage'"}},{"criterion":"AC04","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Kept by default; replacement differences and identity rule; relaxation and builds per entry; changed_since and crash journal","command":"ctest --preset dev -R 'core_agent_configuration|sqlite_agent_configuration_storage'"}},{"criterion":"AC05","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Only file, stores and identity mints; nothing launched","command":"ctest --preset dev -R 'core_agent_configuration|sqlite_agent_configuration_storage'"}},{"criterion":"AC07","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Both RFC-BPR-0104 settings carried; builds off unless accepted; scratch Off applied; identity unchanged","command":"ctest --preset dev -R 'core_agent_configuration|sqlite_agent_configuration_storage'"}}]
---

# Portable agent and prompt configuration exchange

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0107](../../rfcs/RFC-BPR-0107-agent-and-prompt-sharing.md), criteria `AC02`, `AC03`, `AC04`, `AC05`, `AC07`.

Core exchange admission and import mapping; application file/persistence coordination; narrow settings-store integration. Preserve current agent and prompt validators and the existing rule for when an edited entry keeps its identity.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Round-trip selected portable definitions and refuse malformed, incompatible or conflicting input without effects or partially applied settings. Replace an entry with and without a change to what it runs, and show which keeps its retained conversations; import a definition requesting relaxed checks without accepting the relaxation.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Record (2026-11-11)

Product candidate `7ae9b389c`; ledger [RFC-BPR-0107](../../verification/RFC-BPR-0107.md).
Core format, admission and import decisions (`core/agent_configuration`),
host and all-or-nothing store (`application::agent_configuration_host`,
`adapters::sqlite::make_agent_configuration_store`), and the written-against
record for every hook override. AC02, AC03, AC04, AC05 and AC07 pass on
`core_agent_configuration` and `sqlite_agent_configuration_storage` (two
profiles, real stores, a concurrent `bpr`-style edit, a simulated crash
between the two settings files). The owner's 2026-11-11 decision (export the
shipped defaults; all purposes; provenance recorded) is in the RFC.
