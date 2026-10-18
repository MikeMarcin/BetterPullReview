# BPR-1603 handoff: design round (no product code)

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

## Next

- Build `docs/design/BPR-1603-conversation-chips-v1/` on the BPR-1574 kit.
- Render Light/Dark stills, open a fresh attended-review session, record
  URL here.

## Processes

- None running.
