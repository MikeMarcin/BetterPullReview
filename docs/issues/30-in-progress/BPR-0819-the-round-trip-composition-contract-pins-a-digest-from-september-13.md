---
schema: "implement-rfc.card.v1"
id: "BPR-0819"
title: "The round-trip composition contract pins a github_application.cpp digest from September 13"
status: "In Progress"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"integration run at the BPR-0810/BPR-0811 merge; confirmed at a clean 8b2bea5b worktree","found_at":"8b2bea5b83639c78f69f431e24a724e0fccdb3e9","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0819: The round-trip composition contract pins a github_application.cpp digest from September 13

## Defect

`application_review_round_trip_composition_contract`
(`tests/end_to_end/application_review_round_trip_composition_contract.cmake`)
fails in 0.06 s because the source digest it pins for
`src/app/src/github_application.cpp` is
`5df1138b95f54a780a4dbf2aa7312695b9164001f407928b7b08ff1965c15f82`. That value
was last refreshed in `ce7df75e` ("Refresh composition digest for CLI profile
checkpoint", 2026-09-13), in RFC-BPR-0015's first dispatch. It matches none of
the revisions of this run, from `3f1fd775` through `f33a10c9`. The contract has
therefore been red since the file's next change, and a reviewed-digest tripwire
that is always red reviews nothing.

The file changed since then under several cards, each with its own review
record:

- BPR-0786, `df458d30`;
- [[BPR-0797]], `b1f89533` and `a78eefcf`;
- [[BPR-0804]], `f2b8e682`;
- [[BPR-0806]], `d0964fdd`;
- [[BPR-0808]], `282e4908`;
- [[BPR-0810]], `3ca3e12c`.

## Scope

- Read what the digest protects: the review round-trip composition surface
  and the RFC-BPR-0015 confirmation path. Confirm that every change since
  `ce7df75e` to that surface has a recorded review.
- Reseal the digest in the same commit that records that reconciliation, or
  replace the whole-file digest with a narrower check that does not go red on
  unrelated edits to a 13,000-line composition file.
- Do not reseal blindly.

## Acceptance

- The contract passes on macOS, and this card lists the reviewed changes the
  reseal covers.
- [[BPR-0788]] knows about the reseal.
