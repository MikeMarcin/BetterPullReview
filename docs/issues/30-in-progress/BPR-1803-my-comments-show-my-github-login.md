---
schema: "implement-rfc.card.v1"
id: "BPR-1803"
title: "My comments show my GitHub login, not \"you\""
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1803--BPR-1806--63af5d42316f.json","kind":"change","found_by":"owner, 2026-10-28","related_cards":["BPR-1741","BPR-1757","BPR-1374"]}
acceptance: []
---

# BPR-1803: My comments show my GitHub login, not "you"

Owner, 2026-10-28: "I don't like the comments or windows saying "you" id
rather just always have our GitHub names like everyone else".

Where the conversation surfaces put a name on something the signed-in user
wrote or did, they said "you": the open thread's post header and its avatar
tooltip ("login (you)"), the closed note's head in Discuss and in the
file's list, the off-screen thread chips' tooltip and accessible name, the
Conversations finder row and its search (which matched "you"), and the
reaction tooltips ("You, someone and octocat reacted with thumbs up"). An
unpublished note's head said "Your note", "Your reply" or "Your suggestion"
in the same slot, and a note with an unsent reply showed "Your reply" in its
meta.

## Acceptance

- AC1. Every post, note head, chip, finder row, reaction description and
  accessible name that names the author of something the signed-in user
  wrote names them by their GitHub login, drawn as anyone else's name is.
  None says "you" or "You" in a name slot.
- AC2. An unpublished note still says it is unpublished (its dashed frame
  and the meta's "Draft", "Pending", "Not sent" or "Earlier head"); the
  unsent-reply marker reads "Draft reply". Edit and reaction permissions
  still follow `Mine`.
- AC3. With no login known (offline or a fixture without a viewer), the
  name is "Unknown author", as the Pull request page and panel already say
  for a post with no login, never "you".
- AC4. Offscreen renders of my thread, my note with and without a login,
  and the unsent-reply marker exist outside the repository.

## Scope

Name labels only. Second-person sentences that address the user ("Your
draft is kept", "Only you see this draft", "You approved this on ..." on
Home, "Verified by you" in Review Cycles, the defect card's "You checked
this") are instructions or status addressed to the reader, not a name put
on a post, and are unchanged; the owner can widen this. The 1 px ring on my
own avatar is unchanged. Comment-body Markdown rendering is untouched.
