---
schema: "implement-rfc.card.v1"
id: "BPR-1200"
title: "Signed out, a private pull request link flashes an empty Overview, and Home refuses it too quietly"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin":"owner request in the note on attended candidate BPR-0641-private-pr-smoke-v001, session 06ad5734ed9f7c2fd6d0a62c8328fa0a, decision cbba6fac-e7dc-4b39-a7aa-1dbd0f7e1571, 2026-09-25","observed_revision":"3759b0d8caf5f1732c8bf807be427525f3f9a89a","related_rfcs":["RFC-BPR-0032","RFC-BPR-0026","RFC-BPR-0017","RFC-BPR-0016"],"related_cards":["BPR-0641","BPR-0668"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json"}
---

# BPR-1200: Signed out, a private pull request link flashes an empty Overview, and Home refuses it too quietly

## Observation

In [[BPR-0641]]'s read-only smoke (packet
[owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)),
the owner, signed out, pasted the link to a private pull request. The owner,
verbatim: "I hit a weird broken overview page after going to the URL without
an account, getting kicked back to home, and then connecting with CLI", and
"the error message (4th screenshot) for getting kicked out feels too subtle
and is easy to miss, I think we should file a follow-up card to improve that
UX."

What the screenshots show, in the app's words:

1. The workbench on Overview with two empty panes, "Select a file comparison"
   in the header and the status line "No pull request is open in the
   workbench." Nothing says a pull request is being opened or why it failed.
2. Then Home, with one line of ordinary helper text under the Open field:
   "This app's GitHub connection cannot read this pull request. Your browser
   sign-in may have different access." and Copy details at the right, beside
   the Sign in card. The status bar reads "Not signed in. Public pull requests
   open read-only."

The line is `github_presentation_text.hpp`'s `permission_denied` sentence. It
talks about the browser's access although the app is signed out, and it does
not say that signing in may open the pull request.

## Acceptance

- AC1 Opening a pull request link that the current connection cannot read
  never presents an empty workbench first: the window stays on Home, or
  returns there, without passing through Overview. A composed course pins it,
  and a control that presents the workbench before the refusal fails it.
- AC2 The refusal on Home is prominent and actionable: set apart from helper
  text, it names the pull request and says why in terms of the connection
  (signed out: sign in to open private pull requests; signed in: this account
  cannot read it), offers the matching action (Sign in, Use GitHub CLI, or
  another account), and is announced to accessibility. Renders at 100, 125
  and 200 percent.
- AC3 The owner judges the revised refusal on macOS in attended review, one
  question for this criterion alone.

## Boundaries

No live private pull request is needed for the automated courses; use the
existing permission-denied fixtures. Credentials, private repository content
and the owner's screenshots stay out of the repository. The core decides the
refusal's wording from the typed reason and connection state; Qt presents it.
