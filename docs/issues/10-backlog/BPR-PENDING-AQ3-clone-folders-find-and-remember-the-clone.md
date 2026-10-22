---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ3"
title: "Clone folders find the reviewer's clone by its remote and remember it per repository"
status: "Backlog"
rfcs: ["RFC-BPR-0091"]
depends_on: ["BPR-PENDING-AQ1"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0091 adoption, 2026-10-22 (owner decision D2)","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0091 M3"}
---

# BPR-PENDING-AQ3: Clone folders find the reviewer's clone by its remote and remember it per repository

RFC-BPR-0091 M3 ([RFC](../../rfcs/RFC-BPR-0091-agent-first-run-setup.md#milestones)), owner decision D2.

## Scope

- The core's `.git/config` remote reading, URL normalization (HTTPS, SSH,
  scp-style), user-information removal and repository matching.
- The adapter's bounded read of listed folders: two levels, no links, at most
  2,000 directories per folder, `.git/config` (64 KiB) and `.git/HEAD` only.
- The stored clone folders and the per-repository link by GitHub identity,
  checked again before each use.

## Acceptance criteria

- AC1: RFC AC04's fixture folders give exactly the matches and order defined
  in the RFC, with no process, no write and no user information anywhere.
- AC2: RFC AC05's remembering, rename, moved clone and Forget behave as
  defined.
