---
id: "RFC-BPR-0112"
title: "Optional shareable workflow extensions and companion skills"
status: "Proposed"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0006","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
depends_on: ["RFC-BPR-0107", "RFC-BPR-0108", "RFC-BPR-0109", "RFC-BPR-0110", "RFC-BPR-0111"]
---

# Optional shareable workflow extensions and companion skills

## Problem and grounded current behavior

The user's enterprise create-PR skill combines classification, ticket work,
self-review, formatting, validation, push/publication and follow-up. The user
does not control it and prefers a companion skill over maintaining a fork.
According to the supplied overview, some helpers are callable separately, the
shared validator has no CLI, and the creation script has no dry-run. These are
reported facts about an uninspected enterprise implementation, not verified
capabilities. The overview and a compatibility analysis built on it are
kept outside this repository; their location and digest are in
[provenance](assets/v0-5-0-prerelease-targets-v1/local-drafts/PROVENANCE.json).

BPR already supports custom agents and skill references in prompts, but those
references do not widen a writing/read-only run's permissions. A prompt alone
does not supply a reliable external-operation lifecycle. See [S1–S3 and
S10](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md).

## Proposed behavior and boundaries

### Complete defaults and optional configurations

The built-in local PR draft, evidence, description, tour and native publication
flow remain usable with no enterprise configuration. Add named author-workflow
configurations that can select prompt guidance, map agent roles and bind a small
set of extension actions. Selecting a configuration for a draft is explicit;
repository associations may offer a remembered choice but do not activate
untrusted repository instructions merely because a folder was opened.

The early companion milestone establishes independence of local preparation
(Changes, Evidence, Description and Tour). The later effectful milestone checks
the complete built-in journey through draft PR creation, after native publication
exists. The RFC's publication dependency applies to that later work and closeout;
it does not make publication a prerequisite for the early preparation contract.

Extend the first RFC's selective import/export with recognized workflow items
and dependencies. Do not make an older importer silently accept unknown action
items as inert successful configuration: offer a clearly identified supported
subset or refuse the unsupported selection. Programs, credentials and local
paths remain machine bindings; skills and enterprise scripts are referenced,
not redistributed. A configuration update is compared and accepted; it cannot
silently change a running or saved publication plan.

Settings owns definition, import/export and prerequisite inspection. The draft
owns its selected configuration revision and the status of its actions. Display
ordinary author tasks, not callback names. Missing optional tools disable only
their dependent actions and preserve local work. A required selected check that
is unavailable remains unmet; do not silently fall back to native publication
and imply organizational requirements were satisfied.

### Prompts and actions have different contracts

Prompts guide writing and investigation within the selected run's existing
capabilities. Actions have declared inputs, effects, cancellation and results.
The initial action slots are gather context, propose description/checklist,
check preparation, publish draft, and finish publication. A configuration need
not implement all slots, and slots do not force a one-shot sequence. Evidence
production can be requested through a check/investigation action and imported
through the evidence contract.

An invocation identifies the local draft and expected revision, exact candidate
and source/target observations, selected artifacts/context, selected configuration
revision and action identity. The return can contain a proposal, check findings,
an input request, observed external references, a completed result, failure,
interruption or uncertainty. A check and its authority are not inferred from
free-form confidence. Apply returned edits only as accepted current proposals;
do not overwrite later author changes. A blocked check prevents its dependent
publication, not editing or unrelated investigation.

Bounded versioned request/response envelopes are a BPR-facing companion contract,
not a requirement that the enterprise skill itself change. Use explicit file or
stdio inputs and argument arrays through configured executables/harnesses;
never interpolate PR text into a shell command. Reuse the 16 MiB external-result
admission ceiling from the evidence flow. Plain terminal/human handoff remains
available when a tool cannot return machine-readable results, with its outcome
reported rather than falsely attested by BPR.

### Effects and authorization

Reading selected context, writing proposals, executing workspace checks, changing
a ticket and publishing a PR are distinct effect classes. Importing or selecting
a configuration authorizes none of them. The author invokes an action with the
relevant scope and receives specific progress and recoverable outcomes. External
tools use their own authentication; no hook receives BPR vault credentials.

Do not describe a configured action as read-only merely because its manifest
says so. A runtime must actually support the admitted restrictions, or the
action is an explicitly effect-capable external handoff under its real harness
permissions. BPR's own parser, current-state checks and selected-context boundary
are not an operating-system sandbox for an arbitrary same-user executable.
Current tool-free/read-only writing lanes retain their restrictions.

**ACTIONS-01 is settled.** On 2026-11-10 the owner chose the interactive
handoff for the first effect-capable extension release: BPR hands an effectful
action to the configured interactive harness or terminal, whose existing
approval interaction owns execution, and takes structured import/status back
where the tool supports it. BPR launches no supervised effectful action in this
version.

The paragraph below describes Agent-tab sessions as they were when this
choice was made. Its account is re-read once
[RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md)
lands; see the fitted rules at the end of this section.

The alternative was a BPR-supervised action launched after a BPR scope
confirmation. Tour sessions are read-only and reapply that restriction on every
connection. Agent-tab native sessions do carry the agent's own permitted
approvals, but they act in a private workspace or, only by an explicit choice
for each invocation, in an existing checkout
([S16](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md)); neither is a BPR-scoped route for an
arbitrary configured action in the author's worktree. Supervised execution
would extend that session contract and needs its own qualified contract in a
later RFC.

Only a BPR-supervised action can produce an observed execution in the evidence
RFC's sense. An interactive handoff returns reported results, however structured
its output, so version-one evidence stays reported.

Publication identifies exactly one publisher for the
attempt: the built-in implementation or the external integration. Before effectful
handoff, validate current accepted input and preserve the finite publication
plan. The result is associated only after checking actual host/repository/PR and
candidate observations. An external lost answer remains uncertain and uses
read/status reconciliation; an unqueryable result stays unresolved. No automatic
replay of ticket creation, pushes or PR creation occurs after restart.

### Enterprise compatibility without enterprise code in BPR

A separately maintained companion skill can read the installed upstream skill,
reuse supported helpers, consult its current references, propose material into
BPR, and call its final publisher with the accepted artifacts. Its adapter owns
script names, exit codes, internal-library imports and compatibility checks.
BPR contains no Jira project/field schema, feature-flag taxonomy, risk scale,
code-owner accounting rules or enterprise template parser. No live enterprise
service or proprietary skill is needed to develop or qualify the generic flow.

Record the upstream version/fingerprint actually consulted. On resumption or
before publication, changed upstream rules require revalidation of affected
preparation. Do not copy the enterprise rules into the companion and call that
tracking upstream. The companion cannot safely invoke a monolithic publisher
as a lint step or promise a pause inside an indivisible script. Unsupported
decomposition returns a visible limitation and preserves manual preparation.

The genuine self-review marker and downstream readiness checks stay the
enterprise tool's responsibility. Empirical or formal evidence does not forge,
rewrite or substitute for its required marker. A permitted draft-only skip is
still an explicit upstream operation, not a BPR exemption. Ticket operations
can occur before PR creation, but are explicit actions; pre-PR does not mean
offline or effect-free. Follow-up can link a ticket and host visuals after
the real PR exists. Such failures cannot undo or recreate the PR.

### Fitted to this pack's other decisions

These rules come from the [reconciliation record](assets/v0-5-0-prerelease-targets-v1/RECONCILIATION.md); its row numbers are given.

- **What the Agent tab may do (X15).** [RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md) gives an Agent tab agent a
  scratch folder, on to begin with, and lets the reviewer allow builds in
  the disposable checkout. Neither is a BPR-scoped route for an arbitrary
  configured action in the author's worktree, so the handoff decision above
  stands unchanged.
- **Builds and evidence (X16).** A build or test run by an Agent tab agent
  is a reported result, as
  [the evidence RFC](RFC-BPR-0109-author-evidence.md#fitted-to-this-packs-other-decisions)
  says.
- **The accent on actions (X11; confirmed by the owner, 2026-11-10).** An
  extension action bound to an agent wears the agent accent of [RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md); one
  bound to a script or tool uses the ordinary button roles.
- **Shared configurations (X17).** Agent definitions inside a workflow
  configuration follow the sharing RFC's rules for the two added agent
  settings.

## Acceptance criteria

- **AC01:** With all extensions absent, local Changes, Evidence, Description and Tour remain usable; selecting/importing an enterprise-shaped configuration does not make unrelated drafts or existing BPR features depend on it.
- **AC02:** A shared configuration binds local agents/skills/actions explicitly, previews conflicts and updates, and never executes or changes credentials merely through import or selection.
- **AC03:** A fixture companion supplies context, proposals, repeatable checks and an input request; the author can pause, edit and resume without rerunning unrelated actions or accepting stale proposed edits.
- **AC04:** A changed upstream rule, missing helper, unsupported dry-run or required review marker produces an honest unmet requirement; BPR neither embeds those rules nor fabricates satisfaction or bypasses the chosen publisher.
- **AC05:** The selected publisher receives the exact accepted plan through an interactive handoff whose harness owns approval; BPR launches no supervised effectful action and does not misrepresent a declaration as runtime isolation or execution proof.
- **AC06:** Uncertain publication and failed ticket/visual/checklist follow-up retain actual results, reconcile before retry and cannot create duplicate external objects through automatic replay.
- **AC07:** A synthetic organization adapter qualifies the public preparation integration without creating a PR or requiring enterprise accounts/scripts, and the user can inspect origin, selected context, effect scope and current outcome in the native author workbench.
- **AC08:** With extensions absent or unselected, an author completes the built-in journey from worktree selection through GitHub draft creation without enterprise tools, while optional integrations remain available to separately configured drafts.
- **AC09:** An extension action bound to an agent wears the agent accent and one bound to a script does not; importing a workflow configuration turns on builds for no agent without acceptance; and no handoff result is labelled observed.

## Proposed delivery

[Optional configurations](../issues/10-backlog/BPR-2032-share-optional-workflow-configurations.md) and the
[companion action contract](../issues/10-backlog/BPR-2033-provide-a-resumable-companion-action-contract.md) form a read-only milestone
after local tour preparation; their acceptance covers preparation and needs
neither publication nor an effectful action.
[Effectful handoff and recovery](../issues/10-backlog/BPR-2034-hand-off-effectful-actions-and-recover-their.md) and
[closeout](../issues/10-backlog/BPR-2035-close-rfc-0112.md) form the final [milestone](plans/v0-5-0-prerelease-targets-v1/milestone-plan.json)
after built-in publication, including the end-to-end independence check. The
owner's answer on the effect route is in [Decisions](assets/v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices).
