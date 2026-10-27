---
id: "RFC-BPR-0010"
title: "Review Queue, Observer Rules, and Local Notifications"
status: "Auto-Verified"
area: "BPR"
milestone: "M4 Review Discovery"
depends_on: ["RFC-BPR-0007","RFC-BPR-0008"]
pack_version: 2
metadata: {"milestone_plan":{"id":"rfc-bpr-0010-owner-closeout","path":"docs/rfcs/plans/rfc-bpr-0010-owner-closeout/milestone-plan.json"}}
---

# RFC-BPR-0010: Review Queue, Observer Rules, and Local Notifications

## Summary

Provide the reviewer/author/observer landing page, person and path observer rules, attention buckets, background refresh, and local notifications without changing GitHub reviewer assignments.


## Motivation

A reviewer needs a landing page that answers what requires attention across reviews, including PRs observed because of authors or paths, without asking the organization to change reviewer assignment or subscription culture.

## Goals

- Build a landing page for PRs where the user is author, reviewer, requested reviewer, participant, or local observer.
- Add local observer rules for repository, author, and path patterns.
- Derive attention buckets from GitHub activity and local review state.
- Add bounded background refresh and native local notifications.
- Explain why a PR appears in the queue.

## Non-goals

- Modifying GitHub reviewer assignments, watches, CODEOWNERS, labels, or subscriptions.
- Replacing all GitHub notification features.
- Running a hosted synchronization service.

## Architectural allocation

- **Core:** owns deterministic observer-rule matching, enrollment/retention policy, attention bucket derivation, and semantic queue navigation from bounded GitHub/local observations.
- **Host:** owns clocks/timers, periodic refresh, provider requests, queue/cache persistence, backoff, cancellation, and execution of semantic native-notification requests. The core never reads the wall clock or schedules a timer.
- **Qt adapter:** owns queue models, filters, sorting presentation, status/empty states, and native notification presentation. Models update incrementally by stable PR identity.
- Background refresh completions carry host/account/repository/PR/generation identity and cannot update a different active session.

## Queue buckets

- Initial review requested
- Changed since my review
- New comments or author replies
- Fixes awaiting verification
- Authored reviews awaiting reviewer action
- Observed with new activity
- Quiet/current

Observer enrollment persists locally until closure or manual removal even if the matching path later disappears. Each observed item records the rule that matched.

## Refresh

A local scheduler uses conditional requests, host rate-limit state, bounded concurrency, backoff, and cancellation. It runs only while the desktop user session permits and has no hidden system service requirement in the initial implementation.

## Acceptance criteria

- Queue role classification matches sanitized GitHub fixtures.
- Author/path/repository rules enroll matching PRs through local-only composition with no provider mutation capability.
- Rule exclusions handle generated/vendor paths.
- Each queue item explains its bucket and observer reason.
- Background refresh respects ETags, rate limits, offline state, cancellation, and host/account isolation.
- Notifications contain no source or comment bodies by default and composed activation selects the correct local review.
- Closing, reopening, transferring, or losing permission to a PR updates the queue predictably.
- Rule/attention decisions are replay-tested without clocks/network/Qt; host timer/provider tests and Qt incremental queue tests prove the architecture path.

## Owner-amended closeout r4 — 2026-09-21

The owner's direction to unblock and land RFC-BPR-0014 explicitly authorizes
this bounded closeout after RFC-BPR-0009. The
[adopted plan](plans/rfc-bpr-0010-owner-closeout/milestone-plan.json) and
[mandate](plans/rfc-bpr-0010-owner-closeout/mandate-r4.md) preserve the original
pack source and distinguish the implemented scope from unfinished observations.

The original requirement to compare GitHub's reviewer, requested-reviewer,
subscription and label state through an independent client remains unobserved
on standalone [BPR-0169](../issues/60-done/BPR-0169-rfc-0010-observer-rules-leave-github-untouched.md).
The local enrollment path remains incapable of provider mutation; its fixtures
do not establish that live server comparison. BPR-0169's identity, dependency,
missing safe target and read-only permission limits are preserved.

The owner accepted native banner text, privacy, a real click and exact focused
selection at BPR-0168's v006 executable. That exact checkpoint and the later
interim replay waiver remain valid. Standalone
[BPR-0168](../issues/20-ready/BPR-0168-rfc-0010-native-notification-delivery.md)
retains terminal candidate identity replay and any necessary successor native
verdict after BPR-0169. Neither a current banner observation nor another
platform's native behavior is inferred from the automated composition check.

These two cards remain open outside the amended terminal gate. BPR-0244 uses
focused current verification and relevant unchanged evidence rather than
requiring replay after unrelated HEAD movement. This acceptance amendment
changes no product behavior, no permission boundary and no release acceptance;
the [ledger](../verification/RFC-BPR-0010.md) records exact evidence and limits.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Observer enrollment is locally composed | Core plus queue composition fixture | Match rules, persist and restore exact reasons, remove a rule, and require zero provider mutations |
| Independent server non-mutation observation | Standalone BPR-0169, still pending | Match each rule and compare reviewer/requested-reviewer/subscription/label state through an independent client |
| Attention reason is correct | Queue projection | Feed known activity and inspect bucket explanation |
| Rate limiting is honored | Scheduler contract | Simulate primary/secondary limits and inspect backoff |
| Notification privacy and local activation hold | Composed system-sink course plus exact accepted v006 checkpoint | Assert metadata-only content and exact local selection; preserve later-candidate native replay on standalone BPR-0168 |
| Queue decisions are clock/network independent | Replay + manual scheduler | Same observations produce same buckets; host owns refresh/notification effects |

## Documentation impact

Role semantics, observer grammar, persistence, refresh schedule, rate limits, notification privacy, offline behavior, and queue explanations are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.

## Home waiting successor (RFC-BPR-0070)

[RFC-BPR-0070](RFC-BPR-0070-preserve-home-waiting.md) corrects the observations
consumed by Home: exact enrolled authorship, preserved pending requests, unmet
server review requirements and concurrent check/merge blockers. Authored waiting
placement and unread precedence remain as settled in RFC0066. Known blockers stay
visible under Hide quiet; bounded names, separate people/team counts and last-known
freshness extend existing native row anatomy. This successor adds bounded provider
reads and numeric-account-bound metadata retention; historical section rearrangement
and its evidence are unchanged. Historical human/native obligations are not waived.
