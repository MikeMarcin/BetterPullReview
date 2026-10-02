---
schema: "implement-rfc.card.v1"
id: "BPR-1200"
title: "Signed out, a private pull request link flashes an empty Overview, and Home refuses it too quietly"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin":"owner request in the note on attended candidate BPR-0641-private-pr-smoke-v001, session 06ad5734ed9f7c2fd6d0a62c8328fa0a, decision cbba6fac-e7dc-4b39-a7aa-1dbd0f7e1571, 2026-09-25","observed_revision":"3759b0d8caf5f1732c8bf807be427525f3f9a89a","related_rfcs":["RFC-BPR-0032","RFC-BPR-0026","RFC-BPR-0017","RFC-BPR-0016"],"related_cards":["BPR-0641","BPR-0668"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3bbc6561c","evidence":{"kind":"command","detail":"application_refused_first_read_public_path passes: signed out and signed in, the composed window stays on workbenchHome from submission to the refusal of octo-org/inaccessible-repository#17; its control (a readable pull request) records workbenchReviewSurface. With the Home rule disabled the course and qt_composed_home_refusal_scale_100 fail, the latter's control presents the workbench before the refusal and fails the predicate."}},{"criterion":"AC2","outcome":"pass","revision":"3bbc6561c","evidence":{"kind":"command","detail":"core_pull_request_refusal and core_github_policy pass (core words the refusal from the typed reason and connection). qt_composed_home_refusal_scale_100/125/200 pass: card under the bar, heading names the pull request, signed out Sign in then Use GitHub CLI, signed in Use another account then Use GitHub CLI, Copy details, one polite announcement from Home in the card's words, Tab from the bar reaches the first step, nothing clipped; renders build/dev/tests/adapters_qt/composed-home-refusal-*.png."}},{"criterion":"AC3","outcome":"pending","revision":"3bbc6561c","evidence":{"kind":"freeform","detail":"Owner attended review on macOS: candidate BPR-1200-refusal-card-v001 prepared at session sequence 613, one Accept/Reject question for AC3 alone."}}]
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

## Implementation — 2026-09-29

Candidate source `3bbc6561c` (on `4a60ee96d`, claimed at `d12895f4b` from
baseline `1e86156ad`). Mandate: `/implement-rfc BPR-1200`, unamended.

- **The core decides.** A first read that fails (no accepted review behind it)
  now carries a `core::pull_request_refusal` beside its diagnostic: the typed
  reason, the connection (`signed_out` for public preview, else `account`)
  and the named pull request (`github_transition.cpp`). A failed refresh
  carries none. `core::word_pull_request_refusal`
  (`src/core/src/pull_request_refusal.cpp`) words a permission refusal from
  that and the window's facts (login, whether GitHub CLI is offered, whether
  the account is GitHub CLI's): signed out "Sign in to open owner/name#N"
  with Sign in, then Use GitHub CLI; signed in "<login> cannot open
  owner/name#N" with Use another account, then GitHub CLI unless the account
  already is GitHub CLI's. Other reasons keep their diagnostic line.
- **Qt presents.** `qt::home_refusal` is a card under the Open bar (style
  warning icon, heading, explanation, steps, Copy details), painted from the
  palette. The window announces it once, politely, from Home in its own
  words; the line and its Copy details give way to it. Home's spare height is
  a fill below its last row, so the card and the loading sentence sit under
  the bar.
- **AC1.** A first read stays on Home, said under the bar, until GitHub has
  answered for both the repository and the pull request (two received
  pages), or its overview, review or cached overview is shown. A refused read
  gets no answered page, so it never reaches Overview. A long read still
  shows its progress on Overview from its first answers, which keeps
  [[BPR-0795]]'s owner rejection of a read that stayed on Home. The two held
  courses that pinned an Overview painted before any answer now pin Home
  with its loading or resuming sentence, then Overview once answers arrive.
- **Fixture.** The existing `octo-org/inaccessible-repository` denial now also
  answers the signed-out authority, with 404 as GitHub answers a reader
  without an account, after the same strict replay of the exact request.

### Evidence

| Criterion | Check | Result |
|---|---|---|
| AC1 | `ctest --preset dev -R ^application_refused_first_read_public_path$` (composed course: signed out and native signed in, every central page recorded from submission to refusal) | pass; control inside it (a readable pull request) records `workbenchReviewSurface` |
| AC1 control | same course and `qt_composed_home_refusal_scale_100` with the new Home rule disabled | both fail ("signed out, the refused read never passes through the workbench"; "while the first read runs the window stays on Home") |
| AC1, AC2 | `ctest --preset dev -R ^qt_composed_home_refusal_scale_(100\|125\|200)$` | pass at 1.0, 1.25, 2.0; its control presents the workbench before the refusal and fails the predicate |
| AC2 | `core_pull_request_refusal`, `core_github_policy` | pass |
| Integration | 65 focused tests (all `core_*`, GitHub/setup/window-read hosts, public preview, setup-destination and restoration public paths, architecture boundaries) plus `application_profile_window_review_restoration_public_path` | pass |
| Gates | `coding-format-check`; clang-tidy on touched TUs; `python3 scripts/verify_documentation.py --root .` | touched files clean (remaining format findings pre-exist at baseline); tidy pass; `DOCUMENTATION_OK` |

Renders (ignored build tree):
`build/dev/tests/adapters_qt/composed-home-refusal-{100,125,200}-{loading,signed-out,account}.png`.

Pre-existing at baseline, unchanged by this card: `qt_composed_github_scale_*`
stop at "the agent launcher, the side chat, and the finding queue stack down
one visible column"; `qt_workbench_window_scale_*` at "no Notes tab control
is clipped at 200 pixels"; `qt_document_tabs_scale_*` and
`qt_workbench_focus_order_scale_*` at duplicate keys; the dev build of
`agent_profile_tests.cpp` (static size assertion) and `scoped_writing_agent.cpp`
fails to compile.

### Checkpoint — AC3 pending

AC3 is the owner's alone. Attended candidate `BPR-1200-refusal-card-v001`
(fingerprint `23453e02…`) is prepared at session sequence 613: the
tests-disabled app built from `3bbc6561c` in `build/app-release-bpr1200`,
copied to the disposable root `/Users/jaeger/bpr1200-review.cM4Yfi/review`
(executable SHA-256 `5a965e91…ac87`), with the CLI read fixture. One
Accept/Reject question for AC3. Next: on hand-back, record the verdict; on
Accept finish to Done; on Reject apply the note and prepare v002. Follow-up
worth a card: the signed-out provider publishes no overview observation.

## Owner verdict and result — macOS, 2026-10-02

The owner accepted `BPR-1200-refusal-card-v001` (AC3) at event 810 of
session `cbd48b91000b00f1e72e1ad62c7c384c`. A second save at event 811 kept
Accept and added the note "Marvelous." The owner handed back at 815, and the
protection was released by name at event 820. The candidate was the
tests-disabled app built from `3bbc6561c` (executable SHA-256
`5a965e91…ac87`).

AC1 and AC2 pass on the evidence recorded above, and AC3 now passes. The card
names no platform requirement, so it is Done. The follow-up noted in the
checkpoint still stands as a candidate card: the signed-out provider
publishes no overview observation.
