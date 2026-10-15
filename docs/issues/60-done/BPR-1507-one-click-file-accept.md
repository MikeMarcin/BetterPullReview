---
schema: "implement-rfc.card.v1"
id: "BPR-1507"
title: "Make file Accept available in one click"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Accept should be the default action on a file so it's one click, or we redesign that element so it can easily be one click.","milestone_plan":{"id":"bpr-1507-one-click-file-accept","path":"docs/rfcs/plans/bpr-1507-one-click-file-accept/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e483ef9997f3e85632e44c8c46c0fb34682cd792","evidence":{"kind":"observation","detail":"The offscreen physical mouse click test dispatches one exact accept command without opening the menu; the composed ledger seed course clicks the normal button for Accept/Revoke/Accept and observes seven exact durable completions."}},{"criterion":"AC2","outcome":"pass","revision":"e483ef9997f3e85632e44c8c46c0fb34682cd792","evidence":{"kind":"observation","detail":"Inspected light/dark state-sheet renders and the full composed diff window. Accept is named; accepted state exposes Revoke acceptance and the existing state icon; all four alternate menu actions remain. Focus and toolbar geometry pass at 1.0, 1.25 and 2.0 scale."}},{"criterion":"AC3","outcome":"pass","revision":"e483ef9997f3e85632e44c8c46c0fb34682cd792","evidence":{"kind":"observation","detail":"The direct presenter uses the existing QAction and exact projected file/material command. Tests prove Seen projection emits no acceptance, no optimistic checked state, disabled excluded/loading/unavailable state, replacement material identity, durable receipt round trip and changed-material reopen."}},{"criterion":"AC4","outcome":"pass","revision":"e483ef9997f3e85632e44c8c46c0fb34682cd792","evidence":{"kind":"observation","detail":"qt_review_state_icon, all six focus/title scale rows, architecture_boundaries, application_review_ledger_composition_contract, application_github_public_path_github_com and application_diff_workbench_public_path pass. Offscreen production diff render inspected; frozen launcher --check reaches src/engine/renderer.cpp."}}]
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
preserves AC1–AC4. The owner card has no human-only criterion. Coordinator
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

Product candidate `e483ef9997f3e85632e44c8c46c0fb34682cd792` changes only the
Qt control, its focused regression and composed acceptance gesture, plus the
owning UX/ledger documentation. The acceptance records are
[acceptance.json](../_checkpoints/BPR-1507/acceptance.json).

Verification on macOS 26.6.2 arm64, Qt 6.11.2:

- `cmake --preset dev` and the focused application/window-test build pass.
- `qt_review_state_icon` passes (3.44 s): real mouse click, exact command,
  committed state, excluded Include access, loading, replacement and unavailable
  states. The light/dark render sheet was inspected.
- All three `qt_workbench_focus_order_scale_*` and all three
  `qt_workbench_title_row_scale_*` rows pass (1x, 1.25x, 2x).
- `application_review_ledger_composition_contract` passes (15.26 s), including
  seed, reopen and unavailable courses through the composed durable host.
- `application_diff_workbench_public_path` passes (3.74 s); inspected its
  full-window code render through the installed Git/cache actor. The ordinary
  GitHub public-path render also passes (4.54 s).
- `architecture_boundaries` passes (30 s); Clang-format 22.1.8 on all four
  touched C++ files and `git diff --check` pass. Documentation tests: 67 pass.
- The documentation gate was run and currently refuses only the shared
  NEXT_ID/high-water mismatch: local BPR-1516 versus historical BPR-1520.
  No counter override, allocation or manual lane move was attempted.

Self-review: both presenters use the canonical acceptance action. There is no
new policy, provider capability, receipt authority, store path, or automatic
acceptance. Exact receipt material remains host-owned; the normal checked state
waits for its persisted projection. The menu keeps Include usable when the
separate Accept button is disabled. No independent/native matrix was required
for this bounded change, and none is claimed.

The initial implementer plan appended a generic human-decision exit that was
not in the owner card. This was an extra gate, not an unmet owner requirement.
The final plan corrects that interpretation without changing AC1–AC4; its
[initial plan](../_checkpoints/BPR-1507/initial-plan.json) remains intact. The
mechanical check-amendment helper refused changed exit text (it preserves every
exit byte); that refusal is recorded as a consistency limitation, not permission
or a verdict. No human decision existed, no candidate had been protected, and
the sole engineering writer reconciled and renewed the final binding. The
unused unprotected review draft was removed; no review server was started.

The frozen source/build is a self-contained optional inspection artifact, not
an owner acceptance obligation:
`/Users/jaeger/bpr1507-one-click-accept-v001.xav8xdr7`, source `e483ef9997`,
executable SHA-256
`97e7edd6763036898d3da35fbf7016f5dfc7b0bd9ec55286ae9cab0cf72cbfda`.
Its generated launcher's own `--check` passed offscreen at
`ATTENDED_FILE_SHOWN:src/engine/renderer.cpp`, head `b20cb443428212543f5911e091a0ffeb407d23cf`.
Optional launch: `python3 /Users/jaeger/bpr1507-one-click-accept-v001.xav8xdr7/launch.py`.
Remove it with `python3 scripts/freeze_attended_build.py remove` and that exact
root when no longer useful; no historical or other-agent build was touched.

All task-owned configure/build/test/freeze processes have finished. No server,
waiter, background process, descendant agent or unknown writer remains. Runtime
logs/renders remain uncommitted under `build/bpr1507-runtime`. Coordinator
performs source integration and canonical finish after reconciling its testing-
note imports/counter; BPR-1515 is a separate next milestone. Its shortcut must
use accept-only eligibility rather than binding the toggle QAction directly,
so repeated key presses cannot revoke acceptance.
