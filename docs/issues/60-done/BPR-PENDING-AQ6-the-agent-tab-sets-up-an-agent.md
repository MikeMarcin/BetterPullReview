---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ6"
title: "The Agent tab sets up an agent in place, in every state of the frames"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ2", "BPR-PENDING-AQ3", "BPR-PENDING-AQ4", "BPR-PENDING-AQ5"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22 (owner answer Q1 A)","related_cards":["BPR-PENDING-AN","BPR-1594"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0092's design frames and again for this card's Qt build","requested_skill":"frontend-design","milestone":"RFC-BPR-0092 M6"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "e0807c4c4", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(qt_agent_setup_view|application_review_submission_composition_public_path|qt_settings_window|qt_workbench_window)$'; BPR_SETUP_RENDER_DIR=... better_pull_review_qt_agent_setup_view_tests (offscreen light and dark renders of none, two, nofolders, noclone, repo, check and signin)", "detail": "qt_agent_setup_view: the setup waits until agents were looked for and the link checked, and a saved agent with a serving link opens on the conversation (RFC AC11's new window); none found names what was looked for with Install and Use a program I choose as intents; two rows with the refused one disabled and none chosen; no clone folder, none found, two clones with none chosen until picked and Use this clone linking it; Check it works names claude auth status and its rows; once both checks pass the setup gives way once to Claude Code is ready with its version, and What the agent gets states the brief. application_review_submission_composition_public_path (the shipped composition, AC01): the first look at the Agent tab on a fixture with no agent shows the setup's Agent step with No agent found on this computer and no question field; nothing is asked so nothing runs. Renders reviewed offscreen in light and dark; the owner's look is BPR-PENDING-AQ8."}}, {"criterion": "AC2", "outcome": "pass", "revision": "e0807c4c4", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(qt_agent_setup_view|qt_settings_window|application_clone_link_host)$'", "detail": "Failed checks show their words (Claude Code isn't signed in with claude auth login to copy; GitHub could not be reached) with Check again, and no stable code; Stop is an intent while checking; Change repository folder... reopens that step. The tab offers Check again in place of Ask again when an Ask ends workspace_unavailable or failed_to_start. Settings rows: Codex and harness programs, protocol and Set up agent... as intents with a refused path's reason; clone folders and links listed with Remove and Forget naming what they act on; the review skill's Keep mine, Use version N, Restore default (with and without the Downloads copy) and Save as intents, a refused save keeping the text with its reason, a failed copy saying nothing was removed. A not-signed-in Ask has no outcome of its own, so it is not recognized (recorded on the card)."}}]
---

# BPR-PENDING-AQ6: The Agent tab sets up an agent in place, in every state of the frames

RFC-BPR-0092 M6 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)), owner answer Q1 A.

## Scope

- `qt::agent_setup_view`, in place of the conversation: Agent, Repository,
  Check it works and Ready, numbered, a finished step folded to one line
  with **Change**, **Look again** and **Agent settings** at the foot, every
  state of the [design frames](../../design/BPR-PENDING-AN-agent-setup-v1/index.html)
  in light and dark: agents found with their paths and how, a refused one
  disabled with why, none found with Install Claude Code, Install Codex and
  Use a program I choose…; no clone folder with Add a folder… and the usual
  folders that exist, none found, one or several clones with Use this clone,
  Choose a folder… and the private copy, a link that moved; the check's two
  rows, Check, Stop and Check again, and each failure's words with the sign-in
  command to copy. The setup shows once the agents were looked for and the
  repository's link checked, and a step is missing, and stays until both
  checks pass; a saved agent and a serving link open on the conversation.
- `qt::agent_tab_view::adopt_setup`: the `setting_up` state, History's **Set
  up agent…** and **Change repository folder…**, the ready line with **What
  the agent gets** and the question focused, and **Check again** on an Ask
  that ended `workspace_unavailable` or `failed_to_start`.
- The first time a window's Agent tab is shown, the setup host looks for
  agents (owner answer Q2); `clone_link_projection` gains `Loaded` and
  `Verifying` so the setup knows when the link is known.
- Settings › Agents & context: **Set up agent…**, Codex's program and protocol,
  a harness that reads plain text, and the review skill's editor with Save,
  Restore default (Downloads copy ticked), Compare, Use version N and Keep
  mine. Settings › Repositories: clone folders and linked clones with Forget.
- Each press is a request to the agent setup, clone link or review skill
  host; Qt holds the words and the open step only.

## Not done here

- **Clone into FOLDER** is not offered: cloning belongs to the RFC's
  `git worktree` form, whose replacement awaits the owner's confirmation
  ([[BPR-PENDING-AQ4]]).
- A not-signed-in Ask has no outcome of its own to recognize, so only
  `workspace_unavailable` and `failed_to_start` offer Check again.

## Acceptance criteria

- AC1: RFC AC01 and AC11 offscreen, light and dark.
- AC2: RFC AC12.
