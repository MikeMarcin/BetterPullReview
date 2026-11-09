# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **native-agent-sessions-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## native-agent-tab

The Agent tab continues the real backend conversation and preserves the identities, workspace and lifecycle needed to resume it.

**Work:** `BPR-1951`

**Prerequisites:** None

**Completion:**

- Claude stream-json and qualified Codex conversations continue natively through the public Agent tab without closing a live connection after each answer.
- Durable identity independent of window activation, retained workspace recovery, safe storage admission, stale revisions and Stop/uncertain-send fencing have direct functional evidence.

**Boundary:** A usable native Agent-tab conversation; the shared session host is available for tour integration.

## tour-sessions

Each tour side chat has its own native session per agent, and both surfaces support New session with retained, reopenable history.

**Work:** `BPR-1952`

**Prerequisites:** native-agent-tab

**Completion:**

- Tour, Problem, Solution and stable-stop chats remain isolated by agent and resume through normal close/reopen cycles.
- Nondestructive New session, reopening history and Reset tour/Undo reset preserve the defined BPR state and session associations without redirecting pending work or late completions.

**Boundary:** Usable native conversations and scoped history in both requested product surfaces.

## tour-context

A continuing tour agent understands current BPR tour state and explains source warnings using the same reasons that drive the UI.

**Work:** `BPR-1953`

**Prerequisites:** tour-sessions

**Completion:**

- Versioned tour guidance and coherent snapshots refresh all relevant facts, distinguish source confirmation from Ready/publication, and include actual blockers and available actions.
- Text/order updates preserve confirmed anchors under the existing preparation rules; compaction/reconnect restores current context, and changed source state agrees in core, UI and agent context.

**Boundary:** The reported warning/context problem has a distinguishing public-path course without inventing a reproduction of the owner's exact prior session.

## qualified-continuation

Native continuity, legacy migration and explicit replay fallback are qualified and documented for the selected installed scope, with remaining installed Claude verification explicitly waived by owner amendment native-agent-sessions-claude-waiver-r1.

**Work:** `BPR-1954`, `BPR-1955`

**Prerequisites:** tour-context

**Completion:**

- Unsupported backends and unavailable or legacy native state offer explicitly identified replay generations or New session, without identity reassignment, permission bypass, silent downgrade or duplicate submission.
- Installed Codex protocols, composed controls, crash/storage/idle boundaries, retained-context privacy, Release reuse/cost observations and operator disclosures have actual scoped evidence. Owner amendment native-agent-sessions-claude-waiver-r1 waives only remaining installed Claude verification; Claude behavior and usage remain unverified and its actual failures are preserved.

**Boundary:** The proposed behavior is reviewable with honest runtime support and cost claims; unrelated native qualification is not inferred.

## accepted-scope

Every criterion of the adopted native-session RFC is judged against real evidence and the changed live contracts are reconciled.

**Work:** `BPR-1956`

**Prerequisites:** qualified-continuation

**Completion:**

- The closeout accounts for every RFC criterion, source amendment, focused public-path check and required documentation gate.
- Any remaining required installed, platform or human evidence stays explicit rather than being replaced by authoring or fixture validation.

**Boundary:** Acceptance of the authorized scope only, with the repository's preserved historical and release obligations.
