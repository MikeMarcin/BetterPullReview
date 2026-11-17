---
schema: "implement-rfc.card.v1"
id: "BPR-2249"
title: "Seen in the review closes the pull request the way Approve does"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"change","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner request, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2249--BPR-2249--3017b2614c30.json","related":["BPR-1816","BPR-2049","BPR-2217"],"acceptance":["BPR-2249:same-setting","BPR-2249:after-recorded","BPR-2249:failure-stays","BPR-2249:same-notice"]}
---

# BPR-2249: Seen in the review closes the pull request the way Approve does

## Report

The owner, 2026-11-17: "Seen in the compose review menu should close the PR
same way approve does"

[[BPR-2217]] put Seen beside Approve in the review. Approve, once GitHub
confirms it, closes the review page and, by Settings › Review › Finishing a
review ([[BPR-1816]]), closes the review and shows Home. Seen recorded the
new commits as seen and left the review page open.

## Scope and acceptance

- **BPR-2249:same-setting.** Seen follows Approve's own finish choice: ticked
  (the default), the review closes and Home shows; unticked, the review page
  closes and the pull request stays open. The core decides the transition.
- **BPR-2249:after-recorded.** It closes only once the queue has recorded the
  seen head (the store answered); a refused Seen (head moved, no longer
  offered) or a store that fails keeps the review page open.
- **BPR-2249:failure-stays.** On failure the review stays open, Seen is
  offered again (the unstored seen head is withdrawn) and the status line
  says the commits were not marked seen.
- **BPR-2249:same-notice.** The notice is Approve's shape: the status line
  and a polite accessible announcement, with "Closed the review of …"
  appended when the review closes. Approve has no undo, so neither does Seen.

The Pull Request menu's Mark New Commits Seen is not the review's Seen and is
unchanged.

## Intended verification

A core test of the Seen transition per setting; an application test of the
queue host's answer (recorded, refused, store failure withdrawing the seen
head); the composed `--seen` course closing to Home on the default setting,
and a new course where the store fails and the review stays open.
