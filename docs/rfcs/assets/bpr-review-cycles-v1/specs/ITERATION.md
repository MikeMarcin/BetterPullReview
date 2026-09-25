# Iteration, recipient responses and verification

## A round is not a source revision

Start at captured source `R0`, initially with no diff. The reviewer writes feedback
and freezes round `F1` against it. A recipient changes code through ordinary tools.
The reviewer captures `R1`, reads the results and authors `F2`; capture `R2` after the
next implementation. A discussion-only `F2` may use `R1` again. Capturing code does not
require a response report, a live agent, a Git commit or proof the round was delivered.

`Capture revision` is a manual first-class action. Optional adapter end-of-turn events
can offer a candidate capture, never automatically bless unstable bytes or verify work.
Snapshots have explicit parent observations. Imported or externally produced branches
may form a DAG; the UI shows incomplete or divergent history rather than pretending
capture ordinal means Git upload order. The usual single local sequence stays simple.

## Comparison selectors

- **Full source at revision** is a real single-source mode, not an empty-file diff.
- **This round's changes** compares the round's bound starting material with a named
  resulting revision; a reported result without captured bytes is unavailable.
- **Since my last review** uses an explicit reviewer checkpoint, not the most recent
  file opened or last recipient report.
- **All cycle changes** compares original scope material in `R0` with the selected
  latest revision; expanded scopes show unknown initial material honestly.
- **Choose revisions** pins the exact selected pair and does not jump when new work
  arrives. Semantic aliases may re-resolve, with visible newer-available affordance.

Use the existing diff engine and alignment/material identities. A source-first view
retains file/tab reading positions, code search and notes. Both old and unchanged
files remain commentable. Do not let a PR-specific “no new comment on old head” rule
suppress local historical discussion; do retain that rule for native PR publication.

## Responses

A structured report has its own ID, exact round ID/digest, author/provenance observation,
per-item responses and optional claimed code/test references. JSON is an optional
convenience. Readable reports and manual replies work without a parser or agent.
Responses can come from multiple recipients; do not overwrite a human reply with an
agent reply or silently pick one as truth.

Import reports with item membership, schema, byte and identity checks. Duplicate
report ID and identical body is a no-op; the same ID with different bytes conflicts.
Report arrival may append a conversation and expose a candidate result revision, but
may not write source, mark reviewed, verify, publish, reopen provider credentials or
retarget the current tab. Existing operation/correlation identity prevents late output
from another cycle or round attaching here. Partial reports state their coverage;
unmentioned items remain unknown. Do not fabricate provenance from a supplied model
name, filename, user agent, or text claiming to be a particular harness.

A report's claimed tests remain **reported**, distinct from test evidence the user
actually observed or independently ran. This pack does not make BPR a test executor.
The external harness may run tests within its own authorization. BPR can display
paths/hashes/log references or user-imported evidence; it does not execute them.

## Reconciliation and review

Re-evaluate original anchors against captured result material using exact correspondence
and recorded confidence. Present unchanged, addressed-looking, ambiguous, deleted and
unavailable material distinctly. Code disappearance is not proof of resolution.
A suggested replacement matching current material can be shown as already present;
it still does not verify the underlying issue or authorize an unrelated edit.

Reviewer verification names the feedback revision and checked source material.
Recipient `addressed` produces a needs-verification affordance, not a green accepted
state. Verification applies to the checked material; if it later changes, preserve
historical verification and display recheck-needed rather than silently carrying it.
Material-equivalent changes can reuse exact material receipts according to existing
rules, without claiming module-wide semantic coverage. `seen`, `read`, `accepted`
and `excluded` remain different facts; opening a folder never marks every file read.

The next round can select unresolved items, amendments, new notes and deliberate
carry-forward context. It does not re-export every historical conversation by default.
Show the previous round beside the new draft when helpful, but avoid making the
reviewer compose a separate aggregate prompt. Copy/send the new complete assignment
only after the user chooses the handoff. No autonomous loop or watch-and-dispatch.

## Manual and mixed-recipient courses

1. Human reviewer prepares eight items and two suggestions, copies one round to a
   colleague or external agent, then closes BPR.
2. Recipient edits files using ordinary tools and may send only a prose answer.
3. Reviewer reopens BPR, captures the next revision without any callback, examines
   All cycle / This round / Full source and verifies selected items.
4. Reviewer prepares a second round, this time saves a file, and repeats.
5. An optional structured response can be attached later without altering the code
   revision, prior verification or captured delivery observations.

This complete path must run in a binary/package without agent-specific components.
Adding a human response is not a weaker workflow than adding an agent response.
