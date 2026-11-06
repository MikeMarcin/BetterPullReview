---
schema: "implement-rfc.card.v1"
id: "BPR-1875"
title: "A pull request opened from Home is not cancelled by a discovery refresh"
status: "Parked"
parked_on: "awaiting-live-github"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05, split from BPR-1873","frontend_design":"not loaded: no surface the owner sees changed; diagnostics and a fixture course only"}
acceptance: [{"criterion":"AC1 On fixtures, an open started from Home, then a discovery refresh, completes without cancellation or waiting for the refresh's retained read","outcome":"pass","revision":"f23601054","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_home_open_feedback_public_path|application_home_open_feedback_cli_public_path)$' -V","detail":"The composed application on the fixture network, native (device) account and CLI account. The open's first answers are held 3 s each and the refresh's batched retained read (ListHomePullRequests, new fixture hold RetainedMilliseconds) 15 s. Refresh begun 200 ms after the open: #17 opened in 9.1 s (native) / 9.1 s (CLI); refresh begun 2 s before the open, its retained read on the wire: #4001 opened in 9.1 s / 9.1 s (native: the retained read preempted, preempted=1; CLI: beside it, preempted=0), both with the refresh still running. Three held first answers make the 9 s; the limit is 13 s, under the 15 s hold. No pull_request_cancelled status, no fixture-side cancel, and the diagnostics file names no cancelled read other than the course's own Cancel presses."}},{"criterion":"AC2 Every github.pull_request.cancelled names its cause","outcome":"pass","revision":"f23601054","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_github_window_read_host|github_provider|application_release_maintenance|application_home_open_feedback_public_path|application_home_open_feedback_cli_public_path)$'","detail":"Each cancelled line's detail is cancel_cause=CAUSE (application::pull_request_cancel_cause): reviewer, restoration, review_closed, session_retired, window_closed, provider_stopped, provider_closing, transport. test_cancelled_read_names_its_cause drives every cause through the window read host and checks every cancelled line parses; the provider test names a transport's own cancellation transport and a cancel_pull_request stop_requested; the export keeps exactly a known cause and omits any other word; the composed course reads the profile's diagnostics file and finds four cancel_cause=reviewer lines (native) and none (CLI)."}},{"criterion":"AC3 python3 scripts/verify_documentation.py --root . passes","outcome":"pass","revision":"f23601054","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root .","detail":"DOCUMENTATION_OK on this branch merged with origin/main. The gate reads the counter's high water from every local branch. An unlanded branch (d823bb0a9, BPR-1878's allocation) holds BPR-1879, so the gate passes once that allocation lands. Raising the counter to BPR-1879 in the working tree only gives DOCUMENTATION_OK, and there are no other findings."}}]
---

# BPR-1875: A pull request opened from Home is not cancelled by a discovery refresh

## Observed

A PR open started from Home is cancelled a few seconds after a discovery
refresh starts. It restarts only when that refresh's long retained read
finishes, despite `home_loading_yields` / `yield_discovery`.

## Change

- Find who cancels the open and stop it: the open must keep running (discovery
  yields to it, not the reverse).
- Log the cancelling cause on `github.pull_request.cancelled`.

## Acceptance

- AC1. On fixtures, an open started from Home, then a discovery refresh,
  completes without cancellation or waiting for the refresh's retained read.
- AC2. Every `github.pull_request.cancelled` names its cause.
- AC3. `python3 scripts/verify_documentation.py --root .` passes.

## Investigation

The fixtures do not reproduce the cancellation. Every path that ends a
window's pull request read as cancelled was traced:

- the window read host's own `cancel()`: Home's Cancel
  (`cancel_home_open`), and `cancel_restoration` when a restoration is
  pending;
- `close_pull_request` (Close review);
- `release()`, from `retire` (the account session retired or replaced, as an
  identity invalidation or a session revalidation does) and `shutdown`;
- the provider's cancelled answer: its job stopped by `cancel_pull_request`
  or `shutdown`, or a transport answering cancelled with no stop asked.

Discovery touches none of them. `yield` and `cancel_discovery` stop only
`provider_phase::discovery` jobs, and the provider matches phase, context and
attempt. A CLI account's discovery runs on its own background `gh` runners
(`discovery_beside_reads`), so nothing yields. A native account's discovery
gives way when the open begins (`home_loading_yields`). A refresh begun while
the open waits submits nothing. A refresh begun earlier has its retained read
preempted, and the open goes first.

The new course holds the retained read, which BPR-1679's course never held,
in both orders and for both kinds of account. The open neither cancels nor
waits. Before this card the cancelled line carried no cause, so the owner's
log could not say which path it took. Now it can.

## Built

- `application::pull_request_cancel_cause` and `cancel_cause=CAUSE` in the
  `detail` of every `github.pull_request.cancelled` line
  (`github_window_read_host::record_cancelled`). `cancel(cause)` is named by
  its caller: `reviewer` for Home's Cancel and `restoration` for
  `cancel_restoration`. `close_pull_request` records `review_closed`;
  `retire` and `shutdown` record `session_retired` and `window_closed`.
- `github_pull_request_cancelled::Source` from the provider: `stop_requested`,
  `provider_closing` or `transport`, mapped to `provider_stopped`,
  `provider_closing` and `transport`. `transport` is the case nothing asked
  for.
- The diagnostic export admits the line only when it is exactly a known cause
  (`parse_pull_request_cancel_detail`), as documented in
  `docs/architecture/release-maintenance.md`.
- The fixture's `RetainedMilliseconds` hold, and the open-beside-refresh course
  in `home_open_feedback_public_path_tests.cpp` with its new `--gh-cli`
  registration `application_home_open_feedback_cli_public_path`.

## Open

The owner's real-network cancellation is still not explained. The next time it
happens, the `github.pull_request.cancelled` line's `cancel_cause` names the
path. If that cause is `transport`, or `provider_stopped` with no Cancel
pressed, the cause is below the window, in the transport or in another holder
of the provider, and a follow-up card should start there. This card adds no
behaviour change for it, because none of the traced paths is reached from a
refresh.
