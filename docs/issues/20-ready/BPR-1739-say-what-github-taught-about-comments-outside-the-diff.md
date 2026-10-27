---
schema: "implement-rfc.card.v1"
id: "BPR-1739"
title: "Say on the server card and in Support details what GitHub taught about comments outside the diff"
status: "Ready"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-1736"]
priority: "P3"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1739--BPR-1739--23e3961ce427.json","kind":"feature","origin":"BPR-1736 AC6: the editor's words shipped; the server card's line and the published thread's Support details line did not","related_cards":["BPR-1736","BPR-1735","BPR-1737"],"frontend_design":"load frontend-design:frontend-design before the words and layout","triage_20261026":{"date":"2026-10-26","disposition":"updated","reason":"Its dependency BPR-1736 is Done."}}
---

# BPR-1739: Say on the server card and in Support details what GitHub taught about comments outside the diff

[[BPR-1736]] learns, from GitHub's own answer to a reviewer's comment, whether
a server takes a line comment outside its diff, and keeps the fact per
profile, repository, server version and operation in the recovery journal
(`off_hunk_fact`, with the repository, pull request, path and line that
taught it). The editor says before Publish what was learned. Two surfaces
BPR-1736's design named are not built:

- **Server card, Support details** (Settings, Accounts and hosts): one line
  per profile and host, from the newest current entry, in the card's words:
  "Comments outside the diff: accepted (learned 22 Oct from owner/repo,
  GitHub Enterprise Server 3.23.1)", "not accepted (learned …)" or "not
  known yet". It never says "supported by this version".
- **A published native thread at a line outside the diff:** its Support
  details say "GitHub accepted a comment outside the diff on this server."

## Notes

- The store has `load_scope` (one repository) only. The server card needs the
  newest current entry of a profile and host across repositories: one more
  bounded read on `application::off_hunk_fact_port`, decided current by
  `core::current_off_hunk_fact`.
- The settings composition opens the profile's recovery journal on its own
  connection (`adapters::sqlite::make_off_hunk_fact_store`); a widget reads
  nothing itself.
- No host has accepted such a comment (BPR-Test #125), so "accepted" and the
  thread line rest on fixtures.

## Acceptance

- AC1: The server card's Support details name what was learned, where and
  when, or "not known yet", in the words above, from the core's current fact.
- AC2: A published native thread at a line the core placed outside the diff
  says in its Support details that GitHub accepted it on this server.
- AC3: Neither line is derived from the host's name, product or version.

## Triage, 2026-10-26

[[BPR-1736]] is Done, so the card moves to Ready.
Load frontend-design before writing the words and layout, as the metadata
says.
