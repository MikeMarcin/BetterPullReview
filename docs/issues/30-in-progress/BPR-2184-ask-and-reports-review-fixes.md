---
schema: "implement-rfc.card.v1"
id: "BPR-2184"
title: "Ask and reports: the critical review's fixes"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2168"]
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2184--BPR-2184--241b88a88736.json","origin":"critical review of Ask and reports at 5260527d6, relayed by the coordinator 2026-11-14"}
---

# BPR-2184: Ask and reports: the critical review's fixes

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md),
after its close by [[BPR-2168]].

## Scope

- **B1 (blocking):** a wide Was this addressed? row counts every changed
  thread, but a press keeps only 32 and only those the window holds. Core
  applies the cap and returns the exact identities and count; the press sends
  exactly those. Test with 33 or more changed threads.
- **N1:** an agent-written `bpr-draft:` link triggers Add as draft comment.
  Actions leave the hrefs. Test that a hostile anchor drafts nothing.
- **N2:** the architecture gate gathers every violation before it fails;
  QTextBrowser and rich-text setters outside the denying document join the Qt
  rules, Qt file APIs the Qt effect rules; the fixture agent is pinned to
  BUILD_TESTING where there is a clean place. It passes at HEAD and catches a
  seeded QTextBrowser.
- **N3:** a merge's commit count is asked for when What changed is composed,
  with the bound decided in core, not inside every pull request read.
- **N4:** the Q6 baseline choice moves into core (the window's call is the
  queue implementer's to switch).
- **N5:** a report is a host-owned record keyed by a stable turn identity, not
  Qt content keyed by a turn index.
- **N6:** the report page's derived colours (dim, faint, strong, link) are
  derived in core and held to the contrast floor.
- **N7:** the commit listing is given the pull request's whole commit set
  apart from the slice it lists.
- **N8, N10:** the report view's tests feed hostile hrefs and follow links by
  click and Return; Tab reaches the page's links.

## Acceptance

Each item above has its course; `architecture_boundaries` is green. Nothing
the owner sees changes except the faint text's contrast, so no look.
