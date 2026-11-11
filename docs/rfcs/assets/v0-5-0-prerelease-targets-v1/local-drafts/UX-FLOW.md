# Proposed author experience

> Carried into the 0.5.0 targets pack on 2026-11-10 from the local PR drafts pack, version 1. RFC and card numbers were mapped on the way in (RFC-DRAFT-0001 to 0006 became 0010 to 0015; DRAFT-0001 to 0020 became 0028 to 0047). The pack as it was is at `/Users/jaeger/Downloads/bpr-local-pr-drafts-rfc-pack-v1`.

This is a behavior/wireframe proposal, not a rendered or accepted implementation.
The frontend-design skill was loaded from the installed official plugin, version
1.1.1, for this proposal. Its emphasis on task-oriented wording, deliberate
hierarchy and restraint is applied within BPR's higher-priority native Qt rules:
system palette, font, controls and focus, no forced web palette or QSS. No owner's
desktop was driven or captured. Actual native visual judgment belongs to a
later frozen implementation candidate under the existing attended-review rules.

## Starting from Home

Illustrative hierarchy, not pixel placement:

```text
Open a pull request   [ existing URL/repository opening controls ]

[ Prepare pull request… ]

In preparation
  Keep the latest search results     project / feature/search     Local draft
  ...

Existing review queues and recent pull requests
Secondary access to existing local reviews
```

The new action is available while signed out. A folder picker accepts a Git
checkout or linked worktree. Recent drafts resume directly. Review Cycles keeps
File > Open Project… and a compact secondary Home route; its old prominent card
does not compete with the new author entry. Existing PR-review tasks retain
their visibility and keyboard paths.

The normal folder choice reaches the changes immediately. Ask only for a target
or workspace decision that available facts cannot settle. Do not require agent
setup, a ticket, a polished title or a clean tree to inspect the local change.

## The local draft workbench

```text
Keep the latest search results                         Local draft
project     feature/search into main                  [Create draft PR…]

Changes     Evidence     Description     Tour

Changed files                 Exact proposed diff
                              2 local files are not included [Review…]
```

The title and branch names above are illustrative. The actual header identifies
the observed workspace/source and selected target. Its compact disclosures hold
exact revisions, base freshness and candidate qualifications. The initial
comparison is the proposed branch diff, not every source file marked newly
added or an empty comparison with the initial capture.

The four areas are places to work, not an ordered completion checklist. Keep
notes and conversation beside the material they concern, following the current
tour's accepted contextual-conversation direction. The author can return to their
editor or agent at any time. A noticed workspace change offers refresh; the view
does not silently follow a writer while the author is reading it. Accepting a
refresh starts conversations afresh on the new candidate, as a changed head does
for a pull request's tour; earlier conversations stay readable beside the
candidate they discussed.

An illustrative interaction:

1. The author sees a suspicious cancellation path in Changes and writes a question.
2. In a whole-change conversation an assistant proposes a delayed-completion
   experiment; once the author accepts it, it appears in Evidence as planned.
   Before the Tour area ships, the same proposal can only arrive by import.
3. The author runs it through their tools and imports the result as reported,
   including the exact tested material. Nothing in this flow is a host-observed
   run; that origin needs a later BPR-supervised action.
4. The result exposes a defect. The author fixes it outside the source reader,
   refreshes the candidate and reruns the affected check.
5. They use the result in a tour stop, discuss why it demonstrates the claim,
   and accept revised wording. Unselected conversation stays private.

Nothing in this example represents a test actually performed by this pack.

## Settings sharing

Under Agents & context, Import configuration… and Export configuration… act on
selected existing definitions. Export names exactly what will leave the machine.
Import shows a native list of items with current/incoming differences, required
program bindings and unresolved skill references. Keep existing settings is the
default conflict choice; Replace is explicit. A custom agent's relaxed checks
are visible in the comparison and apply only when accepted for that agent. When
a replacement changes what an agent runs, the comparison says its earlier
conversations will stay readable but can no longer be continued.

Applying a configuration saves settings. Check agent remains a separate action.
The first slice has no dependency on the author workbench shown above.

## Optional organization configuration

When configurations exist, the draft can choose the relevant one. In the normal
built-in flow, no enterprise setup or empty hook dashboard is shown. External
actions appear by the author's task, such as Gather ticket context or Check
description. Their details identify the provider, selected context, actual effect
scope and prerequisite. They can return a question without replacing the local
session or forcing unrelated stages to restart.

Settings defines and shares these actions; the draft runs them deliberately.
An unavailable required integration leaves a named unmet requirement. It does
not block local editing or masquerade as a successful default workflow.

## Publication and return

Create draft PR… opens the exact outgoing preview. A branch not yet pushed, or
pushed at a different head, shows the exact push to run with the author's own
Git, naming the hosted repository; BPR does not push. The preview
distinguishes material
prepared locally from follow-up that requires an actual PR number. The chosen
publisher is visible. An actual PR result becomes an association on the same
preparation; the author does not lose notes, evidence or their route through the
code. Partial follow-up is resumable separately from PR creation.

The native branch policy and effectful extension route are settled in
[the decision record](DECISIONS.md#resolved-product-choices). This wireframe
does not approve an implementation layout.
