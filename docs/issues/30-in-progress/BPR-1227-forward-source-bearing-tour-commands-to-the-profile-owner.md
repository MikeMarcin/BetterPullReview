---
schema: "implement-rfc.card.v1"
id: "BPR-1227"
title: "Forward source-bearing tour commands to the profile owner"
status: "In Progress"
rfcs: ["RFC-BPR-0045"]
depends_on: ["BPR-1062"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","origin":"discovered while accepting BPR-1061 and BPR-1062: with a desktop owner, the CLI forwards local edit commands but answers interaction_required to every command that reads the provider","reservation":"docs/issues/_reservations/BPR-1227--BPR-1227--3f02ead5aa45.json"}
---

# BPR-1227: Forward source-bearing tour commands to the profile owner

## Observation

The owner channel of [[BPR-1061]] and the live-draft routing of [[BPR-1062]]
carry `load`, `mutate` and `request_status` for local draft commands: status,
show, edit, move, remove, ready, undo and redo work against a running desktop.
`add`, `open`, `show --source`, `walk`, `preview --refresh`, `publish` and
`recover` answer `interaction_required` while a desktop owns the profile,
because they read the provider through profile-owned stores (epoch catalog, git
cache, device bindings) that only the owner may open.

RFC-BPR-0044's everyday flow includes `bpr tour add PATH:LINES`, and
AC45-05 says CLI commands work against a running desktop. Adding a stop from
source with the desktop open is therefore not yet possible.

## Outcome

Add a finite, versioned `source` operation to the tour channel. The owner
resolves the pinned destination through its own provider composition on its
serialized lane and returns the bounded source material; the CLI builds the same
stop it builds standalone and forwards it as an ordinary mutation. Provider reads
never reach the CLI process and no second process opens a profile store.

`open` for a pull request the owner has not composed, `preview --refresh`,
`publish` and `recover` remain separate command classes: publication needs live
authorization in the invoking presentation and stays `interaction_required`
through this channel.

## Direct acceptance

The shipped CLI adds a stop from exact source through a real desktop owner and
the stop equals the standalone result. Wrong-profile, oversized, unpinned and
unavailable-source requests refuse without a partial draft. A provider read is
never a mutation and adds no request record. Real provider evidence stays with
the designated safe target; fixtures cover the rest.
