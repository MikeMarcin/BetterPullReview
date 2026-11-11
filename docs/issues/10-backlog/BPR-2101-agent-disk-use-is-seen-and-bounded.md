---
schema: "implement-rfc.card.v1"
id: "BPR-2101"
title: "Agent workspaces and scratch folders: disk use in Insights, cleanup in Settings and by pull request state"
status: "Backlog"
rfcs: ["RFC-BPR-0096", "RFC-BPR-0104"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2101--BPR-2101--44805dfe5f1b.json","origin":"owner look e97c6a3d (RFC-BPR-0104-AC03-bound, Reject), 2026-11-11","related_cards":["BPR-2007"]}
depends_on: []
---

# Agent workspaces and scratch folders: disk use in Insights, cleanup in Settings and by pull request state

## Origin

The owner, rejecting the turn-start scratch bound in session `e97c6a3d`
(2026-11-11): "This needs to be a configurable setting. We also need to make
sure that we clean up scratch folders somehow, maybe once the PR is approved
or removed from the list or just over time. I'm not entirely sure but we need
to make sure that our app doesn't just consume disk space without bounds. And
we should be able to see disk space usage from workspaces and scratch folders
in insights and settings should give options to clean up too."

## What BPR-2007's post-review round already does

The scratch folder's limit is a setting (256 MiB to begin with) and a turn
that passes it is stopped; scratch folders unused for a set number of days
(30 to begin with) are removed at startup; Settings › Agents & context shows
what the scratch folders hold and has Clear scratch folders. See
[RFC-BPR-0104](../../rfcs/RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md).

## Scope and acceptance

1. Insights shows the disk used by agent workspaces (the disposable checkouts
   RFC-BPR-0096 retains with native conversations) and by scratch folders,
   measured off the decision lane with a bounded walk, and says when a
   measure was cut short.
2. Settings › Agents & context shows the retained workspaces' total and
   offers to remove those of inactive conversations (all, or unused for N
   days), using RFC-BPR-0096's exact owned-path cleanup; active work, pending
   proposal provenance and modified or unverifiable contents are protected
   and reported as kept.
3. A pull request merged, closed, or removed from the reviewer's list has its
   agents' scratch folders and retained workspaces removed (the
   conversation's transcript stays readable; native resumption becomes
   unavailable, as RFC-BPR-0096 says of workspace loss). Which states clean
   up is a setting.
4. Nothing grows without a bound: the retained workspaces have a total limit
   (a setting) past which the oldest inactive generation is reclaimed, as
   RFC-BPR-0096's "bounded storage policy" asks, with the thresholds recorded.
5. Owner look on a frozen build: Insights' disk section and the Settings
   cleanup rows, Accept or Reject each.

Load frontend-design before the Qt work. RFC-BPR-0096's owner large-project
amendment ("No automatic deletion or larger replacement cap is introduced")
is amended by this card's acceptance: the owner asked for bounded disk use.
