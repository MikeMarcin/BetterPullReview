---
schema: "implement-rfc.card.v1"
id: "BPR-2357"
title: "Update an authored pull request from its target with an agent"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request, 2026-11-23","reservation":"docs/issues/_reservations/BPR-2357--BPR-2357--f1fd050f3098.json","related_cards":["BPR-2327"],"related_rfcs":["RFC-BPR-0085","RFC-BPR-0101"],"acceptance":["AC1","AC2","AC3","AC4"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"5f9d190a6404238d566b2600e69b09f94a1832f2","evidence":{"kind":"observation","detail":"application_pull_request_overview, qt_pull_request_overview_page and application_update_from_target_public_path: exact actor versus matching login, closed/signed-out/deleted head, clean author, stale conflict and visible conflict suggestion checks passed."}},{"criterion":"AC2","outcome":"pass","revision":"5f9d190a6404238d566b2600e69b09f94a1832f2","evidence":{"kind":"observation","detail":"application_update_from_target_public_path: one Status press produces one fixture Agent turn against the accepted stacked PR, with target feature-a, fork head feature-b and the exact expected head; fixture agent answers and retires."}},{"criterion":"AC3","outcome":"pass","revision":"5f9d190a6404238d566b2600e69b09f94a1832f2","evidence":{"kind":"observation","detail":"application_update_from_target_public_path, sqlite_workbench_settings_storage and sqlite_agent_configuration_storage: shipped prompt, replacement carried beside branch binding, version-38 upgrade, durable override and earlier prompt provenance passed."}},{"criterion":"AC4","outcome":"pass","revision":"5f9d190a6404238d566b2600e69b09f94a1832f2","evidence":{"kind":"observation","detail":"application_update_from_target_public_path passed locally on macOS; reviewer gets neither placement even with a conflict and programmatic presses submit no turn. Qt input, architecture and network manifest checks passed."}}]
---

# BPR-2357: Update an authored pull request from its target with an agent

The owner wants one button that asks the agent to fetch the pull request's
target branch, merge it into the pull request's branch, resolve conflicts, and
push the update. The guidance is editable in Settings → Prompts.

## Acceptance

- AC1: An open pull request's authenticated author can start the action. The
  Status drawer suggests the same action when GitHub reports a merge-blocking
  conflict. The action is absent for other accounts, signed-out and closed
  reviews, and a fresh authority check guards each press.
- AC2: A single press sends one agent turn about the accepted pull request,
  with its actual target repository and ref and its actual head repository,
  ref and commit. A fork keeps its own push destination. The turn uses the
  existing agent setup, permissions and workspace path.
- AC3: Settings → Prompts shows the shipped guidance and lets the user
  supplement or replace it. Saved guidance reaches the agent while the
  application provides the current destination independently.
- AC4: Local fixtures exercise the composed action and prove non-author
  refusal, conflict suggestion and exact fork/target routing without spending
  agent usage or updating a live pull request.

## Delivered

The author gets **Update from target with agent** on Overview. A fresh merge
conflict offers the same action in Status. Each press rechecks the accepted
account, author, pull request and revision. The agent gets the exact target
ref, the head repository and ref, and the expected head commit. Settings →
Prompts → System supplies the replaceable Update from target guidance.

## Verification

Tested on macOS with the local fixture agent at source revision `5f9d190a6`.
The composed course passed one Status press through the standard Agent turn
on a fork targeting `feature-a`, read back the replacement guidance, and
exercised the reviewer path. The ordinary Qt, application Overview, settings
storage, prompt provenance, core review-preparation, architecture boundaries,
network isolation manifest and documentation checks passed.

The extra Cocoa offscreen Overview run reached the same existing
conflict-row Return/focus failure on the baseline and candidate. The
initial core-only build ran out of disk in its application dependencies.
After the verified dev outputs were cleared, the core-only build and
review-preparation course passed. Verification used fixtures throughout.
