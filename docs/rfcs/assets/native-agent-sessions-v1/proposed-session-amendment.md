# Proposed amendment: native conversations and tour-state context

This accompanies [RFC-DRAFT-0030](RFC-DRAFT-0030-native-agent-sessions-and-tour-context.md).
It proposes changes to adopted requirements; it does not edit or adopt them.
Source identities below were inspected during authoring. They are not an
implementation baseline or acceptance evidence. Reconcile drift before import.

## Sources inspected

| Source | SHA-256 |
|---|---|
| [RFC-BPR-0092](../../RFC-BPR-0092-agent-first-run-setup.md) | `dff26be67f0388a85c43e1a1b163b3487a993e12fa121fd29fcb73d540fead51` |
| [RFC-BPR-0011](../../RFC-BPR-0011-ai-agent-profiles-context-supplements.md) | `9c1a6123ff2f11e5dc515efa9c61a14b30242bf4da56ab69aa4598ebf2d9d6f4` |
| [RFC-BPR-0012](../../RFC-BPR-0012-ai-review-chat-findings-mcp.md) | `5ca9759df02a7c9ab6e2b06b7fe5d45062fa810153ebbed9d3dea31a0993a1e2` |
| [AI agent boundary](../../../architecture/ai-agent-boundary.md) | `4fb04097b1b8b34d5fc07a24b30b195883b000e698bbafff8fd89ef933151d78` |
| [System architecture](../../../architecture/system-architecture.md) | `cab063206306102514c0b5745105324318d8db1874a6363d9c972a2f0903df7a` |
| [Review Tours architecture](../../../architecture/review-tours.md) | `c043b66647c74888c0778e0ed5e570b6222d4385352cb200fcd8e8a8b18b30e3` |
| [BPR-1375](../../../issues/60-done/BPR-1375-tool-free-writing-cannot-use-a-signed-in-claude-account.md) | `bb54c251de788640fe9114d57b28a8f7c332e4c38c45e2f47a36b6a1e702d075` |
| [RFC-0011 closeout plan](../../plans/rfc-bpr-0011-owner-closeout/milestone-plan.json) | `8ebf2a101d551a692e0452be0573ddb07339c0d9a84758b68cfe62ba8bbafca9` |
| [RFC-0012 closeout plan](../../plans/rfc-bpr-0012-owner-closeout/milestone-plan.json) | `03319ae36fe171ad31c061a2aab9d4ec511073e9ba180aba2475bc72fef75d8f` |

## Before and proposed after

| Adopted source | Proposed reconciliation |
|---|---|
| RFC-0092, owner's per-agent continuous-session decision; AI agent boundary, review brief and conversation storage | Implement continuity as the exact backend session, its tool history and workspace; retain BPR's visible transcript as a projection. Preserve per-agent/PR separation and the own-sign-in/no-direct-model-API boundary. |
| RFC-0092, tour T1 and the BPR-1914 session/replay amendments | Supersede fresh-run-only, no-native-session and in-memory-only requirements for conversational tour drafting and chats. Add per-agent thread scopes, persistent connections/resume handles and durable histories. Keep one-shot tool-free hooks separate. |
| RFC-0092, tour T3; AI agent boundary, the tour's read-only drafting | Qualify persistent/resumed native protocols under the existing read-only boundary, including the currently refused Codex App Server route. Remove the conversational no-session restriction only; unsupported permissions still refuse, even with transcript replay. Retain the documented limits of Claude's enforcement and reporting. |
| BPR-1375's no-session restriction, referenced by tour T1 | Leave that Done card and its original tool-free-writing acceptance untouched. The new conversational scope permits native transcripts; its remaining tool-free modes retain the original restriction. |
| RFC-0092, workspace section and acceptance AC06 | Replace teardown on each conversational turn/review close and unconditional startup reclamation with session-owned retention. Account for required Git objects, allowed changes and safe storage admission/refusal. Respect the accepted private workspace implementation rather than reinstating the superseded linked-worktree form or limit of three; preserve owner-private cleanup and clone non-mutation. |
| RFC-0092 acceptance AC07; RFC-0011 session binding; RFC-0012 stale-chat behavior | Preserve new-session-at-current-revision versus explicit continuation-at-old-revision. Reuse of a native session never silently moves its pinned source tree. |
| RFC-0092, Clear session/history and BPR-1931 Reset tour | Use nondestructive New session without Clear's confirmation; retain the previous generation and allow it to be reopened. Keep accepted tour work, focus, notes and drafts outside conversational reset. Reset tour still clears active tour chats/proposals with its existing confirmation and undo; native histories detach, and Undo reset restores the original associations. |
| RFC-0092, conversation bounds; AI agent boundary, session metadata; System architecture, profile/window ownership | Apply the 500-record conversation bound across both surfaces and generations, separately from the 128-entry invocation metadata catalog. Keep durable conversation identity independent of a window attachment; serialize BPR writers and preserve failed/uncertain outcomes. Record/resource admission cannot silently discard protected workspace state. |
| RFC-0092, replay mechanism and per-agent histories | Label legacy histories honestly; unprovable old numeric agent identities do not bind to current entries. Explicit replay creates a separate generation at its shown revision, preserves native provenance and discloses missing tool/filesystem state. |
| RFC-0092, BPR-1930/BPR-1937 proposals and BPR-1935 carrier | Add the versioned guide, coherent current snapshot, reason/action facts and decision outcomes. Freshness includes head/status/proposal changes, not only accepted-tour edits; compaction/reconnect may need a new bootstrap. Preserve structured proposals, captured stop identities, explicit acceptance and the existing bound on the entire composed prompt. |
| Review Tours source/preparation presentation and BPR-1936's source labels | Share semantic reasons with the agent and presentation, preserve confirmed bindings through text/order edits and stop treating author-selected mapping alone as an attention reason. Keep source confirmation, explanation reaffirmation, Ready and publication eligibility distinct, including the existing readiness consequences of prose changes. |
| RFC-0011 and RFC-0012 host allocation and process/public-path evidence | Extend host ownership from one-turn launches to long-lived protocol sessions and durable scoped resume. Preserve provider/host restrictions, credentials, explicit invocation, cancellation and no automatic publication. |

## Work, plans and prior acceptance

RFC-0092 contains its own original setup milestones; neither those milestones
nor the completed RFC-0011/0012 closeout plans are reopened or rewritten by
this draft. Use the new [authored plan](milestone-plan.json) for follow-up
work. Existing completed BPR-1753, BPR-1900, BPR-1914, BPR-1930, BPR-1936 and
BPR-1937 remain historical evidence for their exact candidates, not proof of
native persistence, the new controls or the corrected context projection.
Live statuses, counters, issue lanes, protected source packs and remaining
native/human/security obligations are unchanged.

An authorized implementation session first reconciles overlapping writers,
imports the provisional IDs, adopts this scoped amendment and binds the new
criteria. Check any frozen attended candidate whose inputs would change
before replacing its source. Update relevant live prose and follow-up
verification without rewriting Done-card verdicts. The owner's session
preferences and answered choices are in [DECISIONS.md](DECISIONS.md).

Implementation can choose the host's durable-record layout and adapter
mechanisms within the RFC's behavior. It must qualify the chosen runtime
versions and their resume restrictions; public documentation is capability
evidence, not a substitute for an installed-agent course.
