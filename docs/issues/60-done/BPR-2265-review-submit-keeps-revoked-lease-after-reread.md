---
schema: "implement-rfc.card.v1"
id: "BPR-2265"
title: "After a pull request reread, Submit review, tour Publish and reply Send keep a factory holding the revoked mutation lease, so every attempt is refused locally until the app restarts"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2265--BPR-2265--8e3ff63766ac.json","amends":"BPR-2263","found_by":"owner, 2026-11-18, build 68 on macOS: an Approve review with one or two pending line comments failed on every Submit and Retry, twice, each time about five minutes after a pull request reread. The page said 'Review was not published. Your summary and its pending comments are kept; check them and submit again.' Root cause traced from source","related_cards":["BPR-2263","BPR-1222","BPR-1540","BPR-1345"],"related_rfcs":[]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "72afb207d", "evidence": {"kind": "command", "detail": "application_review_submission_composition_public_path: two real rereads (window_read_progress snapshot changes, head kept) then one Approve reaches the server, with no refused_not_published logged; application_review_tour_publication_maintenance and _manual: two rereads then the published reorder's Publish completes; application_conversation_read_public_path: two rereads then the new comment's Send posts once; application_inline_publish_cli_consent published: two rereads then inline Publish unchanged"}}, {"criterion": "AC2", "outcome": "pass", "revision": "72afb207d", "evidence": {"kind": "command", "detail": "No factory captures a lease: window_review_drafts.cpp records WindowRead->mutation_transport() in application::publication_lease on composition and on repair, and every submission, tour and reply factory reads it through a weak reference at each call. application_review_submission_application_host test_publication_lease: after a revoke and renewal (twice) the next read gives the newest lease, a revoked lease is never read, and a factory outliving its window reads none. Same-binding adopt also renews the review, tour and reply hosts"}}, {"criterion": "AC3", "outcome": "pass", "revision": "72afb207d", "evidence": {"kind": "command", "detail": "A missing or inactive lease answers unavailable; perform logs provider_unavailable for every operation (test_pre_send_refusals: a discovery whose provider cannot be built is logged with its reason); core_review_submission: provider_unavailable and connection_lease_inactive say 'Not published: the publishing connection was replaced while this review was open. Your review is kept.', never check them"}}, {"criterion": "AC4", "outcome": "pass", "revision": "72afb207d", "evidence": {"kind": "command", "detail": "The BPR-2263 repair asks the window for review_submission_factory(binding), which records the window's current lease before building the factory; composition course: a refused provider is repaired and published from one Submit"}}, {"criterion": "AC5", "outcome": "pass", "revision": "72afb207d", "evidence": {"kind": "command", "detail": "Passed in a clean checkout of the lane (dev preset, macOS, offscreen): architecture_boundaries, core_review_submission, core_operation_diagnosis, application_review_submission_application_host, live_review_submission_provider, application_review_submission_composition_public_path, application_review_submission_cli_ordinary, application_inline_publish_cli_consent, application_conversation_read_public_path, application_review_tour_publication_native, _maintenance, _manual, quill_logging_content_policy; format clean; verify_documentation DOCUMENTATION_OK"}}]
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

## Result

### Built

- `application::publication_lease` holds the window's publishing lease as
  the decision lane last read it. `current()` gives only an active lease,
  and `read(weak)` gives none once the window is gone.
- `review_submission_factory(binding)` records
  `WindowRead->mutation_transport()` in that holder, then builds a factory
  that reads it at each call. The tests-enabled fixture factories read it too
  when the window has a lease. No submission, tour or reply factory captures a
  lease, and a missing or revoked one is `unavailable`.
- Defence in depth: `adopt` for the same binding and authority renews the
  review, tour and reply hosts with the newest factory, without retiring them.
- Corrections to [[BPR-2263]]: the repair goes through
  `review_submission_factory`, so it takes the window's current lease.
  `provider_unavailable` and `connection_lease_inactive` show the
  replaced-connection sentence. `perform` logs a provider it cannot build for
  every operation. That card's Out of scope now names this cause.

### Tests and their limits

- The fixture windows hold no publishing lease (`mutation_transport()` is
  null there). The reread courses therefore show that Submit, tour Publish,
  reply Send and inline Publish work after two real rereads, but they cannot
  fail on the unfixed source. The holder's unit test covers the lease itself.
- The composition course's reread step submits a summary Approve, not one
  with a line comment. With a line comment written before the rereads,
  Submit stopped at "a pending comment changed or could not be held for this
  review" in the fixture. That is the comment hold, not the lease, and it is
  worth a look on its own (see the report).

