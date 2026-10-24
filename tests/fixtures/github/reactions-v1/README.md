# Recorded reaction and resolution answers

[[BPR-1741]]. GitHub's answers to the product's own reaction and
resolution documents, recorded on 2026-10-22 by
`tests/scripts/reaction_live_course.py` on the owner-designated live target
`MikeMarcin/BPR-Test`, pull request 132 (closed and its branch deleted by the
course). The requests were the shipped compiled requests, checked canonical
before they left, and carried by `gh api graphql` as the owner `MikeMarcin`;
`MikeMarcinTest` had added a thumbs up first.

| File | Answer to |
|---|---|
| `observe-review-comment.json` | `ObserveReactionSubject` on the review comment, the second identity's thumbs up on it |
| `add-heart.json` | `AddReaction` HEART on the review comment |
| `add-thumbs-up.json` | `AddReaction` THUMBS_UP on it: two reactors, mine among them |
| `list-reactions.json` | `ListReactions` for the review comment, the pull request comment and the review |
| `remove-heart.json` | `RemoveReaction` HEART |
| `add-eyes-to-review.json` | `AddReaction` EYES on the review |
| `resolve-thread.json` | `ResolveThread` |
| `unresolve-thread.json` | `UnresolveThread` |

The pull request: repository `R_kgDOTyvofw` (1328277631), pull request
`PR_kwDOTyvof88AAAABKIhneA` (4974995320), number 132; review comment
`PRRC_kwDOTyvof88AAAABA4f-_w`, pull request comment `IC_kwDOTyvof88AAAABeqc0AQ`,
review `PRR_kwDOTyvof88AAAABTxO_vw`, thread `PRRT_kwDOTyvof86vZ2dW`. No
credential, header or request body is kept.
