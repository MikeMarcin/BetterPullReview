---
schema: "implement-rfc.card.v1"
id: "BPR-2340"
title: "An inline comment published under load falls back to a file permalink while its comparison is not ready"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2339--BPR-2342--60dd1b567452.json","origin":"BPR-2215 known 0.5.0 intermittent, waived by the owner's standing rule (2026-11-21)","related_cards":["BPR-2215"]}
---

# BPR-2340: An inline comment published under load falls back to a file permalink while its comparison is not ready

## Finding

`application_inline_publish_cli_consent` fails intermittently under suite
load (seen on Windows at `25e214ab5`: "a new inline draft holds its text";
it passed in the next full suite). Under load the comment is published as a
file permalink with "Its comparison is not ready" instead of on its line: the
course's rereads finish before the comparison is accepted, and the
publication takes the fallback.

## Direction (needs a product answer)

- The course should wait for the comparison after its rereads before it
  publishes.
- Whether publishing while the comparison loads should fall back to a file
  permalink at all, or wait or refuse, is the owner's to decide.

## Acceptance

- AC1. The owner's answer on the fallback is recorded and the product follows
  it.
- AC2. The course waits for the comparison it publishes against, and passes
  20 of 20 under suite load on Windows.
