---
schema: "implement-rfc.card.v1"
id: "BPR-2032"
title: "Share optional workflow configurations"
status: "Done"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC01","AC02"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0017","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0044-share-optional-workflow-configurations.md"}
depends_on: ["BPR-2016", "BPR-2017", "BPR-2022"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"dcb462ba1","evidence":{"kind":"command","command":"ctest --preset dev -R 'application_local_draft_workflow_desktop_public_path|application_local_draft_(desktop|tour|author|publication)_desktop_public_path|core_workflow'","detail":"With no workflow a draft prepares Description and Tour and the Workflow area says none is needed; importing the organization-shaped file leaves the open unrelated draft and its stored record without a workflow; the existing local-draft courses pass unchanged"}},{"criterion":"AC02","outcome":"pass","revision":"dcb462ba1","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_workflow|core_agent_configuration|qt_agent_configuration_dialogs|application_local_draft_workflow_desktop_public_path|sqlite_agent_configuration_storage'","detail":"Version-2 file refused as newer by older importers; unknown action named and refused; programs bound explicitly per action (relative refused), agents by the agent choices; preview marks a widened effect and starts an update chosen, compared yours beside the file's before Import (owner's accepted change, workflow-import-v001, session dea93294); import and selection run nothing (companion untouched) and builds stay off without acceptance; no credential field exists"}}]
---

# Share optional workflow configurations

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md), criteria `AC01`, `AC02`.

Named optional author configurations, recognized package extension, local bindings and explicit draft selection.

## Intended verification

Local Changes, Description and Tour remain usable with no extension, and imported organization-shaped settings execute nothing and preserve unrelated drafts. This card requires no PR creation; end-to-end built-in independence belongs to [BPR-2034](../60-done/BPR-2034-hand-off-effectful-actions-and-recover-their.md).

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Owner look workflow-v002 (2026-11-15)

Candidate `BPR-2032-workflow-import-preview` in session
`1d68e30d0ac7b53a07163a1f10777bdf`: **Rejected**, note verbatim: "Can we make
Agents & context just say Agents in the settings. Why does this window look
so different than everything else in the app? Why would leave out be the
default? We're trying to improt it? Why does it take so many actions to
choose between those 3 options? Why is it so hard to read what it does? We
need to send this window back for a front end design pass." The preview's
redesign belongs to a frontend-design pass and AC02's preview is judged
again under [BPR-2035](../10-backlog/BPR-2035-close-rfc-0112.md); the
Settings section is renamed "Agents".

## Design chosen: workflow-import-v001 (2026-11-15)

The redesign went to the owner as three mockups
(`docs/design/workflow-import-v1/`, branch `claude/workflow-import-mockups`
at `5a0551f91`): attended review `workflow-import-v001`, session
`dea93294643b9d7d9f3d64cec5d1e092`, state
`~/.attended-review-state/workflow-import-v001-2026-11-15/state`, decision
`7a40329d` at sequence 6: **selected "B. List and detail"**, no note. Its
host was stopped with the attended-review `stop` command and the design
merged into the v0.5.0 lane.

B is built as drawn: the Prompt library's shape, a ticked list at the left
and the selected item in full at the right, an update shown as yours beside
the file's; every item the file can bring starts chosen and Import is
enabled as the window opens; each item's choice is one segmented control.
The owner's accepted change to AC02's evidence: an update now starts
chosen, compared in the preview before Import is pressed, instead of being
taken "only when chosen". The rest of AC02 is unchanged: no program path
travels, an unknown action is named and its workflow refused, and builds
stay off without acceptance.
