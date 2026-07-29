# Pack Revision Notes

## Version 2

This revision makes the supplied **Deterministic Functional Core, Imperative Host, and Qt Presentation Adapter** architecture explicitly normative for the complete BetterPullReview implementation.

The governing rule is:

> **The core decides. The host performs. Qt presents.**

Version 2 does not change the approved product scope, GitHub authority model, RFC order, or feature set. It strengthens implementation direction by:

- including the complete architecture source in the pack;
- specializing it for BetterPullReview in the system and state architecture documents;
- assigning every RFC's decisions, effects, and presentation to the functional core, application host, and Qt presentation adapter;
- requiring typed intents, events, bounded observations, requests, semantic deltas, and diagnostics;
- requiring a serialized application decision lane, request identity/generation, cancellation, and deterministic stale-result rejection;
- requiring incremental Qt projections and stable-identity model updates rather than whole-workbench refreshes;
- separating Qt-local state from semantic interaction state and authoritative GitHub/local-ledger state;
- expanding issue plans, verification matrices, release gates, and the pack validator to enforce the architecture.

The RFC IDs remain `RFC-BPR-0001` through `RFC-BPR-0014`. Import this revision as the current pack source rather than layering it as an additional architecture RFC.
