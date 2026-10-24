---
schema: "implement-rfc.card.v1"
id: "BPR-1756"
title: "Verify and close RFC-BPR-0092"
status: "Done"
rfcs: ["RFC-BPR-0092"]
closeout_for: ["RFC-BPR-0092"]
depends_on: ["BPR-1748", "BPR-1749", "BPR-1750", "BPR-1751", "BPR-1752", "BPR-1753", "BPR-1754", "BPR-1755"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1747--BPR-1756--f06ad4b55890.json","origin":"RFC-BPR-0092 adoption, 2026-10-22","related_cards":["BPR-1747"],"milestone":"RFC-BPR-0092 closeout"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"270077e655d630ad8afa687046b6bb48bc4f0067","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root .","detail":"Every RFC-BPR-0092 criterion, AC01 to AC14, is recorded in the closeout checklist with its check, result and producing commit, and the checklist passes the gate's verify_closeout_card against the RFC's canonical criteria. The reconciliation's evidence at 08acf5b10 is reused: no RFC-BPR-0092 criterion uses the drawer layout repairs of BPR-1757. Its dependency RFC-BPR-0085 is Auto-Verified (BPR-1595), so RFC-BPR-0092 is set to Auto-Verified.","refs":["docs/issues/60-done/BPR-1595-close-rfc-0085.md"]}}]
---

# BPR-1756: Verify and close RFC-BPR-0092

The closeout card for [RFC-BPR-0092](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md):
AC01 to AC14 with the evidence of milestones M1 to M8.

## Acceptance criteria

- AC1: Every RFC-BPR-0092 criterion is recorded with its check, result and
  commit, and the RFC's status is set to match.

## Closeout checklist

RFC base: `2786b5b3afdb016aa79227b056b60a3f86b9fb5f`.

| Criterion | Direct check / evidence | Result at the reconciliation | Producing commit |
|---|---|---|---|
| **AC01:** With no agent chosen for the profile, opening the Agent tab on a github.com fixture shows the setup, not a conversation, in light and dark, and no process has started (launcher probe count unchanged). | `application_agent_setup_host` (test_detection_starts_nothing: no process request while rows, the choice and Searched publish); `qt_agent_setup_view` light and dark renders; BPR-1749 AC1, BPR-1753 AC1 | Pass: a profile with no agent opens on the setup in light and dark; the scripted launcher's request list stays empty until a gesture | `d005bb3ae`, `a7d0d3768` |
| **AC02:** Detection reports each permitted agent with its absolute path and how it was found, for fixture `PATH` and home folders holding none, one and both of `claude` and `codex`, and a saved wrapper; an explicit GHES host shows Codex disabled with its reason. | `core_agent_setup`, `core_agent_wrapper`, `application_agent_program_resolution`, `application_agent_setup_host`; BPR-1748 AC1 and AC2, BPR-1749 AC1 | Pass: rows carry each permitted agent's absolute path and how it was found for none, one and both and a saved wrapper; GHES keeps Codex visible and refused with its reason | `c3a561778`, `d005bb3ae` |
| **AC03:** With Codex found, a shipped-configuration Ask (no fixture executable override) starts the found absolute `codex`; with none found it ends with words naming where it looked, not a bare `failed_to_start`. | `application_agent_program_resolution` (the found codex, codex_not_found_message); `application_agent_finding_codex_found_contract` on the composed application; BPR-1748 AC2 | Pass: a shipped-configuration Ask starts the found absolute codex; with none found it ends naming where it looked | `c3a561778` |
| **AC04:** For fixture clone folders holding no clone, one clone, two clones, a clone matched by `upstream` only, a URL carrying user information, an SSH URL, a linked worktree, a symbolic link to a clone and a folder three levels down, the Repository step lists exactly the matches defined in Repository, in its order; the read set is only `.git/config` and `.git/HEAD` files, no process starts, nothing is written, and no user information appears in any store, log or label. | `core_clone_discovery`, the adapter's bounded `.git/config` and `.git/HEAD` reads, `application_clone_link_host`; BPR-1750 AC1 | Pass: every listed fixture case lists exactly the defined matches in order; only `.git/config` and `.git/HEAD` are read, nothing starts or is written, and user information is dropped | `860bd14f4` |
| **AC05:** A chosen clone is remembered per repository across windows and restarts, survives a repository rename, and a moved or no longer matching clone reopens the Repository step with its reason; Settings lists, changes and forgets links and edits clone folders. | `application_clone_link_host`, `sqlite_agent_setup_storage`, `qt_agent_setup_view` (Settings › Repositories); BPR-1750 AC2 | Pass: a link is remembered across hosts and restarts and kept across a rename by identity; a moved or re-pointed clone reopens Repository with its reason; Settings lists, changes and forgets links and edits folders | `860bd14f4`, `a7d0d3768` |
| **AC06:** On a fixture clone, a session's worktree is created detached at the exact head under the application's data root, with hooks, fsmonitor and filters off; the clone's branches, refs other than the worktree entry, index, stashes, configuration and checkout are byte-identical before and after; the worktree is removed when its session is superseded, when the review closes and past three per repository; a crash leftover is reclaimed; `git worktree prune` never runs. A head the clone lacks is fetched from the application's cache by local path on a complete review, and on a sparse review nothing is fetched and the step offers the command to copy. | `git_process_contract` (run_borrowed_history_workspace_contract); BPR-1751 AC1; the owner's acceptance of the borrowed-history workspace in place of a `git worktree` (session 89855cd96f098cd74e28d271acd60d8a Q2) | Pass as amended: a private workspace borrows the clone's objects through one alternates line at the exact head; every clone file is byte-identical before and after; no process runs in the clone and `git worktree prune` never runs; a head the clone lacks comes from the application's copy, nothing fetched on a sparse review | `f4ada43e5` |
| **AC07:** A new head leaves the old session's worktree at its head until the session ends, and **Start a new session** makes a new worktree at the new head. | Turn materialization and cleanup courses (test_revision_movement_discards_result_and_still_cleans and the composition courses); BPR-1751 AC2 | Pass as amended: each turn's workspace is made at its bound head and cleaned after its terminal; a new head's session gets its own | `f4ada43e5` |
| **AC08:** Every review request's supplement is version three with the brief's fields and bounds; a description or threads over their bound are cut and counted; no credential, token or environment value is present. | `core_review_skill`, `core_agent_context_supplement`; BPR-1752 AC1 | Pass: every review request's supplement is version three with the brief's fields and bounds; long descriptions and threads are cut and counted; no credential, token or environment value | `c3dda29b6` |
| **AC09:** The review skill: the default is the built-in `bpr-pr-review`; an edit creates the profile copy with its base version and digest; Save refuses a skill without `name` or `description` or over 64 KiB and keeps the text; Restore default saves a copy first; an update with a higher `bpr-version` offers Compare, Use version N and Keep mine and never changes an edited copy by itself; the skill's text is in every review request's brief and nothing is written to a harness's configuration. | `core_review_skill`, `application_review_skill_host`, `qt_settings_window` (the skill editor); BPR-1752 AC2; the owner's acceptance of the copy in `agent-setup.sqlite3` (session 89855cd96f098cd74e28d271acd60d8a Q3) | Pass as amended: built-in `bpr-pr-review` by default; an edit makes the copy with base version and digest; Save refuses without name or description or over 64 KiB and keeps the text; Restore default saves a copy first; a newer built-in offers Compare, Use version N and Keep mine; the text rides every brief and no harness configuration is written | `c3dda29b6`, `a7d0d3768` |
| **AC10:** One Check runs the agent check once through the supervised launcher with the profile's allowlist and no review context, then makes the worktree (or the private copy); both are Stop-able and bounded, and the passed, not-signed-in, worktree-failed, copy-failed and failed-to-start results each show their defined words and actions (fixture agents, a fixture clone, and a fixture cache missing the head's parent commit). | `application_agent_setup_host` (the status check through the supervised launcher, the allowlist, no context), the workspace probe (test_workspace_probe_makes_and_removes_a_workspace_without_a_session), `qt_agent_setup_view` (each result's words and actions, Stop); BPR-1749 AC2, BPR-1751 AC3, BPR-1753 AC2 | Pass: one Check runs the agent check once and then makes the workspace or private copy, both Stop-able and bounded; passed, not signed in, workspace failed, copy failed and failed to start each show their words and actions | `d005bb3ae`, `f4ada43e5`, `a7d0d3768` |
| **AC11:** After a passed check the tab shows the Ready line and the brief, and the question field has focus; a new window and a new review open on the saved agent and link without setup; Send to Agent offers the agent first. | `application_agent_setup_host` (test_choice_and_steps), `qt_agent_setup_view`, `qt_workbench_window_*` (Ready line and focus), `application_review_cycle_send_public_path` (Send to Agent offers the current agent first); BPR-1749 AC3, BPR-1753 AC1 | Pass: after a passed check the tab shows Ready and focuses the question field; a new window and review open on the saved agent and link without setup; Send to Agent offers that agent first | `d005bb3ae`, `a7d0d3768`, `d431745f2` |
| **AC12:** An Ask that ends `workspace_unavailable`, `failed_to_start` or `not_signed_in` shows its check's words, not the harness's, with that check's recovery and Check again; no stable code or Git output is visible until Show details. Removing the saved agent's executable leads the next Ask to **Set up again**; History and Settings open the setup with the current choices selected. | `qt_agent_setup_view` and `qt_workbench_window_*` (the check's words, Check again, Show details); BPR-1753 AC2; Reset agents in History and Settings (the owner's look v006 note) | Pass as amended by the owner: workspace_unavailable, failed_to_start and not_signed_in show the check's words with its recovery and Check again, codes only under Show details; a removed executable leads to Set up again; History and Settings offer Reset agents… in place of reopening the setup (look v006: "we can keep it if it's a reset agents which clears all settings and goes back to the wizard fresh") | `a7d0d3768`, `d431745f2` |
| **AC13:** No credential, token or environment value is read, stored, logged or shown by setup; the saved choices hold only identities, paths and remote names (store inspection and log scan). | `sqlite_agent_setup_storage` (the stored fields: agent names, programs, fixed arguments, models, efforts, folders, links by host, owner, name and remote name), `core_clone_discovery` (user information dropped), `application_agent_setup_host` (checks carry the allowlist only); BPR-1748 AC3, BPR-1749 AC2, BPR-1750 AC1 | Pass: setup reads, stores, logs and shows no credential, token or environment value; the saved choices hold identities, paths, arguments and remote names only | `c3a561778`, `d005bb3ae`, `860bd14f4` |
| **AC14:** The owner, attended, on a frozen build: "The first time I open the Agent tab it tells me what to do, and I end up asking a question that the agent answers from my repository." | Attended session `17312f2f1e95ac0107fa57bb5a50affe` on the frozen build of `d431745f2`, after looks v001 to v006; BPR-1755 AC1; the accept's two conditions verified in `qt_workbench_window_scale_100` | Pass: the owner accepted (decision 52f55363-457e-4397-ad30-d3962a4b80a6), "Otherwise we're done"; the conditions (suggestions hidden while the agent works, the Agent tab its name alone at full width) are built and tested | `d431745f2`, `08acf5b10` |

## Final closeout record, 2026-10-24

Closed. The one thing holding this card, its dependency RFC-BPR-0085,
is `Auto-Verified`: [[BPR-1595]] recorded its AC08 once
[[BPR-1757]] repaired the drawer surfaces that clipped. Every
RFC-BPR-0092 criterion keeps the evidence recorded at the reconciliation
below, at `08acf5b10`; the repairs since touch no setup, agent, clone or
review skill path that any criterion uses, and the workbench window rows
pass again at `270077e65`. RFC-BPR-0092 is set to `Auto-Verified`.

## Reconciliation, 2026-10-24

Every RFC-BPR-0092 criterion passes at `08acf5b10`: AC06, AC07 and AC09 as
the owner amended them in look v001 (the borrowed-history workspace and the
skill's copy in `agent-setup.sqlite3`), AC12 as the owner amended it in look
v006 (Reset agents… in place of reopening the setup), AC14 by the owner's
accept of look v007 with its two conditions built and tested. Every
milestone card, BPR-1748 to BPR-1755, is Done.

The RFC's status is not changed yet. RFC-BPR-0092 depends on RFC-BPR-0085
(the drawer design system), which is In Progress: its closeout,
[[BPR-1595]], is in the backlog: its closeout attempt on 2026-10-23
found AC08 unmet (drawer surfaces that clip at their narrowest and widest
widths), with every other criterion evidenced. The RFC index and the
README's rule ("A dependent RFC does not become `Auto-Verified` before every
listed dependency is `Auto-Verified`") hold RFC-BPR-0092 at In Progress until
RFC-BPR-0085 closes; this card is Blocked on that and closes it then with no
new evidence, or on the coordinator's decision to drop the dependency.

- **Checks at the reconciliation.** The documentation gate fails only on
  provisional card IDs; the closeout checklist passes the gate's own
  `verify_closeout_card` against the RFC's canonical criteria. The focused
  integration pass (agent, workbench at three scales, settings, composition
  contracts, architecture and command registry, review tour writing, Review
  Cycles Send, Markdown code, private notes, clone links and the review
  skill) and the core-only build and rows pass at `08acf5b10`.
- **Follow-ups.** Windows and Linux are unmeasured; every look was on macOS.
  Claude Code lists no models of its own, so its model menu is the current
  four by name; finding cards are not restored with a kept thread.
