---
schema: "implement-rfc.card.v1"
id: "BPR-1884"
title: "Keep the BetterPullReview-managed clone current, so agent deltas stay small"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"BPR-1879 implementer's open item; owner asked for the card 2026-11-06","related_cards":["BPR-1873","BPR-1879"]}
acceptance: []
---

# BPR-1884: Keep the BetterPullReview-managed clone current, so agent deltas stay small

## Problem

BPR-1879 creates a clone owned by BetterPullReview under
`<profile>/agent-clones-v1/HOST-REPOID`. After that, nothing fetches into it.
Each agent question builds its private workspace with BPR-1873's delta fetch,
using the kept clone as its alternate, so the delta is everything the clone
lacks. As the clone ages that delta grows. On an active repository it
eventually exceeds the 32 MiB per-file bound, and the question fails as
`too_large`. The owner wants the app to just work.

## Change

- Fetch into the **managed** clone only, never into a reviewer-linked clone,
  which stays untouched (BPR-1755 Q2, RFC-BPR-0092 Q8). Use the transport's
  existing gh credential path with the same pinned private configuration.
  Fetch only what the pull request needs: its head OID and its base OID,
  each when the clone lacks it. Never fetch the default branch: a head fetch
  already brings in every ancestor the clone lacks (owner, 2026-11-06,
  superseding "fetch the default branch, plus the PR head").
- Choose when to fetch, and record the choice in the RFC amendment. Options:
  before an agent question when the clone is older than some threshold; in
  the background after a question; or on its own idle schedule while a review
  of that repository is open. The fetch must never block a question longer
  than today's delta fetch would. It must be cancellable, and must run on the
  serialized host lane, never in a widget.
- The fetch into the clone runs under its own runner without the 32 MiB
  per-file bound, like the initial clone (BPR-1879 v002): it is the clone's
  own maintenance, not a per-question download. A workspace delta still keeps
  its bounds.
- A fetch that fails leaves the clone usable. The question proceeds with the
  delta path, and the failure is recorded in diagnostics.
- Storage shows the clone's last-updated time. If it is cheap, also run
  `git maintenance` / `gc --auto` on the managed clone so its size stays
  bounded.

## Acceptance

- AC1. After an agent question on a repository with a managed clone, the
  clone is updated per the chosen policy, and the next question's workspace
  delta covers only commits newer than that update. Test this on a fixture
  where the upstream advances between questions.
- AC2. A reviewer-linked clone's refs, objects and config are byte-identical
  before and after (inventory check).
- AC3. A failed or cancelled maintenance fetch leaves the clone valid, and
  the question still succeeds through the delta path.
- AC4. No maintenance fetch runs inside the agent sandbox or with the agent's
  environment.
- AC5. The RFC-BPR-0092 amendment records the policy, and
  `python3 scripts/verify_documentation.py --root .` passes.
