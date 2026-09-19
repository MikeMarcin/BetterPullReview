# Attended Recent Resume evidence

Run `report-followups-run-20260919`, dispatch
`report-followups-dispatch-001`, producer `/root/report_followups`, mandate
revision 1. The accepted integrated source candidate is
`4e409bc967a9814e5a492f3eaf644254352e1df8`; the fixture implementation commit
is `e73831f5882d944a5572f26089f2fd15888c538e`.

## Diagnosed cause and implemented behavior

The capture came from the tests-enabled attended UIforETW fixture under
signed-out public-preview authority. Initial load consumed the strict
anonymous recording. Recent retained the exact public mode, host, destination
and saved position and correctly dispatched another open, but the exhausted
recording rejected its next `get_repository`; the fixture normalized that
strict contract failure to the displayed malformed-response message.

Attended launch now enables repeat read sessions without enabling thread
resolution. The strict fixture rewinds anonymous or account read cursors only
at `get_repository`; bootstrap remains one-shot. Request admission after rewind
still checks operation, method, path, query, headers, body and semantic target.
Production Recent persistence, restoration authority, provider normalization,
credentials and publication paths were not changed.

The new isolated course uses the actual UIforETW recording with independent
window reads disabled. Public and account courses each open the PR, select the
exact `UIforETW/Resource.h` comparison and line, return Home, and Resume twice.
Each Resume accepts a fresh snapshot while preserving mode/profile/host/account,
repository, pull-request number, comparison key and selection. Public preview
uses no vault or account transport. Negative requests alter method, path,
query, headers, body, operation, owner, repository and number and remain
malformed; bootstrap cannot replay a read session.

## Verification and review

`application_attended_recent_replay` passed repeatedly (44.19 seconds in the
integrator rerun). At corrected candidate `4e409bc9`, the 16-test integration
selector passed in 97.67 seconds, including the new course,
`application_anonymous_public_preview_public_path`, its GHES refusal,
`application_github_public_path_uiforetw_74`, and the core/application/SQLite
restoration contracts. Format and whitespace gates passed; all commands used
recorded/synthetic data and contacted no live service.

Critical review found no blocker. It confirmed that repeat behavior is under
the tests-enabled fixture graph, `ResolvesThreads` remains exclusive to the
threaded-reply probe, bootstrap is excluded, and the strict wire/semantic
matcher remains authoritative.

## Explicit limitations and unrelated failures

The focused harness sets the fixture option directly; `main.cpp`'s five-line
`--attended-launch` binding is verified by direct source/build inspection, not
a subprocess flag test. The account recording already reports one unsupported
timeline operation, so its scalar failure-code assertion proves no different
code was introduced but cannot count a second identical code; its fresh exact
snapshots and restored selections provide independent positive evidence. The
signed-out course—the reported failure—starts at no recording failure and
remains there.

Three broader selectors failed outside the changed acceptance boundary:

- `application_uiforetw_selected_diff_contract` reached all exact
  commit/resource/RC/gesture/role/trace markers, then failed its capture
  geometry expectation at `985:835`.
- `application_threaded_reply_contract` timed out at its phase-one draft state
  before publication or replay.
- `application_profile_window_review_restoration_public_path` failed the
  queued notice-order assertion in `auth-reading-gesture`. A bounded A/B run
  reproduced the identical assertion after restoring the original account-only
  rewind condition. Logs are
  `build/bpr0932-current-restoration-suite.log` and
  `build/bpr0932-baseline-restoration.log` (local, uncommitted).

Those failures are not claimed as passing evidence and were not changed. The
new focused Resume course plus passing UIforETW, anonymous and restoration
contracts cover this card's exact fixture lifecycle. No live GitHub/GHES,
credential, publication or BPR-0797 protected-candidate operation occurred.

