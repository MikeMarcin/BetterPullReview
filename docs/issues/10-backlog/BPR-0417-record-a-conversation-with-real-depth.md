---
id: BPR-0417
title: Record a conversation fixture with real reply depth
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0416]
---

# Record a conversation fixture with real reply depth

[[BPR-0211]]'s own precondition, unmet since it was written: "a conversation deep
or wide enough to be a problem is recorded, so the question is asked against real
content rather than synthesised depth."

Measured during [[BPR-0413]] rather than assumed. No recorded pull request in
this repository carries a conversation at all — `directxmath-336` and
`uiforetw-74` both have top-level keys `commits, files, pull_request,
recorded_from, recorded_on, repository` and nothing else. The only conversation
content anywhere is three contract fixtures
(`tests/fixtures/github/contracts/v1/{github-com,ghes-3.18,uiforetw-74}/responses/review_threads.json`);
the first two carry three threads of one, two, and one comments with exactly one
`replyTo` edge, and the third carries none. **Maximum depth in this repository is
one reply.**

So [[BPR-0418]]'s map cannot be evidenced today. Every render would be of
content invented to make the view look necessary, which is the failure mode the
board already refuses elsewhere.

## What this card must produce

A fixture whose reply structure is deep and wide enough that a flat list is
genuinely hard to follow — the condition [[BPR-0211]] named. It must be honest
about its provenance:

- If recorded from a real public pull request, record it the way the existing
  recordings are, with `recorded_from` and `recorded_on`, and pick a pull request
  whose conversation is genuinely deep rather than one padded to look so.
- If constructed, say so in the fixture's README and never let a later card cite
  it as recorded evidence. A constructed fixture is legitimate for exercising the
  model; it is not evidence that the view solves a real problem.

Either way it exercises the cases [[BPR-0416]] answers: a chain several deep, a
first comment with a null `replyTo`, a `replyTo` naming an absent comment, and
siblings at the same depth.

## Verification

```sh
ctest --preset dev --output-on-failure -R 'github_conversation|github_fixture'
python3 scripts/verify_documentation.py --root .
```
