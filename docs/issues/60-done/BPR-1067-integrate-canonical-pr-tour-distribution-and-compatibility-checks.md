---
schema: "implement-rfc.card.v1"
id: "BPR-1067"
title: "Integrate canonical pr-tour distribution and compatibility checks"
status: "Done"
rfcs: ["RFC-BPR-0046"]
depends_on: ["BPR-1065"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-16","milestone":"pr-tour-skill","acceptance":["AC46-01"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC46-01","outcome":"pass","revision":"8924d9f1a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0044-0046.md","detail":"Package, directory and frontmatter name are exactly pr-tour; AgentSkills origin/main b75119c holds skills/pr-tour byte for byte the package (diff -r clean, tree digest a06be62c...), landed as 16992cd. mask install pr-tour (reviewed plan: .mask.json, .mask.lock.json, two links) selects it from agentskills; mask status --check healthy, claude and codex deployed and target-verified; .claude/skills/pr-tour and .agents/skills/pr-tour are MASK store links whose tree digest is a06be62c.... skill_pr_tour_package (network-deny sandbox, macOS arm64, tour_cli_composition at cd40f5f50's product tree) passes with the links in place and without them; the contract and capability checks run against the real CLI parser. Controls: the previous check fails on the MASK link ('a hand-maintained copy'); a real directory copy, a link the selection does not name and a link to an edited copy each fail. Claude Code listed pr-tour among its skills in the session that ran the install (host load observed for Claude Code only; Codex not observed). Linux and Windows runs of the row owed, not gating."}}]
---

# BPR-1067: Integrate canonical pr-tour distribution and compatibility checks

Pack alias: **PACK-PRTOUR-16** (live `BPR-1067`, allocated at import).

## Outcome

Install the skill through the actual shared AgentSkills/MASK source and record source revision, CLI compatibility and selected hosts. Do not overwrite unrelated skills or revive repository-local role implementations. Package a distributable thin skill separate from desktop dependencies under existing license/release policy.

## Existing ownership seams

Canonical shared AgentSkills skills/pr-tour; current MASK source/install metadata; BPR CLI distribution inventory.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Name/frontmatter/directory are pr-tour, required binary/version checks work and no divergent hand-maintained Claude/Codex copies exist. Absent authorized upstream access remains a named distribution blocker, not a fork or false install claim.

Acceptance ownership: AC46-01

## Execution boundary

Milestone: **pr-tour-skill**. Pack alias PACK-PRTOUR-16 was allocated as live `BPR-1067` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.

## Owner answer (2026-10-06): land it

The coordinator put the distribution question to the owner in chat: "BPR-1067:
the pr-tour skill is committed in AgentSkills on the unpushed branch
codex/pr-tour-skill. May I land it on AgentSkills main (via that repo's own
card and push)?" The owner answered, verbatim:

> Yes, land it

The answer covers that one push and no other AgentSkills change.

What was found before acting: there was nothing left to push. The local
branch `codex/pr-tour-skill` no longer exists. Its commit `16992cd12ff4` was
fast-forwarded into the AgentSkills `main` and pushed with it: the remote
ref's log shows `16992cd` as a pushed `origin/main`, and the `bpr-cycle`
imports `399da0d` and `b4a3357` that BPR-1170 pushed on 2026-10-03 sit on top
of it. AgentSkills `origin/main` (`b75119c`) holds `skills/pr-tour/`, its
README row and its `mask-source.json` entry, and its tree is byte for byte
`packaging/agent-skills/pr-tour/` (`diff -r` clean; digest
`sha256:a06be62c08c3fc4de65cdd834d8888e079d9a2ee29524c4ed3b72483083ae2a6`).
So no AgentSkills commit, card or push was made for this card. That
repository's local `main` (17 unpushed commits and a modified
`.mask.lock.json`, none of them this card's) was not touched.

What remains is this repository's side: select `pr-tour` from `agentskills`
through MASK, verify the deployment, and let `skill_pr_tour_package` accept
the MASK links it refused while the import was pending.

## Result (2026-10-06)

Promoted and claimed on the owner's answer. The selection and the row change
are `8924d9f1a`; no product source changed.

- **Source.** AgentSkills `origin/main` `b75119c` holds `skills/pr-tour/`,
  landed as `16992cd`, byte for byte `packaging/agent-skills/pr-tour/`
  (digest `a06be62c…`).
- **Selection.** `mask install pr-tour` was previewed, and the reviewed plan
  (`sha256:3156321f…`: the `.mask.json` entry, the `.mask.lock.json` entry at
  AgentSkills `b75119c`, payload `sha256:44af3e0b…`, and links for `claude`
  and `codex`) was applied to the main checkout. A first approval was refused
  `plan.stale` and previewed again; nothing was applied by it. `mask status
  --check` is healthy, both hosts `deployed` and target-verified. The two
  files were committed here with two `.gitignore` lines for the links.
- **Installed copy.** `.claude/skills/pr-tour` and `.agents/skills/pr-tour`
  link to the MASK store payload; both trees give digest `a06be62c…`.
- **Row.** `skill_pr_tour_package` refused any `pr-tour` under the skill
  trees while the import was pending, so with the links in place it failed ("a
  hand-maintained copy"). It now accepts a MASK link when `.mask.json` selects
  `pr-tour` from `agentskills` and the deployed tree's digest equals the
  package's, as `skill_bpr_cycle_package` does. Under the network-deny runner,
  with `tour_cli_composition` built at `cd40f5f50`'s product tree (macOS 26.6.2
  arm64, `build/dev` with `-DBPR_BUILD_TOUR_CLI=ON`), it passes in the main
  checkout with the links and in a checkout without them. A real directory
  under `.codex/skills`, a link the selection does not name, and a link to an
  edited copy each fail with their own message.
- **Hosts.** Selected for `claude` and `codex` (the project's default
  agents). Claude Code listed `pr-tour` among its skills in the session that
  ran the install. Codex loading it was not observed.

Owed, not gating: Linux and Windows runs of the row in a checkout where MASK
has deployed `pr-tour`. On Windows the contained row reads the MASK link as an
empty tree, as `skill_bpr_cycle_package` did on [[BPR-1185]]; if it stops
there, pin it `WINDOWS_UNCONTAINED` as `repository-content-outside-checkout`
with the measured line.

```sh
git pull --rebase --autostash && mask repair && cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON && cmake --build build/dev-tour-cli && ctest --test-dir build/dev-tour-cli --output-on-failure -R "^skill_pr_tour_package$|^skill_bpr_cycle_package$|^test_network_isolation_manifest$"
```

```powershell
git pull --rebase --autostash; mask repair; cmake --preset windows-dev -B build/windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON; cmake --build build/windows-dev-tour-cli --config Debug; ctest --test-dir build/windows-dev-tour-cli -C Debug --output-on-failure -R "^skill_pr_tour_package$|^skill_bpr_cycle_package$|^test_network_isolation_manifest$"
```

Unblocked: [[BPR-1068]], whose other dependencies (BPR-1059, 1062, 1063 and
1066) are Done. It asks for native Windows, macOS and Linux observations.

