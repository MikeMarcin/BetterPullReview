# Inspected current behavior and proposal seams

> Carried into the 0.5.0 targets pack on 2026-11-10 from the local PR drafts pack, version 1. RFC and card numbers were mapped on the way in (RFC-DRAFT-0001 to 0006 became 0010 to 0015; DRAFT-0001 to 0020 became 0028 to 0047). The pack as it was is at `/Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1`.

Observed checkout: `b12269ea9ad1ff07afe8ec31812875b89d870e5a`, re-observed on 2026-11-10. The pack was first written against `154d307d765a87917c805fe1d4ac024eef0fd6c5`, 344 commits earlier; every anchor below was located again, S7 and S14 were corrected for what changed in between, and S15–S18 were added. These are source/document inspections, not runtime tests or implementation acceptance. Repository paths below are relative to the BetterPullReview checkout; its source was not copied wholesale into this portable pack. A reviewer with that checkout can inspect the named commit. File digests and excerpt locations are recorded in [source observations](repository-observations.json).

## S1 — Custom agents are local definitions with bounded identities and explicit execution settings.

- `src/core/include/bpr/core/agent_entry.hpp:21` — `maximum_agent_entries`.
- `src/core/include/bpr/core/agent_entry.hpp:38` — `struct agent_entry`.
- `src/core/include/bpr/core/agent_entry.hpp:63` — `StrictRunChecks`.
- `src/application/include/bpr/application/agent_setup_preferences.hpp:17` — `struct agent_setup_preferences`.

Agents are not portable merely by copying their numeric IDs or executable paths. Import/export needs an explicit mapping step. Literal arguments remain user text even though the record has no dedicated credential field.

## S2 — Existing settings hold editable review and tour guidance; selective exchange is a proposed new surface.

- `docs/development/settings.md:122` — `### Review preparation writing hooks`.
- `src/adapters/qt/src/agent_setup_settings.cpp:552` — `review_skill_panel::review_skill_panel`.
- `src/adapters/sqlite/src/agent_setup_store.cpp:491` — `class review_skill_store`.
- `docs/rfcs/RFC-BPR-0092-agent-first-run-setup.md:1194` — `### Amendment [[BPR-1916]]`.

The inspected Settings/architecture surfaces establish editing and persistence, not a current portable import/export path. The six existing prompt purposes are the bounded first-slice target: the review skill and the five writing hooks (S18). The Settings page is titled Agents & context; `docs/development/settings.md` still heads the hooks "AI assistance > Writing instructions".

## S3 — Skill references do not grant execution capability.

- `docs/architecture/ai-agent-boundary.md:1841` — `### Skills a prompt names`.
- `src/core/include/bpr/core/skill_references.hpp:40` — `maximum_referenced_skills`.

The documented named-skill mechanism reads bounded SKILL.md text and names supporting files; it does not load arbitrary packages or widen writing/read-only permissions. An enterprise skill needing scripts/references is not made executable merely by naming it in a prompt.

## S4 — Review Cycles is a separate local review domain.

- `docs/rfcs/RFC-BPR-0053-review-cycles-domain-and-local-authority.md:25` — `Introduce Review Cycles as a first-class`.
- `docs/ux/review-cycles-workflow.md:11` — `## How a cycle works`.
- `docs/rfcs/RFC-BPR-0061-native-review-cycles-workbench-and-usability.md:18` — `Full source is the initial`.

It captures source, freezes feedback rounds and retains reviewer judgments without a PR/account/agent. Its source-first entry and baseline are not a prospective branch diff. The new local PR draft must not redefine existing cycles by renaming them.

## S5 — Captured material and Git publication objects are distinct.

- `docs/rfcs/RFC-BPR-0054-workspace-source-capture-and-revision-material.md:28` — `Provide source material directly`.
- `src/core/include/bpr/core/review_cycle_workspace.hpp:127` — `struct workspace_import_mapping`.
- `docs/rfcs/assets/bpr-review-cycles-v1/specs/PUBLICATION.md:18` — `## Three different baselines`.

Capture includes dirty bytes and preserves explicit workspace identity and quality. Initial local source, final capture and publication parent/target cannot be equated. Existing filesystem-capture ownership does not permit Git effects in that adapter.

## S6 — Current tours require real PR and commit identities.

- `src/core/include/bpr/core/review_tour.hpp:22` — `struct tour_owner`.
- `src/core/include/bpr/core/review_tour.hpp:36` — `struct tour_source_range`.
- `src/core/include/bpr/core/review_tour.hpp:15` — `maximum_tour_stops`.

Profile, host, authority, account, repository and PR own the current tour. Source ranges include commit identity. Local/dirty authoring requires an explicit new ownership/material seam, not sentinel IDs.

## S7 — Description ownership and conversation durability need deliberate treatment.

- `docs/development/tour-cli.md:202` — `## Publication and read-first recovery`.
- `docs/architecture/ai-agent-boundary.md:1837` — `The conversation is replayed`.
- `docs/architecture/review-tours.md:169` — `Since [[BPR-1900]]`.

Tour publication normally updates Problem/Solution in the PR body and has a no-description route. Pre-PR preparation must preserve an externally formatted full body.

The second anchor is stale documentation. That paragraph still says a tour conversation is replayed and held only in memory, which was true at the first observed revision. RFC-BPR-0096 has since made tour chats durable native sessions (S16); replay survives only as a labelled fallback for an agent that cannot resume.

## S8 — Native local-cycle publication is a new-branch squash, with draft support and separate association.

- `docs/rfcs/RFC-BPR-0059-clean-pr-publication-and-cycle-continuity.md:16` — `The native v1 output`.
- `src/adapters/qt/src/review_cycle_publish_sheet.cpp:360` — `Open as a draft`.
- `src/core/include/bpr/core/review_cycle_publication.hpp:62` — `struct publication_plan`.
- `src/application/include/bpr/application/review_cycle_publishing.hpp:34` — `Linking an externally published`.

The current finite native path does not preserve an arbitrary existing branch history or provide a general authenticated push. It is not evidence of native GHES publication. Its exact-plan/recovery and external-association principles are reusable.

## S9 — Existing verification is an explicit reviewer judgment, not an executed-test record.

- `docs/architecture/defect-workflow.md:11` — `A fix claim is not a verification`.
- `docs/ux/review-cycles-workflow.md:30` — `- **Replies**`.
- `docs/rfcs/RFC-BPR-0012-ai-review-chat-findings-mcp.md:44` — `## Structured findings`.

Author/agent replies and evidence do not automatically become verified. The proposed evidence domain therefore records provenance and check scope separately from reviewer verification or PR approval.

## S10 — The enterprise integration is described, not independently inspected.

Source: the enterprise overview, kept outside this repository ([provenance](PROVENANCE.json)), especially its sections 4 and 7.


The preserved overview is the only supplied enterprise source. Its section 7 reports reusable helpers, a shared validation library without a CLI and a publisher with no dry-run. The primary-clone and visuals-commit requirements make an unmodified one-shot invocation unsuitable as a pretend prepare-only hook. Section 4 also describes the usual isolated worktree as a local clone whose `origin` is the primary clone, and requires the branch to be pushed from that worktree so pre-push hooks check it.

## S11 — Home currently gives Review Cycles its own visible section.

- `src/adapters/qt/src/workbench_window_central_surface.cpp:2019` — `cyclesTitle`.
- `src/adapters/qt/src/workbench_window_queue_controls_and_menus.cpp:1481` — `OpenProjectAction = fileMenu`.
- `src/adapters/qt/src/workbench_window_home_landing.cpp:735` — `present_local_cycle_recents`.

The existing card, recents and File menu entry can be distinguished. The proposal changes Home prominence while retaining the old subject and a reliable route to it.

## S12 — Architecture and qualification constraints remain in force.

- `docs/architecture/product-contract.md:129` — `## Application architecture invariant`.
- `docs/architecture/system-architecture.md:1951` — `## Testing contract`.
- `src/adapters/qt/AGENTS.md:37` — `Inherit the application and operating-system palette`.

The core decides, the host performs, Qt presents. New operations need narrow host ports and honest native qualification. Importing this draft later would not authorize hand-written infrastructure or new core dependencies.

## S13 — Installed Git is isolated local plumbing; native publication uses REST.

- `src/adapters/git/include/bpr/adapters/git/installed_git_publication.hpp:18` — `no credential helper`.
- `docs/rfcs/RFC-BPR-0059-clean-pr-publication-and-cycle-continuity.md:20` — `the credential-free installed Git adapter remains local plumbing`.

The publication runner has its own home and configuration, empty templates and hooks, no credential helper and no protocol. Native publication writes Git-data objects through admitted HTTPS authority. A native push of the author's branch would cross this boundary; a REST replay runs none of the repository's hooks. Added by the secondary review at the same pinned revision.

## S14 — Tour agents read the review's workspace; more prompt purposes are planned.

- `docs/architecture/review-tours.md:176` — `workspace with read-only tools`.
- `docs/architecture/ai-agent-boundary.md:1827` — `is a read-only run on the`.
- `docs/issues/60-done/BPR-1934-tour-chat-suggested-prompts.md:35` — `Agent settings can later make them editable.`

Draft Tour and ask turns run read-only tools in the review's workspace. For a pull request that is a private workspace pinned to the review's commits and isolated from the active worktree (S16), not the author's checkout; an earlier revision of this pack read it as the live worktree. A local draft has no such pinned workspace yet, so the proposed local-authoring contract materializes one from the captured candidate. BPR-1934 is done: tour chats offer suggested prompts kept in one place in the app layer, and Settings does not yet make them editable, so the sharing format may later meet purposes it does not know. Added by the secondary review; corrected on re-observation.

## S15 — An agent entry's identity owns its conversations and changes with what it runs.

- `src/core/include/bpr/core/agent_entry.hpp:65` — `std::string Identity;`.
- `src/application/src/agent_setup_host.cpp:532` — `aEntry.Identity = original->Protocol == aEntry.Protocol`.
- `docs/rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md:219` — `A replaced/reset agent entry cannot acquire a conversation`.

Each entry has a host-minted identity besides its numeric slot. Editing an entry keeps that identity only while its protocol, program, fixed arguments and strict-run setting are unchanged; any other edit to those mints a new one, and retained native conversations do not follow it. Name, model, effort and check arguments do not affect it. An import that replaces an entry therefore decides whether that agent's conversations can be continued. Added on re-observation.

## S16 — Agent conversations are durable native sessions on private pinned workspaces.

- `docs/rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md:223` — `### Session scope, workspace and history`.
- `docs/architecture/review-tours.md:687` — `## Retained native tour conversations (RFC-BPR-0096)`.
- `docs/architecture/ai-agent-boundary.md:184` — `## Workspaces`.
- `docs/architecture/ai-agent-boundary.md:789` — `### The dangerous workspace mode needs a choice, not a setting`.

RFC-BPR-0096 is adopted (status Auto-Verified, with an owner waiver for installed-Claude verification). Agent-tab and tour-chat conversations are the agent's own sessions, scoped by profile/account authority, pull request, tour thread and agent entry. History, bindings and resume handles survive restart; an idle session needs no process; New session keeps the old one in history; a labelled transcript replay serves agents that cannot resume. One private workspace per pull request and base/head pair is materialized from pinned commits, retained and shared across that pull request's conversations, with no remote and no write to the reviewer's clone. A changed base or head uses another source binding and a new session. Tour connections stay read-only; Agent-tab sessions keep the agent's own approvals and may edit that private tree. Running in an existing checkout needs an explicit choice for each invocation. None of this has a scope for a subject without an account and pull request, and the materialization starts from commits, not captured uncommitted bytes. Added on re-observation.

## S17 — Clone remotes are already read under bounds; the CLI has conventions.

- `src/core/include/bpr/core/clone_discovery.hpp:15` — `maximum_clone_config_bytes`.
- `src/core/include/bpr/core/clone_discovery.hpp:48` — `parse_git_config_remotes`.
- `docs/development/cycle-cli.md:154` — `## Working while the desktop is open`.
- `docs/development/tour-cli.md:338` — `## Working while the desktop is open`.

Core reads the remotes of one `.git/config` within 64 KiB, 64 remotes and 1024 bytes per name or URL, and follows no include. It resolves a hosted location from a URL; it does not follow a local file-path remote to another clone, which is the new step the local-draft entry proposes. `bpr tour` and `bpr cycle` define how a command behaves while a desktop owns the profile. Added on re-observation.

## S18 — Glaze is the approved JSON library; prompts have exact bounds.

- `docs/architecture/system-architecture.md:688` — `Approved pinned libraries are glaze`.
- `src/core/include/bpr/core/review_preparation.hpp:16` — `enum class writing_hook`.
- `src/core/include/bpr/core/review_preparation.hpp:24` — `maximum_writing_instruction_bytes`.
- `src/core/include/bpr/core/review_skill.hpp:17` — `maximum_review_skill_bytes`.

The core implementation inventory lists glaze among the approved pinned libraries, so a new JSON exchange format needs no dependency decision. The five writing hooks are Problem, Solution, outline, stop comment and review feedback, each at most 8 KiB; an empty override selects the shipped text. The review skill is at most 64 KiB. Added on re-observation.

## Observed documentation limits

Some overview prose still describes early Review Cycles RFCs as Proposed, while their owning RFC/card records show later closeouts. This pack uses current source shapes and owning contracts for behavior and does not copy an overview status into new acceptance. No existing status or historical evidence was changed.
