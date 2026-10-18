# BPR-1603 handoff: design v001 under owner review (no product code)

- Worktree `/Users/jaeger/.codex/worktrees/bpr1603-conversation-chips/BetterPullReview`,
  branch `bpr-1603-conversation-chips` from origin/main `3b68c4394`.
- Scope this round: design only. Two or three directions as Light/Dark
  mockups, one choice question in a fresh short attended-review session,
  then stop. No implementation until the owner picks.

## Done

- Studied: RFC-BPR-0085 D3/D8/AC10, BPR-1592 card and
  `private_notes_panel.cpp` (count chip "N on the pull request" at the top
  edge, lock face; click switches the drawer to the elsewhere list with
  "Back to notes at their lines"), `conversation_cards_view.cpp` (top/bottom
  `edge_fade` with arrow; `edge_chip` = mark + line + unread dot, three each
  way then "+N"), BPR-1601 read state, BPR-1589's Status line.
- Finding: Reply is disabled today for comments not on a file (no code
  anchor); GitHub's conversation is flat, so replying means a new
  pull-request comment (issue comment). The staged-review publication
  already has `create_issue_comment`; a reply-in-place lane needs checking
  at build time.

- Design page `docs/design/BPR-1603-conversation-chips-v1/index.html` (built
  on the BPR-1574 kit, linked relatively), stills in `stills/` (A/B/C Light
  and Dark, shared-light) at `598f0aee5`. A: one chip, a list in place
  (Notes' pattern). B: the conversation hangs from the top edge as one
  thread. C: one chip per comment. Shared: Discuss unchanged; read once seen;
  Show in Discuss; tab count adds new PR comments; review summary shows
  verdict and its comments on files; reply = new PR comment quoting the one
  it answers; Status's Go to them still opens Discuss.
- Documentation gate passes (run with
  `PYTHONPATH=<main>/.agents/skills/implement-rfc/scripts`, since the
  worktree has no MASK deployment).
- Attended review: session `537cac7b15fbc9289047b7c091b65897`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261018-bpr1603-conversation-chips-design-v001`,
  candidate `BPR-1603-conversation-chips-design-v001` (revision
  `598f0aee5`, fingerprint `3fcd9253…d0eb`), one select question (A/B/C,
  notes welcome), ready at event sequence 3. URL
  `http://127.0.0.1:60273/` (token in the coordinator's report, not here).

## Next

- Read `status --after 3` on that state. On a pick: amend RFC-BPR-0085 D8
  if the pick departs from "one chip ... lists them" (B and C do in part),
  then implement in `conversation_cards_view.cpp` / `github_conversation_panel.cpp`
  reusing `private_notes_panel`'s count chip; check the reply lane
  (`staged_review_publication` has `create_issue_comment`). Stop host PID
  62087 when the review is consumed.

## Processes

- Attended-review host PID 62087 (handed off; stop by PID when done).
