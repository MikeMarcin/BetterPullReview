# Optional workflows and companion skills

RFC-BPR-0112 lets an organization share an optional author workflow: writing
guidance and a few actions that a separately maintained companion performs
for a local PR draft. Nothing here is needed to prepare or publish a draft;
Changes, Description, Tour and the built-in **Create Draft PR** work with no
workflow at all.

## Sharing a workflow

A workflow travels in the agent configuration file that Settings › Agents &
context exports and imports (RFC-BPR-0107). A file that carries one is
written as version 2, so an earlier BetterPullReview refuses it as a newer
file instead of dropping its actions. Each workflow names:

- `id`, `name`, `origin` (who it says it comes from; shown, never trusted)
  and `guidance` (writing guidance for its agent actions);
- up to 16 `actions`, each with a slot (`gather_context`,
  `propose_description`, `check_preparation`, `publish_draft`,
  `finish_publication`), an author task in plain words, a binding and the
  effects it declares (`read_context`, `write_proposal`, `run_check`,
  `change_ticket`, `publish_pull_request`). `required` marks a check the
  workflow's own publisher needs.

An `agent` action names an agent carried in the same file and the prompt it
is asked; it runs in BetterPullReview's tool-free writing lane and can only
propose, so it cannot fill a publication slot. A `command` action names a
program by file name only and an argument array in which `{request}` and
`{response}` stand alone as whole arguments. The recipient binds the program
on their own computer; no path, credential or enterprise rule travels.
An action this version does not know makes its workflow unimportable, and
the preview names it.

Importing or selecting a workflow runs nothing and changes no credential.
Each draft selects one explicitly and pins the revision it selected; a newer
revision in Settings is offered on the draft and taken only when accepted.

## The companion contract

BetterPullReview never starts a companion program (ACTIONS-01). **Hand off**
records an invocation, writes the request file and names the exact command;
the author runs it in their own terminal or harness, whose approval owns what
it does. **Read its answer** admits the response file. The files live under
the profile in `workflow-exchange/DRAFT/INVOCATION/`, owner private.

The request (`bpr.workflow-request`, version 1) carries the invocation, the
draft and its version, the action and the workflow revision, the candidate's
commits and paths, the destination, the title, description, problem,
solution and notes, the guidance, and when continuing the author's `answer`
to the companion's `question`. A publication action's request also carries
the recorded plan (its digest, destination, account, head, title, body) and,
once linked, the pull request and every object an earlier attempt reported
in `previous`, so a retry makes nothing twice.

The response (`bpr.workflow-response`, version 1) is at most 16 MiB and has
no field this version does not know. It names the `invocation`, `draft` and
`action` it answers and an `outcome`:

| Outcome | Meaning |
|---|---|
| `proposal` | `proposals` for `title`, `description`, `problem` or `solution` |
| `findings` | `findings`, each `met`, `unmet` or `unknown` |
| `input_request` | a `question` for the author; Continue sends the answer |
| `completed`, `failed`, `interrupted`, `uncertain` | the step's end as the companion saw it |

Any response may add a `message`, the `upstream` tool it consulted (`name`
and `fingerprint`), `limitations` (`upstream_changed`, `missing_helper`,
`no_dry_run`, `review_marker`, `unsupported`) and `references` to external
objects (`pull_request`, `ticket`, `visual`, `checklist`; `created`,
`updated`, `failed` or `unknown`).

Only the current invocation's answer is admitted. Proposals bind to the
field revisions the invocation read, so a field the author edited since is
never overwritten. Everything a companion returns is shown as reported by
it, never as observed by BetterPullReview.

A required check is met only when its latest answer, on the latest
candidate and the selected revision, reports every finding met, reports no
limitation, and no later answer reported another fingerprint for the same
upstream tool. BetterPullReview holds none of the organization's rules and
writes none of its markers.

## Publication

A draft has one publisher per attempt: the built-in one or the workflow's.
With the workflow's chosen, **Create Draft PR** stands aside, and **Hand off
the plan** gives the recorded plan, confirmed by its digest, to the
publisher once every required check is met. A report of a pull request
links nothing: **Check GitHub** reads the open pull requests from the
draft's branch into its target and links the one that matches the plan, as
one BetterPullReview did not create. A lost answer is recorded with **Its
answer was lost** and must be reconciled by that read before the plan is
handed off again. A follow-up whose objects failed or are unknown is retried
only after the author confirms they checked them.

`tests/fixtures/workflow_companion` holds a synthetic organization companion
and its upstream tool, which the course
`application_local_draft_workflow_desktop_public_path` runs as the author
would.
