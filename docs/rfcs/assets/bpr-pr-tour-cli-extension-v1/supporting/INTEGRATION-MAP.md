# Integration and predecessor reuse

## Parent contracts reused, not reauthored

| Parent RFC (before live remapping) | Extension dependency |
|---|---|
| 0039 | Same problem/solution/stop model, local draft identities, manual authoring, readiness and persistence. |
| 0040 | Same normal-comment format, index parsing, destinations, root selection, fallback placement and machine-independent shared meaning. |
| 0041 | Same finite staged plan, real returned URLs/IDs, exact write admission, private pending visibility, journal/reconciliation and explicit submit/resume. |
| 0042 | Same website conflict logic, multi-batch route, published reorder/insert/retarget semantics and partial maintenance. |
| 0043 | Same four hooks, parent result contract, exact in-place range replacement and manual-before-AI/per-stop constraints. |

The extension's shared journal is a narrow additional requirement on committed local history: ordinary native UI behavior remains, but changing presentation must not create two canonical undo stacks. The new non-GUI approval/visit presentations do not weaken remote authority; the agent's relayed user response is not cryptographic proof. Existing non-tour comment-workshop behavior is unaffected.

## Implementation allocation

Core: typed local draft/text/structure commands, exact revision/scope validation, state transitions and interpretation policy. Application: canonical tour facade, store/journal, snapshot import, effect planning, owner forwarding admission, assistance tickets, bounded context and publication coordination. Adapters: existing provider/Git/SQLite plus new CLI parsing/rendering/native editor/IPC transport. Composition: minimal headless QCoreApplication runtime and existing desktop binding. Skill: conversation and calls only.

Do not create a general command/plugin server. Extend the current owner/native channel facilities for finite tour commands and keep the activation protocol compatible. The CLI may keep Core/Network, native vault libraries and existing admitted helpers; it must not ship GUI/QPA/Widgets resources. The core-only target remains genuinely independent of those services.

## Repository edits during implementation

Add new RFCs and extension cards, a narrow CLI/runtime target, service/API tests, and precise amendments to build/architecture/privacy/profile/AI/user documentation. Preserve the canonical README/index layout and current names under the source's coding standards. The source audit lists observed paths; the new tour implementation may have different current symbols because the baseline predates the imported pack.

Use the existing shared AgentSkills repo as canonical skill source. This pack's candidate text is transferred there through the installed MASK workflow, not pasted into multiple application checkouts. BPR runtime/command contract is authoritative for syntax; shared skill references must be tested against it. No exact live CLI version number or MASK command is assumed before implementation.

## Archive and live-link placement

Archive relative links work within this pack. During import, retain support material under `docs/rfcs/assets/bpr-pr-tour-cli-extension-v1/` (or the repository's equivalent mapped asset root). Copy only the three new RFCs into canonical `docs/rfcs/`, and rewrite each new RFC's `../../../supporting/`, `../../../specs/`, `../../../references/`, `../../../verification/`, and `../../../skill-source/` links to that installed asset location. Preserve the immutable archived originals separately. Do not blindly apply relative archive links to the live repository or replace the parent asset directory.
