---
schema: "implement-rfc.card.v1"
id: "BPR-0672"
title: "The attended review packet loses answers and notes"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
acceptance: [{"criterion":"Each of the four is reproduced, fixed, and covered by a test that fails before the fix","outcome":"pass","revision":"07df7068","evidence":{"kind":"observation","detail":"All four were reproduced in the shipped page and fixed in AgentSkills ASK-0019 on branch attended-review-verdict-repair: a final verdict, a note never shown again, no way to attach an image, and an offline export that omitted a decision made after the file was written. Its suite is 18 tests and passes; refusing a changed verdict, dropping attachments from the record, dropping the generation stamp and dropping the launch field each fail a named test. This repository's attended contracts pass unchanged."}},{"criterion":"A session run end to end records every verdict, note and image, and the export and the server intake agree","outcome":"pass","revision":"07df7068","evidence":{"kind":"observation","detail":"A throwaway session was served and driven in a browser: a note and a PNG attached to a rejection, read back in the verdict line with the note still in the box, changed to accept, cleared, and exported carrying the outcome, the note and the image. The generated offline file contained the live decision and its generated_at, and importing its export was accepted idempotently by the same intake."}},{"criterion":"Nothing in a packet asks the reviewer to find a file by hand","outcome":"pass","revision":"07df7068","evidence":{"kind":"observation","detail":"A candidate may carry launch: {label, command}; the page shows the command in a copyable field with a Copy button and never runs it, and the field is part of the candidate's fingerprint. Pinned by the offline-file test; dropping the field from the manifest's allowed keys fails it."}}]
---


# The attended review packet loses answers and notes

The owner reviewed the RFC-BPR-0028 attended session on 2026-09-07 and could
not complete it. In their words: "I have no way to reset/clear an answer if I
accidentally did one", "It looks like it lost my response note when I clicked
reject on the first one", "There's no place to add images so I have to use the
fallback flow", and "The fallback flow didn't have the decision I made in the
json export. Literally unusable."

Four defects, and the fourth is the serious one: a decision the reviewer made
was not in the export, so the record of what they decided was lost. The session
is the only durable record of an owner decision; losing one is worse than
never taking it.

## Scope

- Reproduce each of the four against the shipped page before changing it: a
  recorded decision that cannot be changed or cleared, a note discarded when
  the verdict is recorded, no way to attach an image to a verdict, and an
  offline export missing a decision the page had shown as saved.
- A verdict is editable until it is imported, and clearing one is offered.
- The note is part of the verdict and is saved with it.
- A verdict carries images, because a UI rejection is usually a picture.
- The export contains every decision the page shows as saved, and the page
  cannot show a decision as saved that the export would omit.
- The packet launches what it asks the reviewer to look at. The owner had to
  navigate a Finder tree to a path in prose: "the packets instructions to
  launch the app had me manually navigating my finder tree instead of a simple
  command or click". A prepared candidate that names a built application offers
  a one-click open or a copyable command.
- The skill is shared, so the fix lands in both copies and their equality is
  proved.
- Owned paths: none in this checkout. Both `.claude/skills/attended-review`
  and `.agents/skills/attended-review` are symlinks to
  `~/Documents/GitHub/AgentSkills/skills/attended-review`, so the fix is that
  repository's, tracked there.

## Acceptance

- Each of the four is reproduced, fixed, and covered by a test that fails
  before the fix.
- A session run end to end records every verdict, note and image, and the
  export and the server intake agree.
- Nothing in a packet asks the reviewer to find a file by hand.

## Notes (2026-09-07)

- **Where the fix lives.** This checkout has no copy of the skill: both
  `.claude/skills/attended-review` and `.agents/skills/attended-review`
  are symlinks to `~/Documents/GitHub/AgentSkills/skills/attended-review`.
  The repair is that repository's ASK-0019, on its branch
  `attended-review-verdict-repair`, cut from its `main` at the owner's
  instruction. It is not merged there; that is the owner's call.
- **What was wrong, and what each became.** A recorded verdict was final,
  because intake refused any different verdict and the page disabled both
  buttons: a verdict is now replaceable and withdrawable, and every
  superseded one is kept beside the current one. The note was saved and
  never shown, and the page jumped to the next candidate: the verdict line
  reads the note back, the box is refilled from the record, and the page
  stays on what was just answered. There was no way to attach a picture: a
  verdict carries up to four bounded inline images through the page, the
  export and the intake. The offline export omitted the decision: the file
  is a snapshot, it now records and states when it was written, and its
  export carries notes and images.
- **The launch affordance.** A candidate may carry a command that opens
  what it asks the reviewer to look at, shown for copying and never run.
  The packet that started this told its reviewer a path in prose and left
  them navigating a file tree.
- **Evidence.** The skill's suite is 18 tests and passes; four negative
  mutations each fail a named test; the AgentSkills structure gate reports
  no violations. This repository's own attended contracts,
  `attended_review_verdict_contract`,
  `operator_response_validation_contract`,
  `rfc0016_relocated_operator_contract` and
  `application_attended_launch_contract`, pass unchanged. The page was
  driven in a browser against a throwaway session: a note and an image on
  a rejection, read back in the verdict line, changed to accept, cleared,
  and exported with the outcome, the note and the image.
