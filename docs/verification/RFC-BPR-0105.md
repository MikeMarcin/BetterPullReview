# RFC-BPR-0105 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `dependable-session`
(BPR-2010, BPR-2011). RFC_BASE_SHA: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.
Frontend-design was loaded before the Qt surface work. Debug timings are
nongating; nothing below is a timing claim.

The public-path course is `application_dependable_session_public_path`
([source](../../tests/end_to_end/dependable_session_public_path_tests.cpp)):
the composed desktop, a profile window, two pull requests opened from Home,
the real review page and its buttons, the fixture GitHub server and the
normal diagnostic log. Each step prints what the review page shows
(`dependable-session:observe`).

## AC01: the stall reproduced

The course was built and run against the baseline with only the test and two
fixture holds added (a detached checkout of `dc5453afa`). Excerpt of its
observations, unedited apart from line cuts:

```text
step=first-ready         draft=drafting   approve_enabled=1
  (the shared publication lane is now held by an earlier effect)
step=second-opened       draft=none       approve_enabled=0 status="Checking for an unfinished review…"
step=second-after-wait   at_ms=10000      draft=none approve_enabled=0 status="Checking for an unfinished review…"
step=second-after-release draft=drafting  approve_enabled=1
  (#18 ready; the lane is taken again, then Approve)
step=held-approve-pressed draft=submitting waiting="Waiting for GitHub to confirm this review…"
step=held-after-wait     at_ms=10003      draft=submitting waiting="Waiting for GitHub to confirm this review…"
step=held-after-release  draft=drafting   approve_enabled=1
```

Both waits last exactly as long as the earlier effect and end the moment it
does; no log line names them. The RFC's cause section was amended with this
record ([Found cause](../rfcs/RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md#found-cause-bpr-2010-2026-11-10)).
The owner's own session was not logged, so the effect that held the lane then
is unknown. A diagnosis run also found that, in this desktop fixture, an
approval followed by "Finishing a review" returning Home closes the review
and invalidates its authority; that is the configured behaviour, identical at
the baseline, not a defect.

## Results at the candidate

| Criterion | Check | Result |
|---|---|---|
| AC01 | Baseline course above; RFC cause amended | Reproduced and recorded |
| AC02 | Course steps 1 and 2: the shared publication lane held, and #17's own submission held on its lane while the reviewer leaves it; #18's Approve is offered at once and publishes in both | Pass |
| AC03 | Course step 3 (bound shortened to 1.5 s by the course): Waiting with "Waiting for …", then at the bound "Not published. Still waiting for … Your review is kept." with Retry, summary and verdict intact, nothing sent; Retry publishes once released. The 20-second constant is `core::review_submission_wait_bound` | Pass |
| AC04 | Course step 4: the window's authority changes while a submission waits; its answer from the retired host ends the page as not published ("Publication cancelled. Your draft is kept."), never left submitting | Pass |
| AC05 | Course step 5 reads the normal diagnostic log: `review_submit_request`, `review_submit_wait` with `submit_subject` and `duration_ms`, `review_submit_send`, `review_submit_drop submit_subject=retired`, `review_submit_end` with `published` and `wait_expired` | Pass |
| AC06 | Course step 6: #17 left on the Tour tab with the tour side chat open; #18 opens on Comments with no side chat and no tour page; the same again when #18 is reopened from Home | Pass from Home (side chat opened through its own widget entry point, not an agent run). Links and restored reviews take the same signalled path; a notification's open was not exercised |
| AC07 | Course step 6: `view_filters()` and the Comments drawer width are equal before and after | Pass |
| AC08 | Owner's approval on BPR-Test from a frozen build after five or more pull requests with agent and tour use | Pending: owner-attended |

Focused unit evidence: `core_operation_diagnosis` (new submission lines and
their refusals), `application_review_submission_application_host` (abandon
before start, start before terminal), `qt_review_submission_editor_scale_*`
(waiting, sending and not-published states, offscreen renders in light and
dark). Existing whole-review and tour publication courses were rerun; two
waits in `review_submission_composition_public_path` now wait for the start
and the terminal, since a held decision lane now also holds the start.

Integration runs. A full `ctest --preset dev` at `357df825d` (stopped at
761/950 when the open signal was corrected) and a 67-row focused pass at the
candidate (every whole-review, tour publication, agent invocation and chat,
native agent session, Home, composed-window and Qt window row, and the
composition source contract): 66/67 pass. Recorded failures, none changed by
this milestone:

- `application_agent_chat_custom_agent_contract`
  (`custom-agent-empty-refusal-said`) fails the same with the open reset
  disabled; this milestone touches no Agent-tab send path.
- `qt_profile_management_window` expects 28 View actions; the test was last
  changed on 2026-11-04 and RFC-BPR-0097 added Go to line to View on
  2026-11-08. This milestone changes no menu.
- `application_large_review_*` Debug budget rows exceeded their budgets;
  Debug timings are nongating and these are not counted as passes.

`application_composition_source_contract` was resealed for the reviewed
production app sources, and `test_network_isolation_manifest` received the new
course's unmeasured Windows pin.

No agent live course was run for AC08: there is no composed-application live
submission harness, and the provider path that writes to GitHub is unchanged.
The test account opened
[BPR-Test #151](https://github.com/MikeMarcin/BPR-Test/pull/151) so the
owner, its reviewer, can approve a pull request they did not author.
