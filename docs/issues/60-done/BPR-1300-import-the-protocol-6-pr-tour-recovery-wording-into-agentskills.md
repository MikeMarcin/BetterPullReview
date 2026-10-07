---
schema: "implement-rfc.card.v1"
id: "BPR-1300"
title: "Import the protocol-6 pr-tour recovery wording into AgentSkills"
status: "Done"
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

## Owner answer (2026-10-06): push it

The coordinator asked in chat: "may I push the improved protocol-6 pr-tour
recovery wording to AgentSkills main (that repo's own card flow; only this
change, leaving its 17 unrelated local commits alone), then refresh it here
via MASK?" The owner answered, verbatim:

> Yes, push it

The answer covers that one push and no other AgentSkills change.

## Result (2026-10-06)

- **AgentSkills (AC1).** In a worktree on branch `bpr-1300-pr-tour-recovery`
  made from `origin/main` `b75119c`, BPR-1227's row (`03deabbe7`) was applied
  to `skills/pr-tour/references/recovery.md`. That one file and that one row
  are the whole commit, `e7470a7`, which was pushed to `main` as a fast-forward
  (`b75119c..e7470a7`; no force). Earlier imports (`16992cd`, `399da0d`) were
  also plain commits with no ASK card, and no card was added here. An ASK
  card would need a counter commit on AgentSkills `main`, which has the
  unpushed commits. The local `main` (17 unpushed commits, modified
  `.mask.lock.json`) was not touched. AgentSkills' `verify_documentation.py`
  already failed on `origin/main` before the edit (`NEXT_ID` ASK-0102 below
  the ASK-0106 high water that the local `main`'s reservations set). The
  edit does not change that.
- **Package and lock (AC2).** `packaging/agent-skills/pr-tour/` now takes
  the same row. `diff -r` against AgentSkills `e7470a7` `skills/pr-tour/` is
  clean. MASK upgraded `pr-tour` in a registered worktree, and
  `.mask.lock.json` changed only that entry: commit `b75119c` became `e7470a7`,
  and payload `sha256:44af3e0b…` became
  `sha256:be63ec2848ae70f1b27affec3fadd8350077de15b8dd11e8215b063621312d43`.
  `mask status --check` is healthy, with `claude` and `codex` deployed and
  target-verified. Both links' trees equal the package. The upgrade ran
  without `--dry-run` and without an approval hash, and MASK applied it
  directly, not as a plan to review. That happened in the disposable
  worktree, and the effect was exactly the intended entry.
- **Row (AC3).** The row ran at `82093f40b` on macOS 26.6.2 arm64 with
  `build/dev-tour-cli` (`-DBPR_BUILD_TOUR_CLI=ON`, `test_tour_cli_composition`
  built at that tree). `skill_pr_tour_package` passes under the
  network-isolated runner with the MASK links deployed. In a checkout of the
  same commit without links, `pr_tour_skill_tests.py` passes
  (`pr-tour-skill-tests:pass`). Control: a `.claude/skills/pr-tour` link to
  the old `b75119c` payload fails with ".claude/skills/pr-tour differs from
  the package; import it again".

After this change lands, any checkout whose links still point at `b75119c`
fails the row until `mask repair` moves them to the locked `e7470a7`. Linux
and Windows station runs are owed but do not gate this card:

```sh
git pull --rebase --autostash && mask repair && cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build build/dev-tour-cli && ctest --test-dir build/dev-tour-cli --output-on-failure -R "^skill_pr_tour_package$"
```

```powershell
git pull --rebase --autostash; mask repair; cmake --preset windows-dev -B build/windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build build/windows-dev-tour-cli --config Debug; ctest --test-dir build/windows-dev-tour-cli -C Debug --output-on-failure -R "^skill_pr_tour_package$"
```

On Windows the row is pinned `WINDOWS_UNCONTAINED` (BPR-1067), so it reads
the store outside the container.


## Windows station run (bpr-main-win, 2026-10-06)

At `f94614933`, `git pull` came first. Then `mask repair` re-linked
`.claude/skills/pr-tour` and `.agents/skills/pr-tour` to the new store
payload (`be63ec28…`), with no other change.

Both links are byte-identical to `packaging/agent-skills/pr-tour` (`diff -r`),
so the deployed `recovery.md` carries the protocol-6 source-read row.

`build/windows-dev-tour-cli` needed no rebuild: only `.mask.lock.json`,
the package and docs had changed since its last build at `766f2513a`. Under
the network-isolated runner, these pass 3 of 3:

- `skill_pr_tour_package` (`pr-tour-skill-tests:pass`), under its
  `WINDOWS_UNCONTAINED` pin from [[BPR-1067]];
- `skill_bpr_cycle_package`;
- `test_network_isolation_manifest`.
