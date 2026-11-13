---
schema: "implement-rfc.card.v1"
id: "BPR-2021"
title: "Open and resume local PR drafts from Home or a harness"
status: "Done"
rfcs: ["RFC-BPR-0108"]
metadata: {"criteria":["AC05","AC06","AC08"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0006","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0033-open-and-resume-local-pr-drafts-from.md"}
depends_on: ["BPR-2020"]
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"3585dedc7f50ec7296cd345a44b2fa1fe87378cc","evidence":{"kind":"command","detail":"Desktop handoff after restart resumes the same draft with context as a note and no duplicate; bpr draft open from a terminal creates then resumes the same identity; forwarding to a running desktop has no course","command":"ctest --preset dev -R application_local_draft_desktop_public_path; bpr draft open --json"}},{"criterion":"AC06","outcome":"pass","revision":"af839e73d","evidence":{"kind":"observation","detail":"Reachability and keyboard pass (desktop course; Review Cycles desktop course unchanged); owner accepted Home 1 (session 6d00826a, with a selection fault) and the fixed one selection with Escape (session 767b9233, home-selection, accept)"}},{"criterion":"AC08","outcome":"pass","revision":"3585dedc7f50ec7296cd345a44b2fa1fe87378cc","evidence":{"kind":"command","detail":"Associated draft leaves In preparation, its pull request is in Home's prepared set read by the row chip and Open Local Preparation; resumed draft opens on Changes with no chat","command":"ctest --preset dev -R '^(core_local_draft|application_local_draft_desktop_public_path)$'"}}]
---

# Open and resume local PR drafts from Home or a harness

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0108](../../rfcs/RFC-BPR-0108-local-pr-drafts.md), criteria `AC05`, `AC06`, `AC08`.

Home Prepare pull request and In preparation; native local workbench; local handoff entry; secondary retained Review Cycles route.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

The same worktree resumes through GUI and handoff, while existing cycles and PR navigation remain available; prepare the focused frozen UX candidate.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Implementation record

Commits `7e359141b` (Qt page, Home author entry, prepared mark, Review Cycles
below), `1bc6cf4c1` (composition), `d4cf5da72` (`bpr draft open`, owner
channel `local_draft`, desktop course), `3225243ec` (polish), `b6decde2d` and
`3585dedc7` (attended look and its generated worktree). Product candidate
`3585dedc7`. Frontend-design was loaded before the Qt work. Results and
limits: [ledger](../../verification/RFC-BPR-0108.md).

Owner look filed, not waited on: session `5f049205ae4d7f19c1593afa81ea39df`
(state `/Users/jaeger/.attended-review-state/local-draft-entry-v001-2026-11-11/state`),
frozen root `/Users/jaeger/local-draft-v001.h8pe0ibf` from `3585dedc7f50`;
candidates `BPR-2021-draft-page` and `BPR-2021-home-author-entry`.

## Owner verdicts (attended session 5f049205ae4d7f19c1593afa81ea39df, handed back 2026-11-12T06:21:00Z)

- `BPR-2021-draft-page`: **Accept** (decision `6974c627-dede-4ab8-9c5a-5ef4b1f554a6`, 2026-11-12T06:17:03Z, actor human), no note
- `BPR-2021-home-author-entry`: **Reject** (decision `6f6e8d54-e3f0-4f34-9c4a-b3491e9643f0`, 2026-11-12T06:20:58Z, actor human), the owner: "Seems like a waste of vertical space to have it like this."

Root `/Users/jaeger/local-draft-v001.h8pe0ibf` removed with `freeze_attended_build.py remove` and the host
stopped (2026-11-12). The coordinator: a redesign from mockups comes first;
no new look is filed until then.
