---
schema: "implement-rfc.card.v1"
id: "BPR-2049"
title: "Seen, or Approve again, for an approval that new commits brought back to Needs me"
status: "Done"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2048--BPR-2049--1f6f243721c8.json","origin":"owner request, 2026-11-10"}
depends_on: []
acceptance: [{"criterion":"BPR-2049 Seen","outcome":"pass","revision":"146bc2b35ef9344ef382b83c0d319eb400efc3c7","evidence":{"kind":"command","detail":"Seen moves the row at once, writes nothing, survives the store","command":"ctest --preset dev -R 'core_review_queue_attention|application_review_queue_enrollment|application_home_oversized_public_path'"}},{"criterion":"BPR-2049 Approve again","outcome":"pass","revision":"146bc2b35ef9344ef382b83c0d319eb400efc3c7","evidence":{"kind":"command","detail":"Approve again opens the review with Approve focused","command":"ctest --preset dev -R application_home_approve_again_public_path"}},{"criterion":"BPR-2049 owner look","outcome":"pass","revision":"6f12ae3f91ae20e99c2fd06b598ece3a08074c7b","evidence":{"kind":"observation","detail":"Owner accepted BPR-2049-seen (decision c495f9e1) and BPR-2049-approve-again (decision 3e464b5d), attended session 9ed1f1a047a56e4d35f43e92d4f9d70b, no note"}}]
---

# Seen, or Approve again, for an approval that new commits brought back to Needs me

## Origin

The owner, 2026-11-10: "So if I approved something already and it gets new
commits it moves back to needs me which makes sense but then I don't know how
to say okay I still accept it so it clears new commits and moves back to
following."

## Scope and acceptance

Agreed design: while GitHub still holds the owner's approval, the row (and
the pull request) offers Seen, which records the new commits as seen locally
and moves the row back to Following with no GitHub write. When GitHub
dismissed the approval (a stale-review dismissal), the row offers Approve
again, which submits a real approval; the row moves once GitHub confirms. The
row moves at once after Seen, not at the next refresh. No Unapprove action
(the owner declined one).

Load frontend-design before the Qt work; the owner's look on a frozen build
is filed, not waited on.

## Evidence

Record: [RFC-BPR-0098 verification, owner fix round](../../verification/RFC-BPR-0098.md#owner-fix-round-bpr-2048-and-bpr-2049-2026-11-10).
Frontend-design was loaded before the Qt work.

- Core decision and Seen arithmetic: `core_review_queue_attention` — pass.
- Seen moves the row at once, writes nothing, survives the store (facts
  version 5), a later push brings it back: `application_review_queue_enrollment` — pass.
- Composed: Seen on #18 moves it to Following asking GitHub for nothing
  (`application_home_oversized_public_path`); Approve again opens the pull
  request with its review and Approve, focused
  (`application_home_approve_again_public_path`) — pass.

Decision for the owner's look: Approve again opens the pull request's review
with Approve ready rather than submitting from the row, so a summary or saved
comments the review holds go with it knowingly; GitHub's confirmation and the
return to Home move the row. Asked as Accept or Reject in the look.

## Owner look (filed, not waited on)

Attended session `9ed1f1a047a56e4d35f43e92d4f9d70b`, state
`/Users/jaeger/.attended-review-state/home-oversized-seen-v001-2026-11-10/state`,
cursor 12, four candidates ready and protected: `BPR-2048-oversized-row`,
`BPR-2048-too-large-card`, `BPR-2049-seen`, `BPR-2049-approve-again`, on the
frozen composed course of `6f12ae3f9` in
`/Users/jaeger/home-oversized-seen-v001.9rw9vr9h` (course binary SHA-256
`2766916cba0e…`, app `f4309598a892…`; launcher `--check` passed on
README.md of nested-folders-v1). The URL stays in the session state.
