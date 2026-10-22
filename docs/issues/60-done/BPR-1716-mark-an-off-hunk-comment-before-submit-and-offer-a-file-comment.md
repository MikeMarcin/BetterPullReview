---
schema: "implement-rfc.card.v1"
id: "BPR-1716"
title: "Mark an off-hunk comment before Submit and offer a file comment"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-1715"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1715--BPR-1716--d95c8c662f30.json","origin":"Owner report, 2026-10-21: a review on GitHub Enterprise Server 3.20.8 carrying one comment outside every hunk was refused (BPR-1715)","related_cards":["BPR-1715","BPR-1706"],"built_by":"BPR-1735"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_review_submission_editor_scale_(100|125|200)|qt_workbench_line_draft|application_review_submission_composition_public_path)$'","detail":"Built by BPR-1735: a carried comment the core places outside every hunk is marked on Finish your review and on the comment, saying GitHub can't place it in a review, before Submit; a new draft on such a line says before Publish that Publish adds a comment on the file."}},{"criterion":"AC2","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^(github_review_comment_permalink|application_review_submission_composition_public_path)$'; python3 tests/end_to_end/bpr1735_whole_pr_live_course.py --cleanup","detail":"Publish as file comment publishes it once as RFC-BPR-0022's file comment with the permalink to the line at the head and its words byte for byte, out of the review; live on BPR-Test #127."}},{"criterion":"AC3","outcome":"pass","revision":"ba7ed6b095ecc4893bcb90ebf5cf0c656da469d8","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_filtered_comment|application_review_mutation_application_host)$'","detail":"Restated by the owner's decision: the mark and the offer rest on core::place_on_github_diff over the merge base to the head the epoch host reads, never on the host's name or version; with no merge base or the lines unread nothing is inferred."}}]
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

### Investigation, 2026-10-22

[[BPR-1735]] traces why no fallback fired: RFC-BPR-0022's contract is
scoped to BPR-Test PR2 alone, production placement is `unknown`, and the
review path has no fallback. It also shows that a response contract cannot
serve this card. GitHub refuses a review whole and names no comment, so no
contract can say which comment to convert (measured on BPR-Test #124). That
card proposes resting AC1 and AC2 on BPR-1706's measured placement rule,
applied to the Whole PR diff, instead of the response contract in AC3. The
acceptance below is unchanged until the owner decides.

## Owner decision, 2026-10-22

On [[BPR-1735]]'s options the owner chose, verbatim: "Accept option P (hold
off-hunk comments before Submit and publish them as file comments; single
Publish goes straight to a file comment), plus marking a refused single
comment rejected. Build it."

AC3 is restated accordingly. Its first wording ("The offer rests on an
admitted unsupported-response contract for the host, never on its name or
version") is superseded: a response contract cannot serve this card, because
GitHub refuses a review whole and names no comment (BPR-Test #124). The owner
admitted the core's measured placement instead (RFC-BPR-0022 amendment r5).

## Acceptance (restated 2026-10-22)

- AC1: Before Submit or Publish, a carried comment on a line outside every hunk is marked and says GitHub can't place it there.
- AC2: The reviewer can turn that comment into RFC-BPR-0022's file comment with a permalink to the line, as BPR-1706 does for filtered lines, keeping its text.
- AC3: The mark and the offer rest on the core's placement from GitHub's own diff (within three lines of a change of the merge base to the head), never on the host's name or version.

Built by [[BPR-1735]]; its evidence is the acceptance above.
