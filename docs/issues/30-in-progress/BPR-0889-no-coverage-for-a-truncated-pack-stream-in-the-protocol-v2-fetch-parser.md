---
schema: "implement-rfc.card.v1"
id: "BPR-0889"
title: "The protocol v2 fetch parser's truncated-stream refusal has no visible coverage"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable github+git partition; the reviewer flagged this one as uncertain","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0889: The protocol v2 fetch parser's truncated-stream refusal has no visible coverage

## Defect

`src/application/src/git_protocol_v2.cpp:1073-1082` returns
`incomplete_response` from `finish()` when the stream ends mid-packet or
before the flush, and `~git_protocol_v2_fetch_parser` (`:830`) aborts the
sink. Nothing appears to exercise either.

`tests/application/git_protocol_v2_tests.cpp` has five references to failure
reasons and no test named for truncation; grep for
`truncat|mid-packet|partial` returns nothing.

This is a **wire** surface, where a passing suite can still miss the exact
case that matters: bytes stopping right after a sideband channel byte, with
`ChannelRead` true and the payload incomplete. A network that drops mid-pack
is ordinary, and the difference between a clean refusal and a half-consumed
sink is the difference between a retry and a corrupt cache.

The reviewer marked this **uncertain**: it did not read every test body, so
the coverage may exist under a name the grep missed. Confirming that is the
first step and may close the card.

## Scope

- Confirm whether the coverage exists. If it does, record where, and close
  this.
- If it does not, add direct tests that assert `incomplete_response` and that
  the sink observed `abort()`, for at least: truncation mid-packet-header,
  truncation after a sideband channel byte with an incomplete payload, and
  truncation before the flush.
- Check the same question for the sparse path if it has its own parser.

## Acceptance

- Each truncation shape either has a test or a recorded reason why it cannot
  occur.
- A truncated stream leaves no partially written pack behind, and that is
  asserted rather than assumed.
