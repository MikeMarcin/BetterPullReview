---
schema: "implement-rfc.card.v1"
id: "BPR-1716"
title: "Mark an off-hunk comment before Submit and offer a file comment"
status: "Backlog"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-1715"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1715--BPR-1716--d95c8c662f30.json","origin":"Owner report, 2026-10-21: a review on GitHub Enterprise Server 3.20.8 carrying one comment outside every hunk was refused (BPR-1715)","related_cards":["BPR-1715","BPR-1706"]}
---

# BPR-1716: Mark an off-hunk comment before Submit and offer a file comment

## Owner report

On 2026-10-21 a review with nine comments, one of them on a line outside
every hunk, was refused by a GitHub Enterprise Server 3.20.8 host. [[BPR-1715]]
makes such a refusal settle and give the comments back; this card stops the
off-hunk comment from causing the refusal in the first place.

## Notes

BPR-1715's diagnosis puts native off-hunk acceptance on that host at very
unlikely (85 to 90 percent): the REST contract defines a review comment's
line as one in the diff, and the only recorded observation is GitHub.com
refusing an off-hunk single comment with 422. The specification keeps the
production file-comment fallback unavailable until an exact
unsupported-response contract is admitted, so AC3 is that admission.

## Acceptance

- AC1: Before Submit or Publish, a carried comment on a line outside every hunk is marked and says GitHub may not place it there.
- AC2: The reviewer can turn that comment into RFC-BPR-0022's file comment with a permalink to the line, as BPR-1706 does for filtered lines, keeping its text.
- AC3: The offer rests on an admitted unsupported-response contract for the host, never on its name or version.
