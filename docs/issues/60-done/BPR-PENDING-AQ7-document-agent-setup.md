---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ7"
title: "Document agent setup: the boundary amendment, Git model, Settings and the workbench"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ6"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22 (owner answer Q2 accepted)","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0092 M7"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "dff5dfbee", "evidence": {"kind": "command", "command": "python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py", "detail": "docs/architecture/ai-agent-boundary.md (Provider profiles: Looking for agents and Check it works; Workspaces: a linked clone's history; Context supplement: version three's brief and the review skill), docs/architecture/git-and-epoch-model.md (The reviewer's clone), docs/development/settings.md (clone folders, linked clones, Codex, the plain-text harness, the review skill) and docs/ux/workbench-specification.md (the setting up state, Ready, History) state the behavior of the AQ1 to AQ6 slices. The documentation tests pass (67); the gate reports only the provisional BPR-PENDING card IDs, which the coordinator allocates on main, and main's own NEXT_ID high water until this lane is rebased again."}}]
---

# BPR-PENDING-AQ7: Document agent setup: the boundary amendment, Git model, Settings and the workbench

RFC-BPR-0092 M7 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)).

## Scope

- `docs/architecture/ai-agent-boundary.md`: Q2's amendment of "Merely showing
  the panel performs no executable discovery", the Codex search, the
  plain-text harness, worktrees, the brief and the review skill.
- `docs/architecture/git-and-epoch-model.md`: commands run in a reviewer's
  clone and what they leave there.
- `docs/development/settings.md` and `docs/ux/workbench-specification.md`.

## Acceptance criteria

- AC1: Each document states the shipped behavior and the documentation gate
  passes.
