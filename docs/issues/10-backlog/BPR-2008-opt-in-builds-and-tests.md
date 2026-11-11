---
schema: "implement-rfc.card.v1"
id: "BPR-2008"
title: "Let an agent build and run tests in its disposable checkout when asked to"
status: "Backlog"
rfcs: ["RFC-BPR-0104"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0020-opt-in-builds-and-tests.md"}
depends_on: ["BPR-2007"]
---

# Let an agent build and run tests in its disposable checkout when asked to

## Scope and acceptance

Implements
[RFC-BPR-0104](../../rfcs/RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md).

Add the per-agent Build and run tests setting, available only for the
disposable checkout, with the once-per-pull-request question, and pass the
build allowance only after both.

Covers AC04 and the review in AC07. The owner settled on 2026-11-10 that
builds belong in this release, with these limits. Building runs the pull
request's own code: give the allowance critical review.

This is part of `agent-scratch` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Implementation record (2026-11-11, `882ebed54`)

Built in one candidate with [[BPR-2007]] under that card's claim; this card
could not be readied while BPR-2007 was not Done. Ledger:
[RFC-BPR-0104](../../verification/RFC-BPR-0104.md).

- AC04: pass at the unit, host and storage layers: off until turned on; in
  the reviewer's own checkout it is unavailable with its reason and the
  launch carries nothing; in the disposable checkout the next question asks
  once per pull request ("Building runs this pull request's code on this
  computer with your account's rights. Allow for this pull request?"), and
  declining leaves the launch without the build allowance. The question's
  composed path has no course.
- AC07: pending. An independent critical review of both agents' launch
  allowances is required; the implementer's self-review in the ledger is
  input to it, not it.

## Owner look (filed, not waited on)

Session `e97c6a3d72ac570115fc961555b09d6d`, state
`/Users/jaeger/.attended-review-state/agent-scratch-v001-2026-11-11/state`
(URL in its `serve.log`), cursor 12, host PID 72034 (Tailscale child 72042).
Frozen root `/Users/jaeger/agent-scratch-v001.weg1sgre` from `c19749f58892`
(app SHA-256 `cb283436e169…`, launcher check passed). Candidates, ready and
protected: `BPR-2007-grant-menu`, `BPR-2007-agent-sheet`,
`BPR-2008-build-question`, `RFC-BPR-0104-AC03-bound`.

Stays in Backlog: it cannot be readied, claimed or parked while BPR-2007 is not
Done. Next: after BPR-2007 finishes, ready and claim it, and finish it when
AC07's independent review and the `BPR-2008-build-question` look are in.

## Owner hand-back and post-review round (2026-11-11)

`BPR-2008-build-question`: Accept (`df4d82a8`, session `e97c6a3d…`; host
stopped, root removed). The grant menu's rejection (`5a9f122e`) asked for
"Allow build and run tests" as a toggle like the scratch folder's: it is now a
per-pull-request toggle in the footer picker's menu; checking it is the pull
request's answer to the build question, and the agent's setting decides only
whether an unanswered pull request asks first (`cf8a9c12d`; RFC-BPR-0104
amendment). The critical review's finding 6 (Claude's build rules are not
confined) is disclosed in the toggle's tooltip; finding 7 (the modal question)
is fixed. A re-review of the fixes is owed. Still Backlog until BPR-2007 is
Done.

Second post-review round (2026-11-11): the re-review's N5 is fixed: Claude's
parent-step denial names the scratch runners only, so a builds-only grant (or
builds with the folder) no longer denies `cmake ../..` (reproduced live, then
probed on the new rules); Codex in the reviewer's own checkout is given no
grant at all (N1a). Dispositions in the
[ledger](../../verification/RFC-BPR-0104.md#re-review-of-the-fixes-2026-11-11).
Another independent re-review is owed. Still Backlog until BPR-2007 is Done.

Third review round (2026-11-11): F1, a Codex build turn trusted the pull
request's checkout and ran its `.codex/config.toml` MCP servers outside the
sandbox, is fixed by naming the checkout an untrusted project on every launch
there; a build grant now launches only with that checkout named. The builds
tooltip's claim is true with it, and the live course passes for builds on
both agents ([ledger](../../verification/RFC-BPR-0104.md#third-review-round-2026-11-11)).
Another independent re-review is owed. Still Backlog until BPR-2007 is Done.
