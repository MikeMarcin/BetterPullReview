---
schema: "implement-rfc.card.v1"
id: "BPR-2035"
title: "Verify and close RFC-BPR-0112"
status: "Parked"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09"],"resolved_choices":["ACTIONS-01"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0020","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0047-close-optional-workflow-integration.md","draft_title":"Close optional workflow integration"}
depends_on: ["BPR-2032", "BPR-2033", "BPR-2034"]
closeout_for: ["RFC-BPR-0112"]
parked_on: "human-review"
---

# BPR-2035: Verify and close RFC-BPR-0112

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`.

Acceptance assessment for the optional extension RFC.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Judge every criterion with a synthetic integration and actual capability limits; no corporate account or proprietary skill is an acceptance prerequisite.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

`ACTIONS-01` was answered on 2026-11-10; see the [decision record](../../rfcs/assets/v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** (Amended 2026-11-12; see above.) With all extensions absent, local Changes, Description and Tour remain usable; selecting/importing an enterprise-shaped configuration does not make unrelated drafts or existing BPR features depend on it. | `application_local_draft_workflow_desktop_public_path`; the four local-draft desktop courses unchanged | Pass | `dcb462ba1` |
| **AC02:** A shared configuration binds local agents/skills/actions explicitly, previews conflicts and updates, and never executes or changes credentials merely through import or selection. | `core_workflow`, `qt_agent_configuration_dialogs`, the workflow course | Pass | `affe49d2d`, `dcb462ba1` |
| **AC03:** A fixture companion supplies context, proposals, repeatable checks and an input request; the author can pause, edit and resume without rerunning unrelated actions or accepting stale proposed edits. | The workflow course; `core_workflow` | Pass | `affe49d2d`, `dcb462ba1` |
| **AC04:** A changed upstream rule, missing helper, unsupported dry-run or required review marker produces an honest unmet requirement; BPR neither embeds those rules nor fabricates satisfaction or bypasses the chosen publisher. | The workflow course; `core_workflow` | Pass | `affe49d2d`, `dcb462ba1` |
| **AC05:** The selected publisher receives the exact accepted plan through an interactive handoff whose harness owns approval; BPR launches no supervised effectful action and does not misrepresent a declaration as runtime isolation or execution proof. | The workflow course; `core_workflow` | Pass | `affe49d2d`, `dcb462ba1` |
| **AC06:** Uncertain publication and failed ticket/visual/checklist follow-up retain actual results, reconcile before retry and cannot create duplicate external objects through automatic replay. | The workflow course; `core_workflow` | Pass | `affe49d2d`, `dcb462ba1` |
| **AC07:** A synthetic organization adapter qualifies the public preparation integration without creating a PR or requiring enterprise accounts/scripts, and the user can inspect origin, selected context, effect scope and current outcome in the native author workbench. | The workflow course and its renders; owner look `workflow-v001` | Pass on automation; owner look pending | `dcb462ba1`, `001c08ad1` |
| **AC08:** With extensions absent or unselected, an author completes the built-in journey from worktree selection through GitHub draft creation without enterprise tools, while optional integrations remain available to separately configured drafts. | `application_local_draft_publication_desktop_public_path`; RFC-BPR-0111 live #179 reused | Pass | `dcb462ba1` |
| **AC09:** An extension action bound to an agent wears the agent accent and one bound to a script does not; importing a workflow configuration turns on builds for no agent without acceptance; and no handoff result is labelled observed. | The workflow course; owner look `workflow-v001` | Pass on automation; owner look pending | `dcb462ba1` |

Ledger: [RFC-BPR-0112 verification](../../verification/RFC-BPR-0112.md).

## Owner look filed (2026-11-15)

Not waited on. Session `e9336c1caf748712f3c1056a633c4792`, state
`/Users/jaeger/.attended-review-state/workflow-v001-2026-11-15/state` (URL in
that directory's `serve.log`), cursor 9, host PID 51673 (Tailscale child
51681). Candidates `BPR-2033-workflow-area` (AC07), `BPR-2032-workflow-import-preview`
(AC02's preview) and `BPR-2033-agent-accent-on-actions` (AC09). Frozen root
`/Users/jaeger/workflow-v001.xlei1v22` from `fcdd020a2ee1` (executable SHA-256
`644e3c11b7c1…`; the launcher's own check reached
`ATTENDED_LOCAL_DRAFT_WORKFLOW_SHOWN`). Every criterion passes on automation;
RFC-BPR-0112 closes once the owner accepts, or the look's notes are
engineered.

## Owner look workflow-v001 handed back (2026-11-15)

Session `e9336c1caf748712f3c1056a633c4792` handed back at sequence 13.
`BPR-2032-workflow-import-preview`: **Rejected**, note "What the heck
happend to my status drawer?" with a screenshot of Overview's Status drawer
(kept at the session's attachments). `BPR-2033-workflow-area` and
`BPR-2033-agent-accent-on-actions`: unanswered. The Workflow area took over
the local draft's Status drawer: pickers, a paragraph per action and the
publish steps pushed below the fold.

Rework (coordinator): the drawer keeps its own content and layout; the
workflow is one compact section in it; one line per action (name, short
state, one small button) with the explanations in a tooltip or disclosure
and the run model said once; pickers show full names and collapse when
there is one choice; nothing, or one "No workflow" line, when unused. It is
assigned to a dedicated frontend-design worker. Host 51673 (Tailscale child
51681) stopped; root `workflow-v001.xlei1v22` removed with
`freeze_attended_build.py remove`. AC07 and AC09 stay pending until
`workflow-v002` asks all three questions again.

## Design rounds after workflow-v001 (2026-11-15)

Before engineering the rework, the author's checklist went to the owner as
two design rounds (merged into this lane from
`claude/local-draft-workflow-mockups` at `042e035a9`).

- **Round 1** (`docs/design/local-draft-workflow-v1/`), session
  `c5a05f972b30d5683c817e8ef18643b5`, state
  `~/.attended-review-state/local-draft-workflow-mockups-v001-2026-11-15/state`,
  decision at sequence 49: **selected "B, Workflow tab"**, with the note:

  > Sent back is too strong, maybe that happens, maybe i just decide to make
  > a change, maybe there's just a light optional nitpick to handle.
  >
  > Publish update is better than Publish response
  >
  > So on an open pull request I own, I get to this mode how? Is there a
  > button to switch between it and the normal review bench?
  >
  > I think I like workflow, it should become the first tab and become
  > active when we enter that mode. It's very close to A but doesn't confuse
  > the user by making status have different meanings across surfaces.

- **Round 2** (`docs/design/local-draft-workflow-v2/`: Revise as a fifth
  mode), session `273f0aafcc76fd284605b51649bbdbdf`, state
  `~/.attended-review-state/local-draft-workflow-mockups-v002-2026-11-15/state`,
  decision at sequence 7: **Accepted**, with the note "Just one feedback, we
  got rid of the notes tab so not sure why the mockup shows it." Revise's
  drawer has no Notes tab. All six recommended defaults in the round's README
  are accepted. Its host (PIDs 65438, 65447) was stopped with the
  attended-review `stop` command.

The accepted design is engineered as its own card (the author's mode,
Revise) before `workflow-v002` asks this card's questions again.
