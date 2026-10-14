---
schema: "implement-rfc.card.v1"
id: "BPR-1507"
title: "Make file Accept available in one click"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Accept should be the default action on a file so it's one click, or we redesign that element so it can easily be one click.","milestone_plan":{"id":"bpr-1507-one-click-file-accept","path":"docs/rfcs/plans/bpr-1507-one-click-file-accept/milestone-plan.json"}}
---

# BPR-1507: Make file Accept available in one click

Make Accept the default action on the file control, or revise that control so Accept is readily available with one click. The owner authorizes either approach; choose the smallest coherent solution after inspecting the existing interaction.

## Acceptance

- AC1: An eligible file can be explicitly Accepted with one click from its normal file control, without opening a menu first.
- AC2: The action and its current state are clear; existing alternate file actions remain available.
- AC3: Accept retains existing exact file/epoch authority, persistence and eligibility rules. Opening a file or automatically marking Seen does not implicitly Accept it.
- AC4: Verify the rendered control offscreen and exercise its composed one-click action, including relevant unavailable/stale states.

## Coordination and review

BPR-1506 owns automatic Seen on opening a file. Coordinate any shared file-control or receipt paths before editing; keep automatic Seen distinct from explicit Accept. Load frontend-design:frontend-design before UX work as required by AGENTS.md. Prepare the required short attended review with an exact frozen candidate if human visual judgement remains. Never operate or capture the owner's live desktop.

## Bounded milestone plan and design

Mandate revision `owner-2026-10-14`; source baseline `ae7d22844`.
The adopted [single-card plan](../../rfcs/plans/bpr-1507-one-click-file-accept/milestone-plan.json)
preserves AC1–AC4 and the exact-candidate human visual decision. Coordinator
owns integration and any following BPR-1515 hotkey milestone.

Loaded `implement-rfc` and `frontend-design:frontend-design` before UX edits;
the latter was found in the installed Claude plugin marketplace at
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
The design preserves the native application font, palette roles and existing
progress-mark colours. It adds no palette or typography tokens. The horizontal
reading order is `[file/change navigation] [Accept] [current-state menu]`, with
Compose review and Comment anchored at the right. A named direct action beside
the existing state menu fits the existing strip renderer and leaves Include
available when Accept is disabled for an excluded file. The same QAction
supplies exact command, enablement, checked state and Revoke acceptance label;
only the persisted receipt changes the mark. The initial split-button proposal
was discarded before code because disabling its default action would also
disable access to Include.

Coordinated shared functions with BPR-1506: this slice changes Qt control
construction, focus order and receipt presentation; the composed course's
acceptance steps click the direct presenter. Automatic Seen remains separate.

## Engineering checkpoint

Branch `bpr-1507-one-click-file-accept`, exclusive source worktree
`/Users/jaeger/.codex/worktrees/bpr1507-file-accept/BetterPullReview`.
Plan commit `d6b3b10b7`; claim commit `60f4b965f`.
MASK registration was absent; applied the exact previewed register plan
`sha256:048783180a91cb21ae4d4bb7585ef2752bcdd3d9ef7a98509fe48a3e6d511512`.
The prescribed preflight reports `ready_pinned`, Codex
`gpt-6.1-sol`/`xhigh`, with host loading unverified by that filesystem check.
No NEXT_ID or reservation bytes were authored by this implementer; only
coordinator-authorized testing-note import commits were cherry-picked to satisfy
the shared-ref high-water guard. No owner desktop or browser data was accessed.

Candidate verification and attended handoff will be recorded here before the
card boundary. Runtime logs and renders remain uncommitted under
`build/bpr1507-runtime`; command ownership is this implementer only.
