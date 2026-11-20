---
schema: "implement-rfc.card.v1"
id: "BPR-2329"
title: "Auto-merge is shown on the Overview and can be enabled or disabled there"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2327"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","origin":"owner request, 2026-11-20, while BPR-2327 was under way","reservation":"docs/issues/_reservations/BPR-2329--BPR-2329--7f57d17cb2c6.json","split_from":"BPR-2327","related_cards":["BPR-1852","BPR-2327"],"related_rfcs":["RFC-BPR-0085"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"]}
---

# BPR-2329: Auto-merge is shown on the Overview and can be enabled or disabled there

## Asked

The owner, 2026-11-20, after asking for a better conflict status
([[BPR-2327]]): "It also doesn't look like we can view or control
auto-merge." Split from BPR-2327, which shows auto-merge's line when it is
on; this card adds the control.

## Acceptance

- AC1 View: Merge says whether auto-merge is on (with the merge method and who
  enabled it, when GitHub says) or off, whenever the repository allows
  auto-merge. A repository that does not allow it shows no auto-merge line and
  no control.
- AC2 Offer: Enable auto-merge is offered only when the repository allows
  auto-merge, GitHub says the viewer can enable it, the pull request is open
  and auto-merge is off. Its menu lists only the merge methods the repository
  allows. Disable auto-merge is offered only when auto-merge is on and GitHub
  says the viewer can disable it. The core decides both.
- AC3 Mutation: each is an explicit gesture, written once through the
  application's review-mutation host under a one-shot user grant bound to
  the accepted pull request and the acting account, as Ready for review is
  ([[BPR-1852]]). GitHub is asked first; nothing is sent when auto-merge is
  already in the state asked, the pull request is closed, or the viewer may
  not change it. A lost answer is read back, never sent twice.
- AC4 Refusals: each refusal says why in words (not permitted, not allowed
  for this repository, method not allowed, closed, already on or off, GitHub
  refused, connection cannot), and nothing changes.
- AC5 Reflect: GitHub's answer moves Merge in place at once and is held until
  a read agrees.
- AC6 Log: one Standard diagnostic event per enable and per disable with its
  outcome and no content (no names, branches, logins or messages).
- AC7 Evidence: fixture courses for view, enable, disable and a refusal at
  the core, GitHub contract, application host and Qt layers. Any live check
  only on MikeMarcin/BPR-Test, disabling anything it enables.
