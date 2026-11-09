# Implement this pack

Implement only subsequently authorized scope from
[RFC-DRAFT-0030](RFC-DRAFT-0030-native-agent-sessions-and-tour-context.md)
using the [authored milestone plan](milestone-plan.json). Its
[milestone view](MILESTONES.md) is generated; do not edit it independently.
The pack is a draft, not a current implementation mandate.

Read consumer instructions, the [decision record](DECISIONS.md) and
[proposed amendment](proposed-session-amendment.md). Reconcile live source
drift and concurrent writers, import the provisional RFC/card IDs, and adopt
the scoped amendment and authored grouping through implement-rfc before
product work. Keep all settled answers: actual backend continuity in both
surfaces, per-agent separation, New session with retained history, explicit
replay fallback, and authoritative tour-state context.

Start with `native-agent-tab`; its native-session host and durable workspace
ownership support `tour-sessions`, followed by `tour-context`,
`qualified-continuation` and `accepted-scope`. The proposed cards live in
[cards/](cards/). Resolve any material choice the review subsequently opens
before implementing its affected scope; follow the current decision record.

Use installed Claude Code and Codex through their own sign-in, preserving
host/provider restrictions. Persistent Claude stream-json and Codex App
Server JSON-RPC connections survive turn completion; reconnect by the exact
native handle when required. Small tool-free hooks remain one-shot. No
model API, mandatory JS/Python bridge, or unrelated permission expansion is
part of this pack. Load frontend-design before UI changes and use offscreen
verification; never drive the owner's desktop for implementation.

Keep durable conversation identity separate from activation and turn fences.
Protect retained workspaces during startup recovery; make storage refusal
and uncertain sends visible. Replay is a distinct generation under the same
permission rules. The tour snapshot covers all relevant state, with source
confirmation distinct from Ready and publication eligibility. See the review
record for these refinements and the preserved source-warning uncertainty.

Import, draft validation and intermediate card completion are progress, not
acceptance. Preserve actual failures and installed/native/human gaps, keep
Debug timing nongating, and judge the RFC against real public-path evidence.
Report unresolved prerequisites without claiming authoring checks as passes
of the product. No blanket authoring confirmation is an additional gate.
