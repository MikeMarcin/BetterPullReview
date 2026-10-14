---
schema: "implement-rfc.card.v1"
id: "BPR-1502"
title: "A gh reconnect gives the restarted window's tour draft a new owner"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-1502--BPR-1502--a864d86e31d2.json","origin":"observed while diagnosing BPR-1501 for the 0.2.0 release, 2026-10-14","related_cards":["BPR-1501","BPR-1494","BPR-1409"],"related_rfcs":["RFC-BPR-0071"]}
---

# A gh reconnect gives the restarted window's tour draft a new owner

While diagnosing [[BPR-1501]], the CLI tour course showed that a restarted
application which connects through `gh` again gets a different host
authority fingerprint, because the fingerprint includes the profile revision
(`f5e4319e…` before the restart, `4f5f532b…` after). The restored window's
tour owner therefore differs, and it loads an empty draft (revision 1, no
stops), so a tour draft written before the restart does not show in the
restarted window. A saved publication still resumes from its captured plan.

It is the same family as [[BPR-1494]], where a native sign-in minted a new
connection identity.

## Acceptance

- AC1: The cause of the profile revision change on a `gh` reconnect is named,
  and whether it is intended under RFC-BPR-0071 is decided.
- AC2: If it is not intended, a tour draft written before a restart shows in
  the restarted window after a `gh` reconnect to the same account, without
  weakening account isolation; a public-path test shows it.
