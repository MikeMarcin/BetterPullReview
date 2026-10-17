---
schema: "implement-rfc.card.v1"
id: "BPR-1574"
title: "Design round for Notes, Tours, Agent, Compose review and the Overview panel"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
parked_on: "human-review"
metadata: {"origin":"Owner, 2026-10-17 (chat, relayed by the coordinator), extended the same day with Compose review, the Overview side panel and the Start review decision","reservation":"docs/issues/_reservations/BPR-1574--BPR-1574--5d8de7b4e853.json","rfc_draft":"docs/rfcs/assets/drawers-and-agent-redesign-v1/RFC-DRAFT-0023-drawer-design-system-agent-compose-overview.md","design_round":"docs/design/BPR-1574-drawers-and-agent-v1/index.html","requested_skill":"frontend-design","related_rfcs":["RFC-BPR-0012","RFC-BPR-0023","RFC-BPR-0028","RFC-BPR-0039","RFC-BPR-0052","RFC-BPR-0068","RFC-BPR-0069"],"related_cards":["BPR-1290","BPR-1204","BPR-1212","BPR-1345","BPR-1541","BPR-1577","BPR-1579"],"acceptance":["BPR-1574:audit","BPR-1574:directions","BPR-1574:rfc","BPR-1574:session","BPR-1574:choices"]}
---

# BPR-1574: Design round for Notes, Tours, Agent, Compose review and the Overview panel

## Origin

The owner, 2026-10-17:

> I think that comments drawer is beautiful now, but the notes and tours
> drawers are pretty ugly, and I think agent probably deserves a first-class
> tab, not to be tucked away. Let's start a UX redesign process.

Added the same day: Compose review ("I'm not sure what's going on on the left
side of compose review. Is that supposed to be useful? I don't think I've ever
done more than try to ignore it."), the Overview's side panel ("The right pane
overview is getting extremely busy … It'd also be good to make it resizable
maybe? It looks like a drawer but doesn't behave like the other drawers in that
way.") and a decision: "Start review is probably unnecessary now that the
Review tab is so convenient, and double-clicking a file or clicking Next file
also works."

## Outcome

A design phase only; no product code changes. It delivers the audit, two or
three named directions per surface in Light and Dark, one attended session
whose questions choose between them, and the draft RFC that becomes the
decided design once the owner answers.

## Acceptance

- **BPR-1574:audit.** The audit of the Comments drawer, Notes, the Tour tab,
  the agent surfaces, Compose review and the Overview panel is in the RFC
  draft, with offscreen before renders in Light and Dark.
- **BPR-1574:directions.** Each of the five surfaces has two or three named
  directions drawn as whole-window frames in Light and Dark with rationale and
  trade-offs, plus the shared principles board.
- **BPR-1574:rfc.** RFC-DRAFT-0023 records the problem, the audit, the
  principles, decision D1 and the open choices Q1 to Q5.
- **BPR-1574:session.** A fresh short attended session asks one question per
  surface (a choice between its named directions) and one Accept or Reject
  question on the principles.
- **BPR-1574:choices.** The owner's answers are recorded here and in the RFC,
  which is then revised to the decided design. Owner's, pending.

## Execution record

Design worktree `bpr-1574-drawer-redesign`, from `main` at `928dd4191`.
`frontend-design:frontend-design` was loaded first, then `author-rfc`.

**Before evidence.** A scratch export of `928dd4191` with an uncommitted
capture hook in `main.cpp` (evidence tooling only, never committed; the build
tree was deleted afterwards) was built `app-release` with tests on and run
offscreen with `--attended-launch` on `inline-drawer-v1`
(`src/ui/conversation_cards_view.cpp`) and `conventional-comments-v1`
(`--attended-preflight-look review`, `--attended-unknown-review-look`). It
switched the tray tabs and the Notes sub-tabs, chose each modality and drove
the Appearance page's own Light and Dark tiles, then grabbed the window. The
14 committed renders are in
[`before/`](../../design/BPR-1574-drawers-and-agent-v1/before/). No desktop
was captured.

**Directions.** Static HTML frames sharing one kit (`kit.css`, `kit.js`) whose
tokens are the accepted BPR-1290 prototype's:
[index](../../design/BPR-1574-drawers-and-agent-v1/index.html),
[principles](../../design/BPR-1574-drawers-and-agent-v1/principles.html),
[notes](../../design/BPR-1574-drawers-and-agent-v1/notes.html),
[tours](../../design/BPR-1574-drawers-and-agent-v1/tours.html),
[agent](../../design/BPR-1574-drawers-and-agent-v1/agent.html),
[compose](../../design/BPR-1574-drawers-and-agent-v1/compose.html),
[overview](../../design/BPR-1574-drawers-and-agent-v1/overview.html). Each
page takes `?still=ID&theme=dark`; the 34 stills for the session were rendered
from those by headless Chrome with a scratch profile and are kept with the
session, not in the repository.

**Start review audit (D1).** The button calls `enter_overview_at`, which calls
`enter_review_at(EntryPath)`: Review on the first unseen change's file. It is
the page's default button (Return). It starts no session and records nothing.
The RFC keeps both behaviours: ⌘2 from a fresh Overview lands on `EntryPath`,
and Return on the Files list opens the selected file.

## Owner session v001 (prepared 2026-10-17, unanswered)

Session `9b6359b5e66fa4d761e1acb025c4697b`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261017-bpr1574-drawers-v001`
(a fresh short session), checkout this worktree, six candidates ready and
protected at sequence 18, revision `design-v001@10b3309fa`. Each embeds its
stills (today first, then each direction in Light and Dark) and a copyable
`open /Users/jaeger/.codex/worktrees/bpr1574-drawer-redesign/BetterPullReview/docs/design/BPR-1574-drawers-and-agent-v1/<page>.html`
for the clickable frames. The stills are kept beside the session at
`/Users/jaeger/.mask/attended-review/bpr1574-drawers-v001-media/`.

| Candidate | Question | Kind |
|---|---|---|
| `BPR-1574-principles` | Should every drawer and side surface follow these eight principles? | Accept or Reject |
| `BPR-1574-notes` | Which direction should the Notes drawer take? | A Margin notes · B Notebook · C Scratchpad |
| `BPR-1574-tours` | Which direction should the Tours drawer take? | A The route · B Stops at their lines · C Tour page and player |
| `BPR-1574-agent` | Where should the Agent live as a first-class surface? | A Agent modality (⌘5) · B Agent drawer tab · C Agent dock |
| `BPR-1574-compose` | Which layout should Compose review use? | A Focused · B What will be published · C Sheet over the code |
| `BPR-1574-overview` | Which direction should the Overview's side panel take? | A Attention first · B Summary, Checks, People tabs · C Status band and checks drawer |

Next: read `status --after 18`, record each verdict here and in
RFC-DRAFT-0023, revise the draft to the decided design, then hand it to
`implement-rfc`.
