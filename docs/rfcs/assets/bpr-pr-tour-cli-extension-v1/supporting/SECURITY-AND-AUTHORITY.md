# Narrow security and authority changes

## Inherited rules

The extension reuses RFC-BPR-0041's immutable parent publication plan, one-use child grants, transmission witnesses, reconciliation, and explicit resume. Read permission, local edit permission, same-user IPC, a recovered draft, and author readiness are not remote publication permission. No new direct mutation or model API path exists in the skill. GitHub-only reviewers need no installation, marker or service.

## Newly introduced surfaces

| Surface | Required containment and limitation |
|---|---|
| CLI arguments and JSON stdin | Typed finite commands; lossless IDs; exact target/revision; bounded bytes/depth/arrays; reject unknown mutation fields; no generic provider request path. |
| Terminal output | Escape untrusted terminal control sequences/OSC/CSI, including clipboard/title/hyperlink injection. Render a safe readable representation; keep canonical source unchanged. Never hide critical risks in color alone. |
| External editor/pager | Explicit trusted user configuration, executable plus argument vector, no shell evaluation, no repository auto-configuration. Private temp projection with safe reopen and bounded read. Chosen editor plugins/backups remain outside BPR's assurance. |
| Same-user IPC | Native peer authentication plus private root/endpoint, exact owner/profile/protocol, request dedupe, command allowlist. Not a boundary against arbitrary malicious same-user code. No HTTP/TCP service. |
| Shared local edit journal | Profile-private storage, bounded reversible transactions, monotonic revisions, preserved conflicts, no source/prompt logs. No stale cache last-writer-wins. |
| Active external agent | Explicit context export, inherited host/profile policy, no claimed attestation from a provider string; BPR does not audit data already in agent context. |
| Skill distribution | One canonical pr-tour source; versioned CLI feature check; no unverified binary auto-download, token extraction or permissions installation. |

## Headless authorization

Human `publish` shows the exact plan and obtains an explicit terminal response. A supported native agent-host approval bridge can forward an actual approving interaction while binding the same plan/actor/head, but an ordinary model-generated payload cannot mint the grant. Without that bridge, the skill supplies the normal terminal publish command; it does not synthesize key presses or pipe an affirmative answer. Capability output tells callers which mode exists. No mandatory copying of plan hashes enters the everyday human flow.

Approval of a finite tour plan is not approval of arbitrary future comments. Narrative, target, scope or relevant revision changes invalidate authorization. Deterministic actual ID/URL substitutions remain the parent's allowed late binding. A machine-local retry file is a recovery hint, not persistent authority. Existing native/CLI platform restrictions remain in force until separately qualified.

## Local assistance tickets

A requested scoped edit ticket permits exactly one local replacement or one outline result in an admitted editing session. It is distinct from a remote grant. Captured boundaries, bytes, source, hook and revisions belong to the host. The model result cannot set target or offsets, ready flags, or instructions to execute. Parent result validation remains authoritative. Tickets are bounded, cancellable, expiring, superseded on a new request and invalidated by relevant edits. Returning to identical bytes does not revive them.

An external agent can already ask the CLI to make author-requested local edits; do not overstate that a ticket secures a user's files against all code running as that user. Its useful guarantee is application scope, identity, race handling and provenance, not cryptographic proof of human intent.

## No new ambient effects

Opening/importing/walking, mark-ready, profile restoration, hook reads, autosave, edits, undo/redo, owner discovery and read-only recovery cannot create remote comments or launch AI. Only explicit editor commands launch the configured editor. Only explicit assistance exports relevant context; no automatic Jira or arbitrary-link fetching. Native authentication requirements remain explicit; do not open a browser unexpectedly in machine mode.

Use network-isolated fixtures by default. The RFC pack and its tests do not authorize real company PR mutation, real credentials, remote pushes, paid models, permission changes or shared skill deployment. Designated live acceptance requires the existing approved workflow. A confirmed local transaction and a confirmed remote receipt must never share an ambiguous success label.
