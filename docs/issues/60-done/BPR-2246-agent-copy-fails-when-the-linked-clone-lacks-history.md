---
schema: "implement-rfc.card.v1"
id: "BPR-2246"
title: "The agent's copy fails when the linked clone lacks the head's history, and the failed run shows a Report"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2246--BPR-2246--04684998c7b5.json","related":["BPR-1751","BPR-1873","BPR-2118","BPR-2166","BPR-2245"],"acceptance":["BPR-2246:shallow-clone","BPR-2246:missing-history","BPR-2246:no-report","BPR-2246:remedy"]}
---

# BPR-2246: The agent's copy fails when the linked clone lacks the head's history, and the failed run shows a Report

## Report

The owner, 2026-11-17, from screenshots of the 0.5.0 prerelease (main at
about 0e01b3e9a), on a large private pull request (no repository detail is
recorded here): "Was this addressed? (32 threads)" from the Author tab's
Agent view stopped at once ("Stopped after 0:00"). The banner: "<agent>
stopped before answering. The agent's copy of this pull request could not be
made. Reopen the pull request, then resend. The message is kept." The log:

    error: Could not read <object>
    fatal: Failed to traverse parents of commit <head>

The failed run still showed a "Report" attachment whose page repeated the
error, and the queue was left "Paused: a run failed".

Owner guidance, 2026-11-17: "We want if possible for things to just work."
Repair what is missing and carry on with the run; show an error only when
repair truly cannot succeed, and then an accurate one.

## Scope

- The agent's copy is made when the linked clone holds the head but not
  all of its history (a shallow clone, or a borrowed object store that has
  gone), with no step asked of the reviewer.
- A run that stopped before answering shows no Report attachment.
- The remedy said when the copy truly cannot be made is true.

## Acceptance

- `BPR-2246:shallow-clone`: a native (retained) workspace borrowing a
  shallow linked clone that holds the head is made; its history ends where
  the clone's does.
- `BPR-2246:missing-history`: a retained workspace borrowing a clone whose
  head's parent cannot be read is still made, at the head and the base.
- `BPR-2246:no-report`: an ask that always reports (Was this addressed?
  on several threads, Explain this file) that ended without an answer shows
  its error inline and no Report attachment.
- `BPR-2246:remedy`: the copy-failed words no longer tell the reviewer to
  reopen the pull request.

## Findings

- The command: a native (Claude Code or Codex) session keeps its workspace
  (`RetainForSession`), so after checking out the head the workspace packs
  the head's whole history into its own store and drops the borrowed
  alternates: `pack-objects --revs` fed the head (and the base when held),
  in `materialize_agent_workspace`
  (`src/adapters/git/src/installed_git_process.cpp:5296` at 2d2081374).
  That walk is the "Failed to traverse parents of commit <head>".
- Why an object is missing: the workspace borrows the linked clone through
  one alternates line (`installed_git_process.cpp:5014` at 2d2081374).
  Alternates lend objects, not the clone's `shallow` list, so in a shallow
  clone (`--depth`, as CI or a quick checkout makes) the walk goes past the
  clone's boundary into parents it never fetched. A depth-one clone fails on
  the head itself, as in the report. The same happens when an ancestor is
  simply gone (a borrowed store of the clone's that was removed). No
  previous workspace's alternates are reused: each workspace writes its own
  one line. The review's cache is never shallow (its topology check
  refuses a `shallow` file), so it is not the cause.
- Reproduced with plain Git (a `--depth 1` clone borrowed through
  alternates: the same two lines) and in `git_process_contract`.

## Fix

- Mirror the clone's shallow list into the workspace (`.git/shallow`), read
  and checked line by line (`shallow_boundary`): the copy's history ends
  where the clone's does, for the retained pack and for Git the agent runs.
- If the walk still fails (Git exits nonzero: an ancestor is missing for
  another reason), retry once with the copy's history ending at the head
  and the base the clone holds. The run goes on with no step asked of the
  reviewer; the reviewer's clone is never written.
- A resumed session asks for the base only when the copy holds it
  (`RequiredBase`), so a shallow clone that lends only the head resumes.
- The queue is unchanged: a copy that is now made is a run that answers,
  so nothing pauses it.

## Decisions

- Should a run that stopped before answering show a Report attachment?
  No. `core::shows_report_attachment` decides: a turn that ended without
  an answer is never a report; its reason shows in the thread. Asks that
  always report (Explain this file, What changed since your review, Was
  this addressed? on several threads) still do when they answer.
- Is the remedy text right? No: reopening the pull request repairs no
  missing object. History the clone lacks is now repaired, not reported.
  What is left under `copy_failed` (a Git failure that is not missing
  history, a review copy that lost its head, the pull request's files not
  readable) now says "Resend to try again, or show the log for the cause"
  (launch panel) and "Check again, or show the log for the cause" (setup).

## Evidence

Implementation 0ba427cfb, on the lane `claude/v0-5-0-prerelease-targets-52265d`.

| Criterion | Check | Result |
|---|---|---|
| `BPR-2246:shallow-clone` | `git_process_contract`, `run_incomplete_clone_history_workspace_contract`: a clone shallow at mid lends head and mid, the retained copy keeps no alternates, its `.git/shallow` is the clone's, `rev-list --count HEAD` is 2, it resumes with no base required; a depth-one clone (the owner's case) gives a copy of the head | Pass |
| `BPR-2246:missing-history` | Same contract: a clone with no shallow list whose head's parent is gone gives a copy whose history ends at the head and the base, which it holds, required to resume | Pass |
| Reproduction | The same contract with the two repairs disabled fails with Git's own "error: Could not read <mid>" / "fatal: Failed to traverse parents of commit <head>", as in the report | Fails without the fix |
| `BPR-2246:no-report` | `core_agent_report`, `test_report_attachment_needs_an_answer`; `application_review_report_public_path` and `application_explain_file_public_path` still show the card for an answered report | Pass |
| `BPR-2246:remedy` | `qt_agent_launch_panel`: the copy-failed stop says "Resend to try again" and no longer "Reopen" | Pass |

Format (clang-format 22.1.8) passes on every changed C++ file; clang-tidy
reports nothing on the new code (its remaining findings in
`agent_report.cpp`, `agent_report_tests.cpp` and `agent_tab_view.cpp` are
on lines this card did not touch). `python3 scripts/verify_documentation.py
--root .` passes.

No end-to-end course drives a failed always-report ask: the fixture agent
fails only on a request naming `FIXTURE_CRASH` or an environment variable
the agent launch does not pass, and neither reaches Explain this file or
Was this addressed? through the window. The decision is the core's and the
Qt view only reads it.
