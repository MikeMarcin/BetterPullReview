# C++ identifier parser evaluation checkpoint

Assignment: branch `claude/cpp-parser-evaluation` (worktree
`.claude/worktrees/cpp-parser-evaluation`), mandate
`cpp-identifier-parser-evaluation-r1`, strategy coordinator-integrator.
Owner: rfc-integrator, claude-opus-5-5, high.
Baseline: `530d92eaff1578413c0e669f476e740387441152`.

**Done:**
- BPR-1441 (`cad32aa0a`): cancellation test, syntax fuzz target, and the
  restored contract fixture.
- BPR-1440 (labels `309375c5a`): corpus, labels and gap suite.
- BPR-1442 (`02da27cbd`): harness, pins and macOS measurements.

**Outcome on macOS:** extend. T fails Contract, Bounded work and Memory at both
pins. The follow-up is BPR-1474 (Backlog). The report is
`docs/verification/RFC-BPR-0081.md`; raw results are in
`tests/syntax_evaluation/results/macos/`.

**Next:**
- BPR-1443 (Windows x64) and BPR-1444 (Linux x64 and its fuzz campaign) on
  their stations;
- then BPR-1445 closes AC03, AC04 and AC06 and confirms the RFC.

Q1 is open and moot.

**Invariants:**
- the evaluation option stays off by default and test-only;
- shipping spans are byte-identical to the baseline;
- labels are not edited after candidate results; the hand-review adjudication
  lives in `labels/hand-review.jsonl`.
