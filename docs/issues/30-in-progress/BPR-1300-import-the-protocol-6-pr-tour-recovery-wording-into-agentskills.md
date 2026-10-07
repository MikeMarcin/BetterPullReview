---
schema: "implement-rfc.card.v1"
id: "BPR-1300"
title: "Import the protocol-6 pr-tour recovery wording into AgentSkills"
status: "In Progress"
rfcs: ["RFC-BPR-0046"]
depends_on: ["BPR-1067", "BPR-1227"]
metadata: {"kind": "shared-skill import", "allocated": "2026-10-06", "reservation": "docs/issues/_reservations/BPR-1300--BPR-1300--954512ccdc35.json", "requires_owner_authorization": "push to AgentSkills main"}
---

# Import the protocol-6 pr-tour recovery wording into AgentSkills

## Why this card exists

[[BPR-1227]] gave the tour owner channel protocol 6, which forwards
source-bearing tour commands to the desktop that holds the draft. With the
desktop open, `bpr tour add`, `show --source` and `walk` now read source for a
pull request open in it. BPR-1227 changed the `interaction_required` row of
the in-repo package's `references/recovery.md` to say so (`03deabbe7`):

> Give the next normal human action. With the desktop open, `add`,
> `show --source` and `walk` read source only for a pull request open in it;
> other provider reads and settings changes need the profile to themselves,
> and publication needs the author at a terminal. No simulated approval.

`packaging/agent-skills/pr-tour/` must equal the canonical AgentSkills
`skills/pr-tour/` that MASK deploys, and `skill_pr_tour_package` failed on the
difference once `mask repair` deployed `pr-tour`. The edit was reverted in
`5d34edf28` ([[BPR-1067]], BPR-1227's correction), so the package matches
AgentSkills `origin/main` `b75119c` again (checked 2026-10-06: `diff -r` of
`skills/pr-tour/` against the package is clean). The deployed row still says
provider reads need the profile to themselves. That is conservative, not wrong
in effect, because an agent sees `interaction_required` only when a read is
refused. No card owned bringing the wording over.

## Authorization

**The push to AgentSkills needs the owner's go-ahead.** It is a write to
another repository. BPR-1067's answer ("Yes, land it") covered one earlier
push and no other AgentSkills change, and nothing on this card authorizes
one. Ask the owner in one question before any AgentSkills commit is pushed.

## Outcome and bounded work

1. With the owner's go-ahead, change only the `interaction_required` row of
   AgentSkills `skills/pr-tour/references/recovery.md` to the wording above,
   on a branch made from AgentSkills `origin/main` through that repository's
   own workflow. The local AgentSkills `main` has 17 unpushed commits and a
   modified `.mask.lock.json` that are not this card's (BPR-1067). Do not push
   them or touch them.
2. After the AgentSkills commit is on its `origin/main`, apply the same row to
   `packaging/agent-skills/pr-tour/references/recovery.md` here, so the
   package equals the new canonical copy byte for byte.
3. Refresh this repository's MASK selection of `pr-tour` to that AgentSkills
   revision (preview, then apply the reviewed plan; `mask status --check`
   healthy), and commit `.mask.lock.json`.
4. Run `skill_pr_tour_package` against the deployed links and in a checkout
   without them. Record the AgentSkills revision, the payload digest and the
   result.

Out of scope: any other AgentSkills change, `bpr-cycle`, product source, and
the owner channel itself.

## Acceptance

- AC1: AgentSkills `origin/main` holds the protocol-6 `interaction_required`
  wording in `skills/pr-tour/references/recovery.md`, pushed with the owner's
  recorded go-ahead, and no other file changed in that commit.
- AC2: `packaging/agent-skills/pr-tour/` equals that AgentSkills
  `skills/pr-tour/` byte for byte, and this repository's MASK lock selects
  that revision.
- AC3: `skill_pr_tour_package` passes with the MASK links deployed and
  without them, at the committed revision.
