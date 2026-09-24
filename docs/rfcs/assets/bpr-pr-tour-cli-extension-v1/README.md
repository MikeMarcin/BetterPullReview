# BetterPullReview — bpr tour CLI and pr-tour skill extension

**Extension v1 · 3 new RFCs · 20 implementation cards · 6 authored milestones · 32 acceptance requirements**

Add a separately usable terminal `bpr tour` frontend and a thin **pr-tour** agent skill to the previously imported review-tour pack. One shared tour engine, one same-machine WIP draft, and ordinary GitHub comments as the canonical published tour.

## Start here

Read [IMPLEMENTATION-PROMPT.md](IMPLEMENTATION-PROMPT.md), [decisions](supporting/DECISIONS.md), and the [source audit](supporting/SOURCE-AUDIT.md). Use [milestone-plan.json](milestone-plan.json) as the default execution seed; [MILESTONES.md](MILESTONES.md) describes the groups. This archive is additive to imported RFC0039–0043, not a replacement or re-import of that pack.

| Proposed RFC | Scope |
|---|---|
| [RFC-BPR-0044](repository-overlay/docs/rfcs/RFC-BPR-0044-headless-tour-runtime-and-terminal-cli.md) | Shared non-GUI runtime, manual terminal workflow, human/JSON commands, trusted editor integration, exact publication/recovery, standalone packaging. |
| [RFC-BPR-0045](repository-overlay/docs/rfcs/RFC-BPR-0045-shared-local-tour-drafts-and-cross-interface-recovery.md) | Same-machine WIP handoff with desktop open, one store/owner, coherent undo/revisions/conflicts, canonical GitHub recovery on a fresh machine. |
| [RFC-BPR-0046](repository-overlay/docs/rfcs/RFC-BPR-0046-thin-pr-tour-agent-skill.md) | Thin portable pr-tour skill, active-agent use of existing hooks, individual-stop interaction, real authorization, canonical shared-skill distribution. |

## What stays unchanged

The original five RFCs continue to own the problem-first preparation flow, GitHub-native index/comments and fallback placements, pending-review publication, post-publication maintenance and four optional writing hooks. Selection-scoped rewrites still replace text in place as one undoable edit. No required Apply/Dismiss panel, full-tour AI generation, batch-ready or automatic publication is added.

First-run local profile setup and explicit authentication are available from the CLI; no desktop bootstrap is required. Desktop calls shared C++ services directly. CLI composes the same services without a GUI; retaining Qt Core/Network is allowed. The skill calls the CLI and uses the current agent for explicitly requested prose work; it does not start another model or implement a second publisher.

## Where the tour lives

Ordinary WIP is local: editing, saving and switching desktop/CLI/skill does not upload it. The same profile's draft can move among the three interfaces without file copying, including CLI use while the desktop is open through a narrow authenticated owner channel.

Explicit publication can stage a GitHub PENDING review before COMMENT submission. That is remote staging, not full private-draft synchronization. A published tour's accessible content, order, source bindings and discussion are recovered from GitHub on a fresh machine. Private notes, undo history, unpublished edits, provenance and live publication authorization are not fabricated or uploaded to make that claim. See the [recovery matrix](supporting/RECOVERY-MATRIX.md).

## Included handoff material

The archive includes three additive RFCs, an authored milestone plan and 20 cards, [CLI contracts](specs/CLI-CONTRACT.md), [local handoff protocol](specs/LOCAL-HANDOFF.md), a concrete [pr-tour skill candidate](skill-source/pr-tour/SKILL.md) with progressive references, source/parent hashes, synthetic machine examples, security boundaries, acceptance matrices and native/website/agent courses.

The candidate skill is not installed here and requires the CLI this pack specifies. Its canonical destination follows the existing shared AgentSkills/MASK workflow; do not create multiple hand-maintained copies in BPR. Proposed RFC numbers 0044–0046 and card aliases require live collision checking/remapping. The source snapshot predates the user-reported base-pack import; intake resolves actual mappings and code once.

## Validate the pack

Python 3.10+, standard library only, from the extracted pack root:

```sh
python3 -B scripts/verify_pack.py
python3 -B -m unittest discover -s tests -p 'test_*.py' -v
```

The validator is human-readable by default; --json is explicit. It checks checksums, new-only scope, RFC/card/acceptance coverage, dependency/milestone order, skill identity/links, result fixtures and reference text-range cases. It does not contact GitHub, launch an agent, install a skill, compile BPR or validate the installed implement-rfc runtime.

## Evidence

This is a design/implementation handoff, not an implemented CLI or installed skill. All 32 product acceptance rows begin **Not run**. [PACK-VALIDATION.json](verification/PACK-VALIDATION.json) records artifact-only checks; native/platform/service/model and user acceptance are separate implementation obligations. The original source dump and full predecessor archive are not bundled.
