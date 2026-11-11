# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **v0-5-0-prerelease-targets-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## dependable-session

Approve and the other review submissions always publish or say why not, and every pull request opens with a fresh review surface.

**Work:** `DRAFT-0022`, `DRAFT-0023`

**Prerequisites:** None

**Completion:**

- The stalled submission's cause is recorded from a reproduction or another stated means, and a submission can no longer wait or vanish without a visible, bounded, logged condition.
- Another pull request's effects do not block the current submission, and opening any pull request shows the tray on Comments with no tour side chat while remembered filters and layout are kept.

**Boundary:** The reported approve failure and the leaked tray state are fixed and observable; nothing else about submission or navigation changes.

## agent-tab-asks

Any question or suggestion in the Agent tab sends with a stated subject, and every agent chat shares one set of rules for references, the send key, cost and the agent colour.

**Work:** `DRAFT-0014`, `DRAFT-0010`, `DRAFT-0011`, `DRAFT-0012`

**Prerequisites:** None

**Completion:**

- Review this pull request sends from Overview with the whole change as its subject, the scope list is gone, and selected lines attach and detach as a chip.
- Code references open from every agent reply, Return sends, cost shows per turn and per session in dollars and cents, and the accent, now a configurable Appearance colour, is on exactly the controls that start a run.

**Boundary:** A usable Agent tab for asking; run visibility and new agent permissions are later milestones.

## agent-work-visible

A running agent turn shows what it is doing and which helpers it started, and it ends only when that work has ended or failed out loud.

**Work:** `DRAFT-0016`, `DRAFT-0017`

**Prerequisites:** agent-tab-asks

**Completion:**

- How each installed agent reports background work is recorded, and a turn with outstanding helpers stays open, accepts their results and ends once.
- The running block, the folded summary and the 120-second quiet notice appear in the Agent tab and the tour side chat, and no turn ends silently.

**Boundary:** The reported silent failure has a distinguishing course; thinking text remains deferred.

## agent-scratch

An agent can write and run disposable scripts in its own scratch folder, on to begin with and switchable per pull request, and can build and test in a disposable checkout when asked to, with the grant stated honestly.

**Work:** `DRAFT-0019`, `DRAFT-0020`

**Prerequisites:** agent-work-visible

**Completion:**

- The scratch folder starts on under a per-agent setting, a change in one pull request is remembered for it alone, and installed Claude and Codex each write and run a script there while the workspace stays clean.
- Builds are confined to the disposable checkout behind the per-agent setting and the per-pull-request question, and the launch allowances for both agents have had a critical review.

**Boundary:** BPR grants no network access and claims no sandbox that a runtime does not enforce; tour runs stay read-only.

## tour-publishing

An author can publish a tour from a control that never scrolls away, watch each step land, and understand any refusal without choosing a publication form.

**Work:** `DRAFT-0003`, `DRAFT-0004`, `DRAFT-0005`

**Prerequisites:** None

**Completion:**

- The Tour tab's head and agent bar stay in place at any route length and the head reports the tour's publication state.
- The plan chooses the form, the steps advance as GitHub confirms them, failures are error-role banners with the recovering action, and a live mixed tour on MikeMarcin/BPR-Test publishes as before.
- On Overview the tour's text renders as Markdown, a stop row opens its stop in Review, and the state and actions match the Tour tab's head.

**Boundary:** Publication semantics and recovery are unchanged; stops and the tour's chats are the next milestones.

## tour-stops

Arriving at a stop shows its code, a stop at rest offers only reading and going, and the author accepts a stop the way a file is accepted, with the outline gone from the application.

**Work:** `DRAFT-0025`, `DRAFT-0026`

**Prerequisites:** tour-publishing

**Completion:**

- Every way of arriving at a stop reveals its lines in Review with no further click, the controls that change a stop appear only while it is edited, and Suggest an outline is absent from the application.
- A stop is Accepted, Not accepted yet or Changed since accepted from what the author accepted; only a stop whose material changed needs accepting again; and publication requires every stop Accepted.

**Boundary:** What GitHub receives is unchanged; the owner's look at the running stop remains a criterion.

## tour-conversation

A tour's chats end in a composer and one line, change the tour in the turn that was asked, run Enhance in the part's own session, and write references and moved code the way the author wants.

**Work:** `DRAFT-0007`, `DRAFT-0008`

**Prerequisites:** agent-tab-asks, tour-publishing, tour-stops

**Completion:**

- Only the composer and the session line sit under the transcript, a change request yields its proposal in the same turn in every thread, and Enhance sends a turn in the part's session.
- References in tour text are links or snippets in the application and commit-pinned permalinks on GitHub, and the citation and moved-code rules are in every prompt path with advanced versions.

**Boundary:** Sessions, history and the read-only tool list are unchanged.

## home-next-move

Home tells the author which of their pull requests came back, which can merge and which are failing, with who and why.

**Work:** `DRAFT-0001`

**Prerequisites:** None

**Completion:**

- The core decides one next move per authored pull request from carried and bounded facts, and Home presents its word, sentence, tone and order.
- Authored rows stay in My pull requests and Waiting on others in next-move order, and no section is added.

**Boundary:** No new notification and no change to Needs me.

## accepted-scope

Every criterion of the nine RFCs drawn from the prerelease notes is judged against real evidence and the live RFCs they amend are reconciled.

**Work:** `DRAFT-0002`, `DRAFT-0006`, `DRAFT-0009`, `DRAFT-0013`, `DRAFT-0015`, `DRAFT-0018`, `DRAFT-0021`, `DRAFT-0024`, `DRAFT-0027`

**Prerequisites:** dependable-session, agent-tab-asks, agent-work-visible, agent-scratch, tour-publishing, tour-stops, tour-conversation, home-next-move

**Completion:**

- Each closeout accounts for every criterion of its RFC, including live, installed-agent and owner-attended ones, and for the amendments to RFC-BPR-0011, 0012, 0034, 0041, 0042, 0043, 0066, 0070 and 0096.
- Any remaining platform, installed-agent or human evidence stays explicit rather than being replaced by fixture or authoring checks.

**Boundary:** Acceptance of the authorized scope only; release qualification is separate.

## share-agent-prompts

Authors share selected custom agents and existing prompts through a complete native import/export flow independent of later author features.

**Work:** `DRAFT-0028`, `DRAFT-0029`, `DRAFT-0030`

**Prerequisites:** None

**Completion:**

- A second profile uses the selected configuration after explicit local binding.
- Imports preserve unrelated settings and expose conflicts or missing dependencies without running tools.
- Focused native Settings evidence and the owning RFC closeout are complete.

**Boundary:** Independent first delivery requested by the owner; no local PR draft or enterprise dependency.

## local-draft-entry

An author starts from a Git worktree, sees the correctly based local PR candidate and resumes preparation from Home or a local handoff.

**Work:** `DRAFT-0031`, `DRAFT-0032`, `DRAFT-0033`, `DRAFT-0034`

**Prerequisites:** None

**Completion:**

- The signed-out first-click flow reaches the proposed diff with exact source and target identity.
- Selected dirty material and refresh/restart behavior preserve notes and historical candidates.
- Existing Review Cycles remains available through secondary navigation.
- Discard and profile resets remove only private preparation; a full store refuses new admission without silent eviction.

**Boundary:** Second in default order; sharing remains the first requested delivery. Independent local-draft engineering may continue while sharing closeout is pending, with no hard dependency on that closeout. Local author subject and usable changes/notes flow; remote publication is not claimed.

## author-evidence

An author builds and resumes qualified evidence for the actual local candidate without mandatory agent or formal-verification infrastructure.

**Work:** `DRAFT-0035`, `DRAFT-0036`, `DRAFT-0037`

**Prerequisites:** local-draft-entry

**Completion:**

- Reported and observed outcomes remain distinct and cannot auto-approve a claim.
- Relevant candidate changes require recheck while historical scope stays inspectable.
- Manual/companion imports and the public evidence flow satisfy the owning RFC.

**Boundary:** Useful evidence preparation without a general native execution engine or an agent run; conversation-proposed checks arrive with local-tour-preparation.

## local-tour-preparation

The author prepares a durable description and evidence-backed tour before any PR exists.

**Work:** `DRAFT-0038`, `DRAFT-0039`, `DRAFT-0040`

**Prerequisites:** author-evidence, share-agent-prompts, tour-publishing, tour-stops, tour-conversation, agent-tab-asks

**Completion:**

- Local tour material uses genuine local identity and survives candidate changes and restart.
- Agent conversations read only the captured candidate, resume after restart without an account, and start anew on a successor candidate.
- Description ownership is preserved and only selected private evidence enters public previews.
- Exact simulated publication mapping retains unresolved stops truthfully.

**Boundary:** Complete local reviewer-facing preparation with no remote publication claim. Needs the sharing slice because the local-authoring prompts extend its format and Settings flow.

## publish-prepared-draft

The built-in flow creates the prepared GitHub draft from the branch the author pushed and preserves its evidence, tour and recoverable follow-up.

**Work:** `DRAFT-0041`, `DRAFT-0042`, `DRAFT-0043`

**Prerequisites:** local-tour-preparation, dependable-session

**Completion:**

- A draft PR is created from the verified pushed branch through a qualified capability; BPR names the push and performs none.
- The exact outgoing and observed remote candidates are reconciled with evidence and tour sources.
- Lost answers and partial follow-up recover without duplicate PR creation.

**Boundary:** Built-in end-to-end author flow under the settled PUBLISH-01 answer: the author pushes with their own Git.

## workflow-companion-contract

Optional shareable workflow configurations and a read-only companion contract supply context, proposals, checks and author input to local preparation while the default author flow remains independent.

**Work:** `DRAFT-0044`, `DRAFT-0045`

**Prerequisites:** local-tour-preparation

**Completion:**

- A synthetic companion supplies context, proposals, checks and author input through current bounded requests.
- Local Changes, Evidence, Description and Tour remain usable without extensions; this milestone requires no PR creation.
- Imported organization-shaped configurations execute nothing and leave unrelated drafts unaffected.
- No enterprise policy, account, proprietary skill or ticket system is a built-in dependency.

**Boundary:** Read-only and proposal contracts only; needs neither publication nor an effectful action. No implementation of the supplied enterprise skill.

## workflow-effectful-actions

A selected external publisher or follow-up action runs through an interactive handoff to the author's harness, and its uncertain or partial outcomes reconcile without duplication.

**Work:** `DRAFT-0046`, `DRAFT-0047`

**Prerequisites:** publish-prepared-draft, workflow-companion-contract

**Completion:**

- The interactive handoff respects the harness's actual authority and reconciles uncertain outcomes.
- Exactly one publisher acts for each publication attempt.
- With extensions absent or unselected, the complete built-in journey still creates a GitHub draft without enterprise tools.
- The extension RFC closeout judges every criterion with a synthetic integration.

**Boundary:** Settled ACTIONS-01 answer: interactive handoff only, no BPR-supervised effectful action; no implementation of the supplied enterprise skill.
