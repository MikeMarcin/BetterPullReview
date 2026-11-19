---
schema: "implement-rfc.card.v1"
id: "BPR-2265"
title: "After a pull request reread, Submit review, tour Publish and reply Send keep a factory holding the revoked mutation lease, so every attempt is refused locally until the app restarts"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2265--BPR-2265--8e3ff63766ac.json","amends":"BPR-2263","found_by":"owner, 2026-11-18, build 68 on macOS: an Approve review with one or two pending line comments failed on every Submit and Retry, twice, each time about five minutes after a pull request reread. The page said 'Review was not published. Your summary and its pending comments are kept; check them and submit again.' Root cause traced from source","related_cards":["BPR-2263","BPR-1222","BPR-1540","BPR-1345"],"related_rfcs":[]}
acceptance: []
---

# BPR-2265: After a pull request reread, Submit review, tour Publish and reply Send keep a factory holding the revoked mutation lease, so every attempt is refused locally until the app restarts

This card amends [[BPR-2263]] (the "diagnostics card" below): it names the
root cause that card left out of scope, and corrects its repair step. Land
this card first, or with that one.

## Observed

The owner, 2026-11-18, build 68 on macOS, on a GitHub Enterprise Server host.
The acting account was not the pull request's author, and the review's head
matched the pull request's head on GitHub. It happened twice:

- **First:** Approve with one pending comment. Four Submits at 14:54:35,
  14:54:55, 14:55:56 and 14:56:10. The pull request had been reread at
  14:49:53.
- **Second:** Approve with an empty summary and two pending comments. Submit
  and Retry at 16:15:26 and after. The pull request had been reread at
  16:10:18.

Every attempt logged the same events: `review_submit_request`,
`review_submit_wait` (`pending_comments`), `storage_migrate`
(`review_mutation_recovery`), `review_submit_wait` (`review_lane`),
`review_submit_send`, then `review_submit_end` with
`failure=refused_not_published`, 1 ms after send. None had:

- a `github.*` request;
- a `review_submission` ending event;
- a new `submission_attempt` row in the recovery store.

The page said:

```text
Review was not published. Your summary and its pending comments are kept; check them and submit again.
```

Retry failed the same way.

## Cause

1. **A reread revokes the lease.** Accepting a snapshot calls
   `renew_review_mutation_transport()`
   (`src/application/src/github_window_read_host.cpp`, around line 689). That
   revokes the window's review mutation lease and acquires a new one (around
   lines 627–642).
2. **The window offers a fresh factory.** The window composes review
   submission again for the new handoff. `live_factory`
   (`src/app/src/github_application/window_review_drafts.cpp`, around lines
   82–98) builds a factory that captures `WindowRead->mutation_transport()`,
   the new lease, by value.
3. **The workflow keeps its old factory.**
   `review_submission_workflow::adopt`
   (`src/app/src/review_submission_workflow.cpp`, around lines 191–196) clears
   `Snapshot` and `PullRequestGeneration` from the authority. It then returns
   early when `Binding` and `Authority` are unchanged, which they are after a
   reread that keeps the head, account and pull request. The new factory is
   dropped. `Current->Factory`, and the `Host` built from it (around line
   258), still capture the revoked lease.
4. **Submit is refused before any request.**
   `review_submission_application_host::perform`
   (`src/application/src/review_submission_application_host.cpp`, around line
   332) calls `Factory()`. The factory opens the recovery store first, which
   is the logged `storage_migrate`. It then calls
   `make_live_review_submission_provider`. There `admit_review_mutation_provider`
   finds `!aLease->active()` and returns `unavailable`
   (`src/adapters/github/src/review_mutation_provider_admission.cpp`, around
   lines 14–15). `refused(unavailable, true)` answers
   `definitely_not_published`. No session is built, so `log_ending` writes
   nothing.
5. **The page blames the review.** `github_cli_publication_failure_text`
   (`src/adapters/qt/src/github_cli_consent.cpp`, around line 23) has no text
   for `unavailable`. The generic sentence stays, telling the reviewer to
   check comments that are fine. Retry goes through the same
   `Current->Factory`, so it fails the same way. A further Refresh revokes the
   lease again and still doesn't replace the factory. Only a restart, or a
   change of sign-in, which retires the host, recovers.
6. **Tours and replies share the defect.**
   `review_tour_publication_workflow::adopt` (around line 1099) and
   `conversation_reply_workflow::adopt` (around line 1581) have the same early
   return. They are given the same captured-lease factory
   (`window_review_drafts.cpp`, around lines 160–163). After a reread, tour
   **Publish** and reply **Send** should be refused the same way. Inline
   comment publishing is not affected: it reads
   `WindowRead->mutation_transport()` at each call
   (`window_review_mutations.cpp`, around line 73).

## Change

### 1. A factory never captures a lease

- `live_factory` captures what it needs to read the window's current lease,
  not the lease itself. Each `Factory()` call takes
  `WindowRead->mutation_transport()` at that moment, as inline publishing
  already does. If there is no active lease, it answers `unavailable` with the
  reason "no publishing connection" (diagnostics card, Change 1).
- Keep the read on the factory's own lane, and keep it as strict as today's
  admission: the lease must still match the window's account and host.
  Capturing the window's read host must not extend its lifetime past the
  window. Use the same owner guard the factory's other captures use.
- With that, the early returns in the three `adopt` functions stay correct:
  the same binding and authority really mean the same publication target, and
  a reread no longer has to rebuild the host or rerun discovery.

### 2. Defence in depth: a refreshed lease reaches every workflow

- Where the window composes the three workflows after a reread, also hand
  them the new factory on the early-return path (`replace_factory`, without
  `retire()`), so no workflow can hold a factory from before the reread. If
  Change 1 makes the factory lease-free, this is a cheap guard, not a second
  mechanism. Drop it if review prefers a single mechanism.

### 3. Corrections to the diagnostics card

- **Its Change 3 (repair, then retry once).** For `provider_unavailable` and
  `connection_lease_inactive`, the repair must take the window's current lease
  (`WindowRead->mutation_transport()`), not rebuild from
  `Current->Factory`. Rebuilding from the stored factory would capture the
  same revoked lease and fail again. With Change 1 here, a refused attempt
  after a reread can't happen in the first place. The repair stays for a lease
  revoked mid-attempt.
- **Its Change 2 (one sentence per reason).** `unavailable` from provider
  admission says: "Not published: the publishing connection was replaced
  while this review was open. Your review is kept." Say it once, and only if
  the automatic repair also failed.
- **Its Out of scope.** Replace "which check refused this particular review
  … the evidence doesn't say" with: "Known: provider admission refused a
  revoked lease (this card)."
- **Its Change 1 (logging).** `perform` logs factory failures for every
  operation, not only tours (`record_tour_publication`, around lines 329–335,
  is the only logging there today). Then a revoked-lease refusal shows in the
  log as `provider_unavailable` / `connection_lease_inactive`.

### 4. Tests

- App composition (fixture course): open a pull request, accept a reread
  that keeps the head, account and pull request, then **Submit** an Approve
  with a line comment. It reaches the transport and publishes. The log has no
  `refused_not_published`.
- The same course for tour **Publish** and reply **Send** after a reread.
- Two rereads in a row, then Submit: still publishes.
- Application or app: a factory called after its window's lease was renewed
  uses the new lease. A factory called with no active lease answers
  `unavailable` with its reason, logged.
- Regression for inline publishing: unchanged after a reread.

## Acceptance

- AC1. After any number of accepted rereads that keep the head, account and
  pull request, Submit review, tour Publish and reply Send reach GitHub with
  the window's current lease, with no restart and no second press.
- AC2. No review submission, tour or reply factory holds a lease past the
  window's renewal of it.
- AC3. A submission refused for a missing or inactive lease is logged with
  that reason, and the page says so in the reason's own sentence, never "check
  them and submit again".
- AC4. The diagnostics card's repair step, where it applies, takes the
  window's current lease.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Whether a reread should renew the mutation lease at all. This card keeps
  renewal and makes every consumer follow it.
- Everything else in the diagnostics card: typed reasons for the other
  pre-send refusals, and the stale-head confirmation. It stays as written,
  apart from section 3 here.

## Workaround until this lands

Quit and relaunch the app, open the pull request, and submit before it
rereads. Signing out and in again also works. Refresh does not: it revokes
the lease again. Pending comments are stored locally and survive a relaunch.
