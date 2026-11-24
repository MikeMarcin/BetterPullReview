---
schema: "implement-rfc.card.v1"
id: "BPR-2367"
title: "Seen for commits after my comments or change request, so the row can leave Needs me"
status: "Done"
rfcs: ["RFC-BPR-0098"]
depends_on: []
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"c338c1f23","evidence":{"kind":"command","detail":"commented / changes-requested row with commits after it needs me and offers Seen; Seen moves it to Following, writes nothing; the next push brings it back","command":"ctest --preset dev -R '^(core_review_queue_attention|application_review_queue_enrollment)$'"}},{"criterion":"AC-b","outcome":"pass","revision":"c338c1f23","evidence":{"kind":"command","detail":"an author's unread reply keeps the row in Needs me after Seen","command":"ctest --preset dev -R '^core_review_queue_attention$'"}},{"criterion":"AC-c","outcome":"pass","revision":"c338c1f23","evidence":{"kind":"command","detail":"approved wording and the approval courses unchanged","command":"ctest --preset dev -R '^(application_home_oversized_public_path)$'"}}]
metadata: {"origin":"owner bug report, 2026-11-24","extends":"BPR-2049, BPR-2217"}
---

# Seen for commits after my comments or change request, so the row can leave Needs me

## Origin

The owner, 2026-11-24: "It seems like if I have made comments on a PR and a
new commit comes in that hasn't addressed my comments the PR still goes into
Needs Me and there doesn't appear to be anyway for me to get it out of there,
short of probably making another comment."

## Root cause

`core::bucket_for` puts any pull request the viewer reviewed into
`changed_since_my_review` (Needs me) whenever its head moved past the
viewer's last review, whatever the verdict. The only way out was Seen
([BPR-2049], moved into the review by [BPR-2217]), and `review_followup_for`
and `head_seen` offered and honoured Seen only for a standing approval:
BPR-2049 deliberately left out "a change request", and comment-only reviews
were never considered. A commented or changes-requested row therefore stayed
in Needs me until the viewer submitted another review (which moves the
reviewed head) or ignored the row.

## Decision

The policy keeps a push after my review as a reason to look: the commit may
be the answer to my comments, and Home does not read review threads on pull
requests I did not author, so "the author has not answered" is not known.
Hiding the push on unanswered threads would silently miss an author who
fixes without replying. Instead Seen is offered for a standing comment-only
review and a standing change request, as for an approval:

- The review offers Seen beside Approve; Pull Request > Mark New Commits Seen
  is enabled for the same rows. Seen records the current head locally, writes
  nothing to GitHub, and the row moves to Following at once.
- The next push brings the row back to Needs me.
- An author's unread reply still keeps the row in Needs me
  (`new_comments_or_author_replies`): Seen clears the commits, not the reply.
- The words say what stands: "your change request stands", "your comments
  stand"; the approval wording is unchanged. A new standing
  `new_commits_since_review` is appended to `core::review_standing`.
- No review read (`none`) and a dismissed verdict offer no Seen, as before.

## Acceptance

- **AC-a.** A pull request I commented on, or requested changes on, that has
  commits after my review is in Needs me and offers Seen; Seen moves it to
  Following with no GitHub read or write; the next push brings it back.
- **AC-b.** With an author's reply I have not read, the row stays in Needs me
  after Seen, for the reply.
- **AC-c.** Approved and dismissed-approval behaviour and wording unchanged.

## Evidence

See the lane's report; commands above.

## Owner judgment pending (not a gate)

The owner may still prefer the policy route: keep a commented / changes-
requested row out of Needs me while every thread I opened is unresolved with
my comment last. That needs Home to read review threads for pull requests I
reviewed (today only authored ones), and would hide a silent fix.
