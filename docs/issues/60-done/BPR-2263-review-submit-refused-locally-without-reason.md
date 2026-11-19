---
schema: "implement-rfc.card.v1"
id: "BPR-2263"
title: "Submit review fails locally in about a millisecond, four times in a row, and neither the log nor the page says why or what to do, because pre-send refusals carry no reason past the host"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2263--BPR-2263--ca1e2359b203.json","found_by":"owner, 2026-11-18, build 68 on macOS: an Approve review with one pending line comment, from Compose review, failed to publish on four Submit presses over 95 s. Each ended about 1 ms after sending started, with no GitHub request. The log records only refused_not_published, and the owner could not tell from the page how to fix it","related_cards":["BPR-1345","BPR-1541","BPR-1572","BPR-1715"],"related_rfcs":["RFC-BPR-0105"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "6ca57571d", "evidence": {"kind": "command", "detail": "core_operation_diagnosis and core_review_submission: a review_submit_end line carries failure=<its own failure> submit_check submit_reason submit_part submit_comment, and the review_submission_ending line check= reason= part= comment=, both reading back; application_review_submission_application_host test_pre_send_refusals: provider unavailable, grant inactive and a stopped request each answer definitely_not_published with their own host reason and are logged as the run's ending; live_review_submission_provider early_refusals_are_named_and_logged: account and pull request mismatch, a blank second comment and a missing summary each give their own session reason (comment index named) with no request sent and one ending event; application_review_submission_composition_public_path reads check=host reason=provider_unavailable from the diagnostics log"}}, {"criterion": "AC2", "outcome": "pass", "revision": "6ca57571d", "evidence": {"kind": "command", "detail": "core_review_submission test_refusals: every reason's sentence starts Not published and none says check them; an invalid comment is named path:line; an unexplained refusal points to Copy diagnostics; composition course: a moved head shows the new-commits sentence, not check them"}}, {"criterion": "AC3", "outcome": "pass", "revision": "6ca57571d", "evidence": {"kind": "command", "detail": "application_review_submission_composition_public_path: Approve with the submission provider refused once is repaired and published from one Submit, the server holding exactly one new review; the log shows repairing (check=host reason=provider_unavailable) and repaired. With the repair disabled the course fails at no receipt, the page saying the connection was not ready"}}, {"criterion": "AC4", "outcome": "pass", "revision": "6ca57571d", "evidence": {"kind": "command", "detail": "core_review_submission: stale_head and target_moved decide reread_and_confirm, never reconnect_and_retry; composition course: a head moved before sending ends drafting with nothing published, and only the reviewer's next Submit publishes it"}}, {"criterion": "AC5", "outcome": "pass", "revision": "6ca57571d", "evidence": {"kind": "command", "detail": "Passed on the lane (dev preset, macOS, offscreen): architecture_boundaries, core_review_submission, core_operation_diagnosis, application_review_submission_application_host, application_review_submission_contract, application_review_mutation_application_host, live_review_submission_provider, live_review_submission_restart_recovery, application_review_submission_composition_public_path, application_review_submission_cli_ordinary, application_inline_publish_cli_consent, quill_logging, quill_logging_content_policy, qt_review_submission_editor_scale_100; coding-format-check; clang-tidy clean on the changed lines; verify_documentation DOCUMENTATION_OK"}}]
---

# BPR-2263: Submit review fails locally in about a millisecond, four times in a row, and neither the log nor the page says why or what to do, because pre-send refusals carry no reason past the host

## Observed

The owner, 2026-11-18, build 68 on macOS. In **Compose review** they chose
**Approve**, with a summary and one pending line comment, and pressed
**Submit**. The review was not published. They pressed Submit three more times
over the next 95 s, with the same result. The pull request had last been read
at 14:49:53, five minutes before, and nothing was read or changed in between.
The owner closed the app before anyone recorded the page's status text.

Each of the four attempts logged the same five events, in about 35 ms in all:

| Event | Detail |
|---|---|
| `review_submit_request` | `count=1` |
| `review_submit_wait` | `submit_subject=pending_comments`, about 30 ms: the comment was held |
| `review_submit_wait` | `submit_subject=review_lane`, 2–4 ms |
| `review_submit_send` | the host started the operation |
| `review_submit_end` (failed) | `failure=refused_not_published submit_subject=not_published`, **1 ms after send** |

Nothing else appears:

- no `github.*` request or HTTP status;
- no `review_submission` ending event, which `review_submission_session::log_ending` writes whenever the GitHub session finishes a run;
- no credential, connection or account event.

The review-mutation recovery store holds no attempt from that day. Its newest
`submission_post` is from the day before. The session admits an attempt only
after reading the pull request's reviews from GitHub, so none of the four got
that far.

So the app refused the review itself, the same way four times, before any
request left. Neither the log nor the saved state can say which check did it.

## Cause

### 1. Several checks refuse before sending, and the reason stops at the host

Within 1 ms of `started()`, and with no GitHub request, the review can only be
refused by one of these:

- `review_submission_application_host::perform`
  (`src/application/src/review_submission_application_host.cpp`, around lines
  317–419):
  - a stopped request or inactive grant gives `cancelled`;
  - `Factory()` failing or returning nothing gives `unavailable`, or the
    factory's own error.

  Both go through `refused(…, true)`, which answers
  `definitely_not_published` with a `Failure` (around line 192).
- `review_submission_session::run`
  (`src/adapters/github/src/review_submission_session.cpp`, around lines
  704–760), before `observe_reviews`:
  - an identity mismatch gives `undesignated_target`;
  - `compile_review_submission_request` refusing the request
    (`valid_submission`, `github_mutation_contract_edit_requests.cpp` around
    line 846: summary, comment text or path, commit) gives no failure at all;
  - a missing recovery record, a lost connection lease, or a moved target or
    acting identity (`stale_head`).

Each of these knows its reason. Few carry it on:

- `perform` logs nothing for its own refusals.
- The session's `note(reason)` is kept only for an `indeterminate` result
  (`explain`, around line 190). A `definitely_not_published` result keeps only
  `Failure`.
- `log_ending` folds every `Failure` it doesn't list into `stop::other`. It
  returns early if the session has no `Log`, or if `describe()` gives nothing.
- The workflow's `end_attempt` (`src/app/src/review_submission_workflow.cpp`,
  around lines 124–137) maps every ending but published, expired and unknown
  onto `operation_failure::refused_not_published`. It drops `aResult.Failure`,
  so the log can't tell `cancelled` from `unavailable` from
  `undesignated_target` from an invalid request.
- The main submission host is built without the log
  (`review_submission_workflow.cpp`, around line 258), while the tour host gets
  one (around line 1109). Whether the session that `Current->Factory` makes
  has a log needs checking. If it doesn't, `log_ending` writes nothing for any
  submission.

### 2. The page gives no reason and no fix

For a `definitely_not_published` answer, `review_submission_workflow` (around
lines 881–917) sets:

- `Review was not published. Your summary is retained.`, or
- `Review was not published. Your summary and its pending comments are kept; check them and submit again.`

It then lets `github_cli_publication_failure_text` replace that text. That
function names only `credential_unavailable`, `credential_tool_failed`,
`unsupported` and `cancelled` (`src/adapters/qt/src/github_cli_consent.cpp`,
around line 23). Everything else gets the generic sentence:

- `unavailable`
- `undesignated_target`
- `stale_head`
- `target_moved`
- an invalid request

That sentence tells the reviewer to check their comments, which here was
probably not the problem. A press of Submit then repeats the same refusal,
because nothing about the cause has changed. The retry offer
(`RetryOffered`) is shown either way.

### 3. Nothing repairs what the app could repair itself

Most of these causes are the app's own state, not the reviewer's:

- a stale factory or connection lease;
- a binding that no longer matches after a reread;
- a cancelled grant from an earlier attempt.

The reviewer can't fix any of them by editing the review. The app can,
usually by rebuilding the submission host for the current binding, renewing
the connection, or rereading the pull request.

## Change

### 1. Every refusal names its reason, in the log and in the answer

- Add a typed pre-send refusal reason to `review_submission_result`, and give
  every early return in `perform` and `run` its own value. For example:
  - `request_stopped`
  - `grant_inactive`
  - `provider_unavailable` (with the factory's error)
  - `identity_mismatch_account`
  - `identity_mismatch_pull_request`
  - `invalid_request` (with which part failed: summary, comment body, comment
    path, commit, size)
  - `recovery_record_unavailable`
  - `connection_lease_inactive`
  - `target_moved`
  - `stale_head`

  This is the same idea as the `reason` values `note()` already has, made part
  of every not-published answer.
- `end_attempt` logs that reason and the `review_mutation_failure` it came
  with, instead of folding them into `refused_not_published`. Use
  `submit_subject` or a new field, through the existing diagnostic event
  schema.
- Every review submission host and session gets the diagnostic log, and
  `perform` logs its own refusals. Test that a host built by the window has
  one.
- `log_ending` gives every failure its own `stop` value, and never drops an
  ending silently. If `describe()` gives nothing, log a fixed text.

### 2. The page says what happened and what to do

Map each reason to one sentence, in one place, used by the Compose review page
and by inline publishing. Each sentence says what happened and what the
reviewer can do, for example:

- Invalid request: name the part ("The comment on `path:line` has no
  text.", "The summary is longer than GitHub accepts.").
- Account or pull request changed: say which, and that the draft is kept.
- Connection unavailable: say so, and offer **Reconnect** (or say the app is
  reconnecting; see 3).

Never fall back to "check them and submit again" for a cause that isn't in the
review. A refusal that the page can't explain says so and points to
**Copy diagnostics**, rather than suggesting a retry that will fail.

### 3. Repair what the app can, then retry once

For causes the app owns, repair before asking the reviewer:

| Cause | Repair |
|---|---|
| Grant inactive, provider unavailable, connection lease inactive | Rebuild the submission host and connection for the current binding, then retry the same request once, with the same idempotency key, so a retry can't post twice |
| Binding no longer matches after a reread of the same pull request and account | Rebind to the current authority and retry once |
| Stale head or target moved | Reread the pull request, then ask the reviewer to confirm (the commit under the review changed, so a silent retry would approve code they didn't see) |

Log each repair and its result. If the repair fails, or the retry is refused
for the same reason, show the reason sentence (Change 2) and stop. Never loop.

### 4. Tests

- Application host: each early refusal in `perform` answers
  `definitely_not_published` with its own reason, and logs it.
- GitHub session: each early return in `run` before the first request gives
  its own reason, including each `valid_submission` failure, and `log_ending`
  writes an event for each.
- Workflow: `end_attempt` logs the reason it was given. A host and session
  built by the window have a log.
- Qt: each reason renders its own sentence, and none of the app-owned causes
  renders "check them and submit again".
- App composition (fixture course): Approve with one comment while the
  submission connection is unavailable is repaired and published with no
  second Submit, and the log shows the refusal, the repair and the publish. A
  stale head stops with a confirm prompt and doesn't publish.

## Acceptance

- AC1. Every Submit that ends not published logs the check that refused it and
  the reason, whether or not a request left. The log alone tells apart:
  - the account or pull request changed;
  - the connection was unavailable;
  - the request was cancelled;
  - the request was invalid (and which part);
  - a recovery-record problem;
  - a stale head or moved target.
- AC2. The page names that reason in one sentence, says what the reviewer can
  do, and never blames the review's contents for a cause outside them.
- AC3. For causes the app owns (connection, grant, provider, binding on the
  same pull request and account), the app repairs and retries once, without
  publishing twice, and the reviewer needs no second press.
- AC4. A stale head or moved target never publishes without the reviewer
  confirming.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Known: provider admission refused a revoked lease ([[BPR-2265]]).
- Refusals GitHub itself returns (4xx answers). They already name an
  unplaceable line or an own-pull-request refusal ([[BPR-1715]],
  [[BPR-1853]]). Extending those is separate.
- Check status and restart reconciliation of an unknown outcome
  ([[BPR-1475]], [[BPR-1541]]). Their behaviour is unchanged.

## Result

### Most likely cause of the owner's refusal

The window built the submission's provider factory around the publishing
lease current when the pull request was composed. Every reread installs a new
snapshot and revokes that lease (`renew_review_mutation_transport`). A reread
of the same pull request at the same head leaves the review's binding and
authority unchanged, so `review_submission_workflow::adopt` returned early and
kept the old factory. From then on, every Submit built its provider over the
revoked lease. `admit_review_mutation_provider` refused it as `unavailable`,
and `perform` answered `definitely_not_published` about 1 ms after
`started()`, before any session ran. That explains the four identical
refusals. It also explains why the log held no `review_submission` ending:
the session that has the log never ran, and the host had no log. The
evidence does not prove this. With AC1 in place the next occurrence logs
`check=host reason=provider_unavailable` if it is this cause.

### Built

- Core `review_submission_refusal.hpp`: check (workflow, host, session), 15
  reasons, 12 invalid parts with the comment's index,
  `review_submission_content_refusal`, `decide_review_submission_repair` and
  `sentence` (the one sentence for the Compose review page and, for a moved
  or changed pull request, inline publishing). The operation line gains
  `review_submit_repair` (`repairing`, `repaired`, `repair_failed`) and the
  refusal fields. The ending line gains a cause and a stop for each failure.
- Application: `review_submission_result::Refused`. `perform` names and logs
  its refusals (`record_review_submission_ending`). `renew` swaps the
  factory for later operations.
- GitHub session: every early return in `run` names its reason.
  `diagnose_review_submission_request` names the part GitHub's request
  cannot carry. `log_ending` gives each failure its own stop and never drops
  an ending (a fixed `review_submission_ending=unrecorded` text otherwise).
- [[BPR-2265]] corrections: the repair's factory reads the window's current
  lease (`WindowRead->mutation_transport()`) rather than rebuilding from
  `Current->Factory`, and `perform` logs a provider the factory cannot build
  for every operation.
- Workflow: the main host gets the log. `adopt` for the same binding renews
  the host's factory. A connection, grant, provider or identity refusal on the
  window's own binding is repaired once over a new factory from
  `review_submission_factory` and sent again under the same idempotency key.
  A moved head asks for a reread and is never sent. `end_attempt` logs the
  reason and the failure.

### Decisions for the owner

- Provider unavailable and lease inactive say, as [[BPR-2265]] amends:
  "Not published: the publishing connection was replaced while this review
  was open. Your review is kept." The other connection reasons (grant
  inactive, connection unavailable, host admission refused) share one
  sentence that says reconnecting did not fix it and asks for Refresh, which
  reconnects. The page shows either only after the repair failed. There is no
  separate Reconnect button.
- A moved head or target is never retried. The page says so and the
  reviewer submits again to confirm. Because the review's binding is then
  stale, the existing BPR-1715 rule (Discard starts a new review on the new
  revision) still applies once the reread lands.
- Lease-inactive and recovery-record refusals inside the session stay
  indeterminate (Check status), as before. Only refusals that are certainly
  unsent are repaired.

### Not run, and a known failure

`application_dependable_session_public_path` fails at "the review is
accepted" (step 2, reopening #17). It fails the same way on the unchanged
base (efd43ba73), built separately, so this card did not cause it. The whole
suite was not run.

