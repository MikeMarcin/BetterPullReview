---
schema: "implement-rfc.card.v1"
id: "BPR-0835"
title: "The agent finding composition contracts fail without a named failure"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "mac"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0835: The agent finding composition contracts fail without a named failure

## Defect

`application_agent_finding_composition_contract` is red on Windows
uncontained and prints no named failure term, even with the fixture data root
narrowed to the owner-private shape by [[BPR-0812]]
(`tests/end_to_end/agent_finding_composition_public_path.cpp`, data root
`<tmp>/data`).

Reported by `bpr-win` in `task-20260917-win-hardening`, not carded there.

**macOS too, and before the Windows merge (dsp-003).** On macOS
`application_agent_finding_composition_contract` and
`application_agent_finding_full_agent_contract` both fail at main `5a2e264a`
and identically at `2ee49a16`, before windows-lane's BPR-0812 test changes
were merged, so this is not a Windows defect and not caused by that merge.
Both print `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0` and
`panel-status:Finished: success`, and the full-agent contract prints
`agent-finding-full-agent:0:0:<n>:0:0:0`; neither prints its pass term
(`build/r2-logs/dsp3-int1-ctest.log`, `dsp3-agent-ab-head.log`,
`dsp3-agent-ab-pretests.log` with the two test sources restored from
`2ee49a16`, `dsp3-base-finding-ctest.log` built at `2ee49a16`). When it
started is not established.

## Scope

- Find the first bad revision on macOS and the mechanism (which of the
  positive-evidence fields is 0, and why).
- Make the course name its failure first (the step it stopped at), then fix
  the cause.
- Whatever the cause, a composition course that ends without a named term is
  itself a defect: the course must print the failed step.

## Acceptance

- Both contracts pass on macOS and on the Windows station, and the cause is
  named here.

## Cause and fix (dsp-003)

**No product defect. Two accepted product decisions changed what the course
should expect, and the course did not follow either.** The bisect ran from
`bacd5177`, where both rows pass on macOS. Logs are in `build/r2-logs/dsp3-0835-*`.

1. **Both rows: `826fb871` ([[BPR-0705]]).** The parent `061dfcdf` passes.
   BPR-0705 moved the MCP helper's snapshot into a per-window folder,
   `mcp-snapshot/window-<id>-generation-<gen>` (`src/app/src/github_application.cpp`
   ~6410). Its accepted decision reads "MCP snapshots are private, derived
   from typed window/generation identity". The course still expected the
   bare `mcp-snapshot` folder, so `helper-directory-is-snapshot-root`,
   `snapshot-binding-written` and `snapshot-alignment-written` were false.
   Every printed count was at its expected value; the false terms were never
   printed. The course now derives the snapshot path from the one window it
   drove.
2. **Full-agent row, a second cause: `554564f8` ([[BPR-0689]]).** The parent
   `9b0c02c4` passes. The course counted `->submit(` call sites in
   `github_application.cpp` and expected exactly one, the ledger's.
   [[BPR-0689]] and [[BPR-0733]] (`91383cc4`) added two
   `SetupDestination->submit(` calls. `setup_destination_host::submit`
   (`src/application/include/bpr/application/setup_destination.hpp`) parses a
   pull request address and drives setup navigation through its own port. It
   is not an edge to GitHub. The term now requires exactly one
   `ReviewLedgerHost->submit(`, and every `->submit(` must be either the
   ledger's or the setup destination's. A submit on any other receiver still
   fails.

**Named failures.** A `contract_terms` recorder now prints
`agent-finding-composition-failure:<step> failed=<terms>` on every failure
path: the positive, full-agent, dif-context, workshop-press and
construction-cut checks, argument and temporary-folder exits, and shutdown.

**macOS evidence.**

- **Slice worktree.** Both contracts pass 3 of 3 at load 8 to 21
  (`dsp3-0835-final-run{1,2,3}.log`). Negative control: restoring the course's
  two old expectations reproduces `1:1:1:1:1:1:0:1:0:0` and
  `0:0:802982:0:0:0`, and now names
  `positive-evidence failed=helper-directory-is-snapshot-root snapshot-binding-written snapshot-alignment-written`
  and `full-agent-second-edge failed=one-submit-call-site`
  (`dsp3-0835-negative-control.log`). The other agent-finding rows and
  `architecture_boundaries` pass (`dsp3-0835-final-siblings.log`).
- **Main `6c44b6ea` with the change, two `-j 2` batches at load 13 to 31**
  (`dsp3-int6-ctest-{1,2}.log`). All 11 rows passed in each batch except one:
  - batch 1: `application_agent_finding_full_agent_contract` failed with
    `positive-evidence failed=queued-findings-is-one … deliveries-is-one …`;
  - batch 2: `application_agent_finding_workshop_press_contract` failed with
    `workshop-press-turn:3:failed=run-quiescent`.

  Both are load-dependent waits, not this card's causes: each failing row
  passed in the other batch. They are recorded on [[BPR-0840]]. The new named
  terms are what made them readable.

**Windows.** `bpr-win` observed the same two lines before this fix
([the Windows observation](../../verification/BPR-0835-windows-observation.md)).
Both causes are platform-independent.

**Still open:** the Windows rerun with this fix, and one quiet macOS rerun of
both contracts.
