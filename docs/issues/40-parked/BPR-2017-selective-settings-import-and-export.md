---
schema: "implement-rfc.card.v1"
id: "BPR-2017"
title: "Selective Settings import and export"
status: "Parked"
rfcs: ["RFC-BPR-0107"]
metadata: {"criteria":["AC01","AC06"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0002","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0029-selective-settings-import-and-export.md"}
depends_on: ["BPR-2016"]
parked_on: "human-review"
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Two profiles through the real stores; composed sender and recipient app courses through Settings' buttons and dialogs (ledger)","command":"ctest --preset dev -R sqlite_agent_configuration_storage"}},{"criterion":"AC06","outcome":"pending","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"observation","detail":"Keyboard, accessible names, restart, built-in restoration and skill references pass on macOS (qt_agent_configuration_dialogs); owner look session 629d64c72746157219d2473892973d7f pending; Windows and Linux not run"}}]
---

# Selective Settings import and export

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0107](../../rfcs/RFC-BPR-0107-agent-and-prompt-sharing.md), criteria `AC01`, `AC06`.

Native Agents & context import/export, conflict comparison and binding surfaces; existing prompt and agent setup integration.

## Intended verification

Use the actual Settings entrypoints with a fresh recipient profile, keyboard navigation and missing bindings; prepare the focused frozen UX candidate under existing conventions.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Record (2026-11-11)

Product candidate `7ae9b389c`; ledger [RFC-BPR-0107](../../verification/RFC-BPR-0107.md).
Frontend-design was loaded before the Qt work. Settings › Agents & context
has the share row where the [design record](../../design/BPR-2051-agents-roster-and-footer/README.md)
placed it; Export configuration… and Import configuration… are native dialogs
(`qt/agent_configuration_dialogs`). AC01 passes on the two-profile storage
course and on the composed app courses through the real buttons and dialogs
(`--attended-share-look` with `BPR_ATTENDED_SHARE_CHECK=1`, then `recipient`
in a second fresh profile). AC06: keyboard and accessible names
(`qt_agent_configuration_dialogs`), restart read-back, built-in restoration
and skill references pass on macOS; the owner's look and Windows and Linux are
pending.

Filed, not waited on: session `629d64c72746157219d2473892973d7f`, state
`/Users/jaeger/.attended-review-state/share-agent-prompts-v001-2026-11-11/state`
(URL in `../serve.log`), cursor 9, host PID 76865 (Tailscale child 76875),
candidates `BPR-2017-export-selection`, `BPR-2017-import-preview-bind`,
`RFC-BPR-0107-default-prompt-round-trip`. Frozen root
`/Users/jaeger/share-agent-prompts-v001.h9i51jwq` from `9be81a9c2141` (app
SHA-256 `1088de08cc6a…`, launcher check reached `ATTENDED_SHARE_SHOWN:agents=2`).
