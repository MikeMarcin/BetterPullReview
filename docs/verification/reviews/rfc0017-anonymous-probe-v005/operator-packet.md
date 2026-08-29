# RFC-BPR-0017 anonymous conditional-304 accounting operator packet, second issue

Status: **operator-approved in advance; single issuance not yet spent**

This packet requests one bounded decision covering exactly two
credential-free, read-only requests to the same public endpoint, executed
seconds apart in one authorized run: a fresh GET of
`https://api.github.com/repos/octocat/Hello-World/pulls/1` recording the
current `ETag` (expected 200), then a conditional GET of the same URL
replaying that just-recorded `ETag` as `If-None-Match` (expected 304). The
approved course-selection block below is the execution schedule, in that
order.

It authorizes nothing else. No credential, mutation, private repository,
disposable repository, GitHub Actions run, GHES instance, proxy, redirect,
retry, quota exhaustion, or rate-limit evasion is permitted. The two
requests spend 2 of the anonymous 60-per-hour core budget. This is not the
separate anonymous-preview product decision.

## Why this packet exists

The v004 packet's single conditional GET replayed a committed five-day-old
`ETag` and was refused on a body-carrying answer, consuming its issuance:
the pull-request representation embeds volatile repository counters, so a
committed validator goes stale on its own. This packet's paired same-run
course closes that window from days to seconds. v004 stays byte-frozen as
its own record; this is the successor gate for the same accounting
question.

## Advance approval

The operator pre-approved this exact scope in the live session, before
this packet was authored, with the explicit direction to proceed without
another round-trip. The operator's verbatim words are recorded on
[[BPR-0298]]; the canonical response file binds this packet's digest to
the authority statement below under that direction. This deviation from
quote-after-authoring is deliberate, operator-directed, and recorded
rather than implied.

## Bound authority identity

```json
{
  "schema_name": "bpr.rfc0017.anonymous_probe_authority",
  "schema_version": 2,
  "statement": "I authorize exactly the two credential-free read-only requests named below against api.github.com.",
  "targets": [
    {
      "name": "pull-request-fresh",
      "operation": "rest_pull_request",
      "url": "https://api.github.com/repos/octocat/Hello-World/pulls/1"
    },
    {
      "name": "pull-request-conditional",
      "operation": "rest_conditional_not_modified",
      "url": "https://api.github.com/repos/octocat/Hello-World/pulls/1",
      "condition_source": {
        "same_run_course": "pull-request-fresh"
      }
    }
  ]
}
```

## Approved course selection

This is the exact second JSON object capture mode parses. It is part of the
packet digest, and its parsed `courses` array must match the manifest's
`courses` array exactly in target, status, order, and count before any
output reservation, attempt receipt, or request.

```json
{
  "schema_name": "bpr.rfc0017.anonymous_course_selection",
  "schema_version": 1,
  "courses": [
    {
      "name": "pull-request-fresh",
      "expected_status": 200
    },
    {
      "name": "pull-request-conditional",
      "expected_status": 304
    }
  ]
}
```

## What the two courses record

The fresh course records a current sanitized pull-request contract —
refreshing the 2026-08-23 record — including the current `ETag`,
`Last-Modified`, and rate-budget counters. The conditional course then
replays that just-persisted `ETag` (and `Last-Modified` when present) and
records the `304 Not Modified` contract: empty body by rule, the response's
own retained headers, and above all the
`x-ratelimit-limit`/`remaining`/`used` counters beside the fresh course's —
the direct witness for whether an anonymous conditional `304` consumes the
core budget, which is the accounting fact RFC-BPR-0017's ledger needs.

Two live outcomes end without a committed conditional capture, stated
plainly:

- If the representation changes in the seconds between the two requests —
  a repository counter moving is enough — the conditional answer is `200`
  with a body and the recorder refuses after the single-use attempt
  receipt exists, consuming this packet's issuance with the fresh capture
  already durably written but the run refused. A fresh packet would be
  required.
- Any other status, a body on the `304`, or a fresh course that fails its
  own course rules aborts the run the same way; if the fresh course
  refuses, the conditional request is never issued.

## Bound evidence

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `scripts/record_rfc0017_anonymous_contracts.py` | `sha256:52fb5c7cd36faf5d85756b85f3f0946d847eb727c304dabb51d14b989ef2b00c` | 84187 |
| `tests/scripts/record_rfc0017_anonymous_contracts_tests.py` | `sha256:499def8fc9b0dcaa634a86a004403ace56a2b39787019c583886b4c7724bd61c` | 118690 |

The paired-course mechanism is committed at `90754ca1`; the external
capture binding at `4d166772`; the conditional operation itself at
`f7ba12e7`. The focused suite passes 166/166 with a 15/15 mutation sweep
over the same-run logic, the seeded v001 validation prints
`RFC0017_ANONYMOUS_CONTRACTS_VALID:5`, `BOUND:5`, and `DERIVED:1`, and no
network request ran while preparing or verifying this packet.

## Mechanical pre-request bounds

Capture mode refuses before traffic unless all of these remain exact:

- the response carries this packet's one SHA-256 and one exact quoted
  approval using the recorder's canonical response byte grammar;
- the current recorder digest and byte count match the bound-evidence row;
- every recognized lowercase `json` fenced object block parses without
  duplicate members, and the packet has exactly one authority and one
  course-selection schema object regardless of member order;
- the manifest binds this packet and exactly matches the two selected
  courses in order;
- the same-run reference is structurally proven before the single-use
  attempt receipt is reserved: the source course exists, runs strictly
  earlier, and names the same URL; the replayed `ETag` is read back from
  the fresh course's just-persisted record, never from any committed file;
- the output is a safe descendant of this packet directory and the
  destinations do not exist;
- a repository-wide packet-identity attempt receipt is exclusively
  created, flushed, and durably synced before the first request;
- both requests remain HTTPS GETs to the one URL above with no ambient
  proxy and no redirect, the conditional one carrying only `If-None-Match`
  and, when the fresh record supplies it, `If-Modified-Since`.

Within this checkout, the recorder never deletes an attempt receipt, even
if GitHub is unavailable or a returned shape is refused.
`capture-bindings.json` and its owning-commit verification own the
repository-external evidence boundary.

## Operator response

The advance approval recorded above authorizes the integrator to bind and
spend this packet's single issuance without a further round-trip.

Approval statement bound by the canonical response:

> I authorize exactly the two credential-free read-only requests named below against api.github.com.

Rejection before the run would be:

> I reject this anonymous conditional probe.

An approval permits the recorder to execute the packet-bound two-course
selection once, in order. On any refusal, no further request runs and the
consumed state is recorded.
