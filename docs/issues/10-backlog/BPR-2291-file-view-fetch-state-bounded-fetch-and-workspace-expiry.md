---
schema: "implement-rfc.card.v1"
id: "BPR-2291"
title: "What BPR-2288 still needs: the file view says it is fetching and can cancel, a fetch is bounded by progress and needs no history without a linked clone, and retained workspaces expire under a size cap shown in Settings"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2288--BPR-2291--19d6a88ae1a3.json","parent":"BPR-2288","origin":"split from owner card BPR-2288 (2026-11-19) after BPR-2289 and BPR-2290","related_cards":["BPR-2288","BPR-2289","BPR-2290","BPR-1972"]}
---

# BPR-2291: What BPR-2288 still needs

[[BPR-2289]] and [[BPR-2290]] landed the cheap parts of [[BPR-2288]]. This card
holds the rest; the owner's card states the reasons in full.

## Scope

1. **File view state (BPR-2288 Change 4, AC3/AC4's view half).** While a file
   waits on an acquisition, it says "Fetching the base commit from <host>…",
   with bytes when known, and a Cancel that stops it; after a timeout it says
   what failed and what the reviewer can do. The `historical_acquire` and
   `protocol_acquire` stages already log the facts.
2. **Progress timeout (Change 3).** The clone runner's deadline becomes an
   idle bound: a clone still receiving data (its `tmp_pack` growing) is not
   killed at two minutes; one that receives nothing for N seconds is.
3. **No history without a linked clone (Change 2, AC2).** A complete
   acquisition with no linked clone must not clone the commit's history: fetch
   it shallow (`--depth=1 --filter=blob:none` and the shown files' blobs), or
   allow `complete_closure` only below a repository-size threshold (GitHub's
   `diskUsage`). This changes what a complete review's cache holds and needs
   the cache's closure checks to accept it.
4. **Workspace expiry (Change 5, AC6).** Retained workspaces are removed after
   their session ends, after N days unused, and under a total-size cap, oldest
   first; Settings shows their total size with Remove. The 113 GB already on
   disk is reclaimed by this, not by hand.
5. **Checkout cost (AC5 "ready in seconds").** A retained workspace still
   writes every file of a large head; check out from the linked clone's index
   as a cache, or sparsely (the pull request's files, widened on demand).

## Acceptance

BPR-2288's AC2, AC3 (message), AC4 (view), AC5 (seconds), AC6 and AC7.
