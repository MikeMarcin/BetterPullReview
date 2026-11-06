---
schema: "implement-rfc.card.v1"
id: "BPR-1879"
title: "BetterPullReview can keep its own clone for agent questions"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1873"]
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"feature","found_by":"split from BPR-1873 AC4 (owner decision 2026-11-05)"}
acceptance: []
---

# BPR-1879: BetterPullReview can keep its own clone for agent questions

Split from BPR-1873, which landed the linked-clone delta fetch, the PR-only
workspace and the Repository step's choice (link a clone / this pull request
only). This card adds the third option the owner asked for: **Let
BetterPullReview keep one**.

## Change

- Add the option to the Repository step, beside linking a clone; state the
  approximate download from GitHub's repository `size` and the location on
  disk.
- A full clone (RFC-BPR-0092 amendment: not blobless, so no lazy blob fetch
  can run in the agent's sandbox), owned by the profile under its application
  data, made in the background with progress and Cancel, outside the
  transport's temporary-store limits, through BetterPullReview's own gh
  credential and pinned private Git configuration.
- Later questions use it as the workspace's alternate exactly like a linked
  clone (BPR-1873's delta fetch), keeping it current by fetching into it.
- It appears in Insights › Storage and is removed by profile purge.

## Acceptance

- AC1. Choosing the option creates the clone once, with the stated size and
  location, cancellable, and the question continues when it finishes.
- AC2. Later questions reuse it and fetch only deltas.
- AC3. It is shown in Storage and removed by profile purge.
- AC4. The owner accepts the option's look (attended review).
- AC5. `python3 scripts/verify_documentation.py --root .` passes.
