---
schema: "implement-rfc.card.v1"
id: "BPR-1283"
title: "Profile purge refuses any profile that ever planned a publication: its inventory omits publication-git-v1"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1185 documentation pass (code reading)","reservation":"docs/issues/_reservations/BPR-1283--BPR-1283--9b8c97b80602.json","related_cards":["BPR-1177","BPR-1185"]}
---

# BPR-1283: Profile purge refuses any profile that ever planned a publication

## Observation

While documenting storage, [[BPR-1185]] read `release_maintenance_files.cpp` and
found that the profile purge inventory does not list `publication-git-v1/`, the
folder Publish as Pull Request ([[BPR-1177]]) creates in the profile root. Purge
preparation would therefore refuse any such profile with
`incomplete_inventory`. Seen by code reading only; no run reproduced it.

## Outcome

Reproduce it with a profile that has planned a publication, add the folder to
the purge inventory with the same ownership and safety rules as the other
profile stores, and pin it with a test and a negative control. Keep shared
updater and registry state out of purge, as now.
