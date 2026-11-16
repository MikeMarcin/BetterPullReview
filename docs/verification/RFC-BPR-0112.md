# RFC-BPR-0112 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestones `workflow-companion-contract`
([[BPR-2032]], [[BPR-2033]]) and `workflow-effectful-actions` ([[BPR-2034]],
closeout [[BPR-2035]]). RFC_BASE_SHA: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.
Frontend-design was loaded before the Qt work. RFC-BPR-0109 is Rejected:
nothing here stores, imports or names evidence; a check's findings are
shown with its outcome as reported. ACTIONS-01 is implemented as settled:
interactive handoff, no BPR-supervised effectful action. Debug timings are
nongating; nothing below is a timing claim.

## What was built

| Layer | What |
|---|---|
| Core (`core/workflow_configuration.hpp`) | A workflow (ID, name, origin, guidance, up to 16 actions: slot, task, agent or command binding, declared effects, required); its `workflows` item list in the agent configuration file, which is written at version 2 when present so an earlier importer refuses it as newer; item problems (`unsupported`, `invalid`, `duplicate`, `agent_effect`, `unknown_agent`); the revision (SHA-256 of its canonical encoding); local bindings (an absolute program per command action, an agent name per agent action); `workflow_differences` (including `effects_widened`); `apply_workflow_choices` (all or none; replace keeps an unchanged program name's binding); `action_prerequisite`; `workflow_command` (whole-argument `{request}`/`{response}` only) and `shell_line` |
| Core (`core/workflow_action.hpp`) | The draft's pinned `draft_workflow` and action records; `select_workflow`, `accept_workflow_update`, `choose_publisher`; `begin_action` (an answer only continues a question); the bounded request (`bpr.workflow-request` v1) and response (`bpr.workflow-response` v1, 16 MiB, strict); `admit_workflow_response` (current invocation only; proposals bound to the field revisions the invocation read); `required_checks` (never run, pending, earlier candidate, earlier configuration, upstream changed, finding, limitation); `hand_off_publication`, `external_publication_open`, `reconcile_external_publication` (links through `adopt_local_pull_request` after a read), `hand_off_follow_up`, `reconcile_follow_up`, `mark_action_lost`. `local_publication_refusal::external_publisher` keeps the built-in route aside |
| Storage | `workflow_configurations` in the agent setup database, written in the import's transaction and compared for `changed_since`; the draft record's `workflow` member |
| Application | `export_agent_configuration` with workflows (their agents travel with them), `default_workflow_agents`, host `apply` with workflow choices; the workbench's workflow commands, its Workflow projection and `workflow_exchange_port`; `reconcile_external_local_publication` (reads the open pull requests from the draft's branch; writes nothing to GitHub) |
| Qt | `local_draft_workflow_area` in Overview's Status tab above the publish steps; Settings' export rows and import page for workflows. Agent actions wear the agent accent; command actions use the ordinary roles |
| App | `local_draft_workflow_exchange` (owner-private `workflow-exchange/DRAFT/INVOCATION/` files; starts nothing), `read_workflow_profile`, the desktop's dispatch and agent-bound Ask, the reload after a Settings import, `--attended-local-draft-workflow` for an attended look |
| Author ([[BPR-2196]]) | Core `author_mode` decides whether Author is offered, the opening mode, the handoff and the checklist (`core_author_mode`); Qt's fifth mode on Ctrl+5 with This update, the file pane (Edit, one pane, two panes) and a drawer opening on Workflow, the checklist that replaced the rejected Workflow area; the host's `set_pull_request_author_case` from the draft-state reading |
| Fixture | `tests/fixtures/workflow_companion`: the synthetic organization companion, its upstream tool (rules, a callable helper, the self-review marker) and the shared configuration |

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `application_local_draft_workflow_desktop_public_path`: with no workflow, Description and Tour prepare and the area says none is needed; after the organization's file is imported, the open unrelated draft and its stored record have no workflow. `application_local_draft_{desktop,tour,author,publication}_desktop_public_path` pass unchanged | Pass |
| AC02 | `core_workflow` (version 2, unknown action named and refused, relative program refused, replace keeps a binding, leave changes nothing); `core_agent_configuration` (`plan_configuration_import`: each item New, Updated, Unchanged or refused and its starting choice; builds start unaccepted); `qt_agent_configuration_dialogs` (the preview as the owner's design B: a ticked list and the item in full, everything chosen and Import enabled as it opens, one click per choice, an update shown as yours beside the file's with a new action and a widened effect marked before Import, no program path in the file, one program chosen for every action naming it, a refused workflow named; the panel is the Prompts page's in Dark, Light and both high-contrast themes); the course (import and selection run nothing, builds stay off without acceptance). An update now starts chosen: the owner's accepted change (workflow-import-v001, session `dea93294`) | Pass; owner accepted in look `workflow-import-v003` |
| AC03 | Course: context asks the author, a check runs meanwhile, Continue reruns only the asking action, the proposal on a field edited since is stale and Accept refuses it; `core_workflow`: an earlier invocation's answer is refused | Pass |
| AC04 | Course: the upstream-only review marker and a changed upstream rule leave the required check unmet and its publisher unhanded; the companion's own rules and validator are outside BPR. `core_workflow`: missing helper and no dry run are limitations | Pass |
| AC05 | Course: the workflow's publisher chosen, the built-in creation refuses and GitHub sees none; the exact plan (digest, title, body) reaches the publisher; BPR names the command and starts nothing; the area says the program is neither started nor restricted by BPR | Pass |
| AC06 | Course: lost answer, no handoff before Check GitHub, nothing found reopens under a new confirmation, the report links only after the read (as not created by BPR); a partly failed follow-up keeps its results, is retried only after reconciliation and makes only what is missing | Pass |
| AC07 | Course and renders (`build/dev/tests/end_to_end/local-draft-workflow-desktop/`): origin, the selected context a handoff sent, who acts, declared effect scope and current outcome in Author's Workflow tab (BPR-2196; Status keeps its one meaning and a local draft has none until its pull request exists); no PR created and no enterprise account or script. `qt_workbench_window_scale_100`: Author offered only to the owner, Ctrl+5, the opening mode per case. Owner look `workflow-v002` rejected the Workflow tab for panning sideways; its rows, buttons, command lines, names and footer now wrap or shorten inside the drawer, and `qt_drawer_surfaces_fit_scale_100/125/200` holds every drawer tab (Author's Workflow on a pull request and on a local draft included) to never panning: 8 states failed per scale before the fix, none after | Pass on automation (the drawer fit guard); the reworked Workflow tab had no separate owner look |
| AC08 | `application_local_draft_publication_desktop_public_path` (worktree to linked draft pull request with no workflow, Workflow area composed); the workflow course keeps an unrelated draft free of the integration. RFC-BPR-0111's live BPR-Test run (#179) is reused: the built-in route changed only by a guard a draft without a workflow never meets | Pass |
| AC09 | Course: Ask (agent action) is an agent run control, Hand off is not; the file's builds request is off without acceptance; results read "Reported by the companion; not observed by BetterPullReview", and a companion's row is marked outlined Reported, never filled (`core_author_mode`, the course) | Pass; owner accepted in look `workflow-v002` |

## Not done, and why

- An in-app editor for defining a workflow: Settings defines workflows by
  import, as the shared file is their definition (design question 1 in the
  handoff).
- A remembered workflow per repository (the RFC's "may offer"): not built.
- `shell_line` quotes for a POSIX shell; Windows has no course (the course is
  `NOT WIN32`, as the companion is a script with a shebang).

## Owner looks of the import preview

- `workflow-v002` (session `1d68e30d`): the first preview **Rejected** ("Why
  does this window look so different than everything else in the app? ...").
- `workflow-import-v001` (session `dea93294`): three designs; the owner
  **chose B, List and detail**, no note.
- `workflow-import-v002` (session `2f70e593`): B as built **Rejected**:
  "Super ugly. Something went way wrong with the colors." The dialog drew
  with the system palette instead of its opener's.
- `workflow-import-v003` (session `5052733e`, decision `09e243f2`):
  **Accepted**, note verbatim: "Still not quite as nice as what had settled
  on but its pretty close." The remaining gaps against the mockup are carded
  as [[BPR-2204]].

## Closeout state

Every criterion has direct engineering evidence. The owner accepted the
import preview (AC02, `workflow-import-v003`), the agent accent (AC09,
`workflow-v002`) and Author ([[BPR-2196]]). AC07's Workflow tab was rejected
in `workflow-v002` for panning sideways; the rework is held by
`qt_drawer_surfaces_fit_scale_100/125/200` and had no separate owner look.
**RFC-BPR-0112 is closed** (Auto-Verified, 2026-11-15) by [[BPR-2035]].
