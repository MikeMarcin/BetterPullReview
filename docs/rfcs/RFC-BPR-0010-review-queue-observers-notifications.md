---
id: "RFC-BPR-0010"
title: "Review Queue, Observer Rules, and Local Notifications"
status: "Proposed"
area: "BPR"
milestone: "M4 Review Discovery"
depends_on: ["RFC-BPR-0007","RFC-BPR-0008"]
pack_version: 2
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
- Author/path/repository rules enroll matching PRs without changing GitHub assignments.
- Rule exclusions handle generated/vendor paths.
- Each queue item explains its bucket and observer reason.
- Background refresh respects ETags, rate limits, offline state, cancellation, and host/account isolation.
- Notifications contain no source or comment bodies by default and deep-link to the correct local review.
- Closing, reopening, transferring, or losing permission to a PR updates the queue predictably.
- Rule/attention decisions are replay-tested without clocks/network/Qt; host timer/provider tests and Qt incremental queue tests prove the architecture path.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Observer is local only | GitHub plus queue fixture | Match rule and compare server reviewer/subscription state |
| Attention reason is correct | Queue projection | Feed known activity and inspect bucket explanation |
| Rate limiting is honored | Scheduler contract | Simulate primary/secondary limits and inspect backoff |
| Notification privacy holds | Native notification harness | Assert metadata-only content and correct deep link |
| Queue decisions are clock/network independent | Replay + manual scheduler | Same observations produce same buckets; host owns refresh/notification effects |

## Documentation impact

Role semantics, observer grammar, persistence, refresh schedule, rate limits, notification privacy, offline behavior, and queue explanations are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
