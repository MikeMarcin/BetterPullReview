---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AN"
title: "Set up an agent the first time the Agent tab opens"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"kind":"design","origin":"Owner, 2026-10-22, relayed by the coordinator","owner_note":"I think we need some ux for setting up your agent the first time you go to the agent tab. Maybe a little wizard I'm not sure.","phase":"1 (investigation and design); no product code changed; awaits the owner's decisions","draft_rfc":"docs/rfcs/assets/agent-first-run-setup-v1/RFC-DRAFT-0026-agent-first-run-setup.md","draft_rfc_revision":"r2","design_frames":"docs/design/BPR-PENDING-AN-agent-setup-v1/index.html","frontend_design":"frontend-design:frontend-design loaded before the design frames","requested_skill":"frontend-design","owner_decisions":["D1","D2"],"superseded_sessions":[{"id":"29049098deb24f47c839771a46b05c4c","state":"/Users/jaeger/.mask/attended-review/owner-review-20261022-agent-setup-v001","status":"superseded by r2 with no answers; released and host stopped 2026-10-22"}],"related_cards":["BPR-1594","BPR-1684","BPR-1301","BPR-1375","BPR-1179","BPR-0348","BPR-0532"],"related_rfcs":["RFC-BPR-0011","RFC-BPR-0012","RFC-BPR-0060","RFC-BPR-0085","RFC-BPR-0003"]}
---

# BPR-PENDING-AN: Set up an agent the first time the Agent tab opens

## Owner request

The owner, 2026-10-22, verbatim:

> I think we need some ux for setting up your agent the first time you go to
> the agent tab. Maybe a little wizard I'm not sure.

## Phase 1 finding

The request is right, and there are defects under it. Observed on
`b69b82c9b`, rendered offscreen with a fresh profile, light and dark:

- The Agent tab opens on a conversation. Its "Set up an agent" empty state
  can only show when no profile is offered, which never happens on
  github.com or GHES.
- The agent menu is the four built-in profiles by protocol name, with
  "Codex App Server" selected first. Nothing is looked for until Ask.
- The first Ask ended "Codex App Server stopped before answering. Finished:
  failed_to_start." The Codex and external terminal profiles name a bare
  program, and only Claude's launch is resolved to an absolute path, so a
  shipped build can never start Codex (the Unix supervisor refuses a
  relative program; Windows passes it to `CreateProcessW` unsearched).
- The owner's own first run (2026-10-22) chose **External terminal
  harness** for a Claude wrapper and ended "Finished: workspace_unavailable"
  with a raw Git log: the review workspace could not be made from the cache
  (a missing parent commit, fixed separately), and no agent ran. The words
  blamed the harness, and the only actions were Ask again and the log.
- The agent works in a private copy filled from the application's full,
  filterless cache: the head's history is there while the cache is whole,
  but no branches, tags, remotes, LFS content or submodules. "Use my working
  copy" always refuses, because no shipped path sets the checkout it needs.
  Nothing links a repository to the reviewer's clone, and the agent is told
  identities, not the description, threads or a review method.
- The MCP helper needs no user setup, and `bpr` ships in no release package.

Details, sources and the proposal are in
[RFC-DRAFT-0026](RFC-DRAFT-0026-agent-first-run-setup.md) r2;
the frames, including the real renders, are in
[the design page](../../../design/BPR-PENDING-AN-agent-setup-v1/index.html).

## Proposal in one paragraph

The first time, the Agent tab is a four-step setup. **Agent** is found by
file metadata, shown with its path, and chosen for you when there is one.
**Repository** finds the reviewer's clone in the clone folders listed in
Settings, reading `.git/config` only, and remembers the choice per
repository; the private copy is the stated fallback. **Check it works** runs
the agent's status command and makes the agent's own detached worktree of
the clone at the head, each result in plain words with a recovery. **Ready**
is one green line, the review brief the agent gets, and the question field.
Worktrees never touch the reviewer's checkout or branches and go when their
session ends. Every review request carries the brief (description, base,
head, changed files, threads) and the editable review skill. Codex is found
as Claude is; the plain-text harness leaves the menu.

## Owner decisions

- **D1**, 2026-10-22, verbatim: "I think we ship it as a skill and make it
  editable by the user." The review workflow is the `bpr-pr-review` skill,
  built in and editable per profile, with newer defaults offered, not
  applied (RFC "The review skill").
- **D2**, same message: "We probably also give the user a setting to point to
  their folders where it can find existing repo clones." Settings ›
  Repositories › Clone folders (RFC "Repository").

## Decisions for the owner

Q1 to Q9 in the RFC's "Decisions for the owner", one per question, in a fresh
attended-review session (its URL is not recorded here). D1 and D2 are not
asked. The first session, `29049098…`, asked r1's Q1 to Q6, had no answers,
and was superseded by r2.

- Session `349862f0cb991e14b6cb57cf80653c5f`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261022-agent-setup-v002`,
  source this branch's worktree, loopback host started 2026-10-22, event
  cursor 27, nine candidates ready and protected, none answered.
- Candidates `BPR-PENDING-AN-q1-shape`, `-q2-detect`, `-q3-check`,
  `-q4-names`, `-q5-codex`, `-q6-scope`, `-q7-worktree`, `-q8-source` and
  `-q9-skill`, revision `agent-setup-v002@996f560ba`. Q1, Q3, Q8 and Q9 are
  named alternatives; the rest are Accept or Reject.
- The superseded first session `29049098deb24f47c839771a46b05c4c` (state
  `…/owner-review-20261022-agent-setup-v001`) has each candidate observed as
  superseded, all released, and its host stopped through the script.

## Continuation

Read the answers with
`attended_review.py --repo-root <this worktree> --state <state> status --after <cursor>`,
record each on this card and in RFC-DRAFT-0026, then adopt, revise or set
aside the RFC and allocate real IDs for this card and the plan's cards. The
offscreen renders came from a tests-on release build of `b69b82c9b` with a
temporary, uncommitted hook in `src/app/main.cpp`'s `--attended-agent-look`
(stop before Start, grab the tab and Settings); the stills are headless
Chrome renders of `still.html?still=ID&theme=light|dark` with a throwaway
browser profile.

## Plan after the answers

Estimated size: large, eight cards, about five to seven agent days.

1. Core setup model: state, rows, policy filtering, check decoding and
   failure words; a Codex locator and program path (Q4, Q5).
2. Clone discovery: the core's remote URL parsing and matching, the
   adapter's bounded `.git/config` reads, and the stored clone folders and
   links (D2).
3. Agent worktrees: getting the head (Q8), `worktree add` and `remove` with
   hooks, fsmonitor and filters off, ownership, the per-repository limit and
   crash reclamation (Q7).
4. The brief as context supplement version three, and the review skill:
   resource, profile copy, versions and delivery (D1, Q9).
5. Application setup host: detection when setup shows (Q2), the check (Q3),
   the saved agent, Send to Agent's default.
6. Qt setup in the Agent tab (Q1) with every state in the frames, light and
   dark, and the History entries.
7. Qt Settings: clone folders, linked clones, worktree usage and the review
   skill editor.
8. The AI agent boundary, Git and epoch model, Settings and workbench
   specification amendments; a frozen build and the owner's attended look
   (RFC AC14).

## Acceptance

- The owner's answers to Q1 to Q9 are recorded here and in the RFC, with D1
  and D2, and the RFC is adopted, revised or set aside to match.
