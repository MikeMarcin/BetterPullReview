---
schema: "implement-rfc.card.v1"
id: "BPR-1059"
title: "Compose terminal publication maintenance and read-first recovery"
status: "Done"
rfcs: ["RFC-BPR-0044"]
depends_on: ["BPR-1055", "BPR-1056", "BPR-1058"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-08","milestone":"publish-and-recover","acceptance":["AC44-08","AC44-09","AC44-10"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC44-08","outcome":"pass","revision":"5341b12e8a2f0f250d7251501473e81197664430","evidence":{"kind":"observation","detail":"Shared finite plan, parent host/journal and native/admitted-gh composition. CLI isolated counters plus parent desktop native/maintenance/stop/unsupported and staged restart tests pass; production qualification remains refused.","reference":"docs/verification/RFC-BPR-0044-0046.md"}},{"criterion":"AC44-09","outcome":"pass","revision":"5341b12e8a2f0f250d7251501473e81197664430","evidence":{"kind":"observation","detail":"Actual POSIX PTY confirmation exact publish text, lease release and concurrent edit refusal; JSON and --yes do not authorize. Human/agent live qualification remains later scope.","reference":"docs/verification/RFC-BPR-0044-0046.md"}},{"criterion":"AC44-10","outcome":"pass","revision":"5341b12e8a2f0f250d7251501473e81197664430","evidence":{"kind":"observation","detail":"Actual SIGKILL at all four publication write boundaries before/after witness and applied effect, plus acknowledgement; recover twice leaves server bytes unchanged; explicit safe resume no duplicates, ambiguous sends refuse.","reference":"docs/verification/RFC-BPR-0044-0046.md"}}]
---

# BPR-1059: Compose terminal publication maintenance and read-first recovery

Pack alias: **PACK-PRTOUR-08** (live `BPR-1059`, allocated at import).

## Outcome

Expose existing finite publication and maintenance plans through preview/publish/recover/resume. Bind exact actor/head/target, surface staged versus published visibility, retain real child IDs/receipts, and use current native/admitted gh transports. Implement human-terminal confirmation; unsupported agent approval modes return interaction_required without writes.

## Existing ownership seams

Parent RFC0041/0042 session/journal/admission; current provider/native/gh CLI adapters; headless host.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Fixture remote counters show zero writes from all local/read commands. Kill before/after send and acknowledgement, recover twice, and verify no duplicate publication. Wrong account, unknown gh platform, missing vault and unscoped bypass refuse honestly.

Acceptance ownership: AC44-08, AC44-09, AC44-10

## Execution boundary

Milestone: **publish-and-recover**. Pack alias PACK-PRTOUR-08 was allocated as live `BPR-1059` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
