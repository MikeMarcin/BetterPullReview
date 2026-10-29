---
schema: "implement-rfc.card.v1"
id: "BPR-1803"
title: "My comments show my GitHub login, not \"you\""
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1803--BPR-1806--63af5d42316f.json","kind":"change","found_by":"owner, 2026-10-28","related_cards":["BPR-1741","BPR-1757","BPR-1374"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "a73f5dc740e489c79430755a65ba0d58fcfea42a", "evidence": {"kind": "command", "detail": "qt_conversation_cards_view test_my_posts_are_named_by_my_login: my thread root, my reply and my unpublished note carry my login, and my open reply's accessible name is reviewer-one; test_reactions_show_and_toggle_by_mouse_and_keyboard: 'Post by reviewer-one', 'reviewer-one reacted with heart', 'reviewer-one, someone and octocat reacted with thumbs up, waiting for GitHub'. qt_conversation_finder_scale_100/125/200: my login finds my posts. A source search of src/adapters/qt finds no 'you' in a name slot."}}, {"criterion": "AC2", "outcome": "pass", "revision": "a73f5dc740e489c79430755a65ba0d58fcfea42a", "evidence": {"kind": "command", "detail": "Edit and reaction gating still read Mine (code paths unchanged); qt_conversation_cards_view, qt_feedback_label_threads, qt_composed_github_scale_100 and qt_workbench_window_scale_100 pass. The Discuss render shows 'Draft reply' and the list render shows the Draft meta on my note."}}, {"criterion": "AC3", "outcome": "pass", "revision": "a73f5dc740e489c79430755a65ba0d58fcfea42a", "evidence": {"kind": "command", "detail": "test_my_posts_are_named_by_my_login clears the viewer login: my note carries no login and its head is drawn 'Unknown author' (list-no-login-light.png) through shown_author, the words the Pull request page and panel use."}}, {"criterion": "AC4", "outcome": "pass", "revision": "a73f5dc740e489c79430755a65ba0d58fcfea42a", "evidence": {"kind": "artifact", "detail": "Offscreen renders outside the repository: better_pull_review_qt_conversation_cards_view_tests --render-bpr-1803 wrote list-light.png, open-thread-light.png, list-no-login-light.png, thread-light/dark.png and picker-light/dark.png; --discuss-alignment with BPR_DISCUSS_ALIGNMENT_PNG wrote discuss-draft-reply-badge.png, all in the session scratchpad's self-name-renders directory."}}]
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

## Built

- `shown_author(login)` in `conversation_open_card` names a post's author:
  the login, or "Unknown author" when none is known. The open card's post
  header, avatar and elided-name tooltips and accessible names, the closed
  note's head, the thread chips and the finder rows all name through it, so
  my posts read as anyone's.
- Draft cards carry the viewer's login (empty when unknown) instead of
  "you". The note head no longer says "Your note", "Your reply" or "Your
  suggestion"; the meta and dashed frame say what the draft is. The
  unsent-reply marker in a note's meta reads "Draft reply".
- Reaction descriptions name me by login in GitHub's order, prepending my
  login only when GitHub's short list stopped before me; without a login I
  am one of the count. A checked picker option is named "Heart, added".
- The finder's search no longer matches "you" to my posts; my login does.
