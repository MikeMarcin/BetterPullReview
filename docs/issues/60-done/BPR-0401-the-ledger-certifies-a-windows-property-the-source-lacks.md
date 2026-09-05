---
schema: "implement-rfc.card.v1"
id: "BPR-0401"
title: "The RFC-BPR-0011 ledger certifies a Windows property the source does not have"
status: "Done"
rfcs: ["RFC-BPR-0011"]
depends_on: []
---

# The RFC-BPR-0011 ledger certifies a Windows property the source does not have

[[BPR-0332]] found six statements asserting, in the present tense, that the
adapter refuses a Windows agent launch before `QProcess::start`, plus a seventh
evidence row citing a test that contradicts its own claim. `c6f3d96` replaced
that refusal with a dispatch into `windows_agent_process.cpp`, which no toolchain
has compiled.

BPR-0332 chose to carry that cost deliberately, and wrote the trade down:

> Anyone reading those six statements before this card closes should read this
> card instead.

Nothing at those six locations says so. A reader of
`docs/verification/RFC-BPR-0011.md:109` sees a claim-matrix **Result** cell
reading "Windows starts no child" and has no way to learn it is false. The
mitigation exists only in the card that reader has no reason to open.

## What this card does, and what it must not do

It marks each site in place. It does **not** change any claim, choose between
BPR-0332's two resolutions, or make a Windows behavioural claim — nothing here
has been compiled or run on Windows.

That distinction is the whole point. BPR-0332's terminal criterion is a *true and
final* statement about Windows launch behaviour, which needs [[BPR-0240]]'s
compile first. Marking a statement as known-false is a strictly lesser thing that
needs nothing, and it leaves both resolutions equally open.

## The seven sites, re-verified at HEAD

BPR-0332's table was written at a different commit, so every location and claim
was re-checked here rather than copied.

| Location | Claim | Verified |
|---|---|---|
| `docs/verification/RFC-BPR-0011.md:13` | "Windows launch remains disabled before process start" | present |
| `docs/verification/RFC-BPR-0011.md:109` | "Windows starts no child." — a claim-matrix **Result** cell | present |
| `docs/verification/RFC-BPR-0011.md:115` | "Windows still proves pre-start refusal only." | present |
| `docs/verification/RFC-BPR-0011.md:216` | "The adapter refuses before `QProcess::start`" | present |
| `docs/architecture/ai-agent-boundary.md:140` | "refuses Windows launch before `QProcess::start`" | present |
| `docs/architecture/ai-agent-boundary.md:314` | evidence row for "Windows launch is refused before start" | present |
| `docs/architecture/ai-agent-boundary.md:335` | "Windows still refuses before child start" | present |

The source side was re-verified too. `agent_process_launcher.cpp` reads

```cpp
#if defined(Q_OS_WIN)
		return detail::run_windows_agent_process(
			aRequest, aTimeoutMilliseconds, aSink, cancelled);
```

with no refusal branch, and the test the seventh row cites,
`test_windows_assignment_failure_has_no_pre_assignment_side_effect` at
`tests/adapters_qt/agent_launch_canary_tests.cpp:621`, ends on
`recovered.Outcome == application::agent_run_outcome::success`. Both were read
directly rather than taken from BPR-0332.

## Required final state

- Each of the seven sites carries an in-place marker naming BPR-0332 as the owner
  of the resolution, so the statement cannot be read as certified.
- No claim text is rewritten, and neither resolution is pre-empted.
- BPR-0332's location table still cites correct line numbers afterwards. Marking
  the sites reflows the surrounding paragraphs, and leaving that card pointing at
  stale lines would reintroduce exactly the defect [[BPR-0399]] and [[BPR-0400]]
  repaired elsewhere.
- The documentation gate and both documentation contract tests pass.

## Non-goals

- Do not close, reclassify, or narrow [[BPR-0332]]. It still owns the resolution
  and its Required final state is still unmet: the assertions remain, marked.
- Do not restore the guard. That is resolution 1 and it changes shipped
  behaviour.
- Do not claim any Windows runtime property.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
ctest --test-dir build/core-only --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

## Result — 2026-08-19

All seven sites are marked at `108ab88`. `DOCUMENTATION_OK`; 94/94 unit tests;
`documentation_structure` and `documentation_issue_graph_contract` 2/2;
`git diff --check` clean.

Where a sentence is half true the marker says which half. `RFC-BPR-0011.md`'s
non-goal bullet and `ai-agent-boundary.md`'s composition paragraph both still
correctly claim that no Windows launch is proved; only their stated *mechanism*
is wrong. Marking those whole would have overstated the defect.

Marking the sites reflowed four of the line numbers [[BPR-0332]] cites, so that
card's table, its "Line 118 is the one that matters most" sentence, its
`ai-agent-boundary.md:321` reference, and its Required final state were corrected
in the same commit. Leaving them would have created the same stale-reference
defect [[BPR-0399]] and [[BPR-0400]] existed to repair — while repairing one.

BPR-0332 is otherwise untouched: still `10-backlog/`, still `depends_on:
[BPR-0240]`, still owning the choice between restoring the guard and rewriting
the claims, and its Required final state is still unmet. The assertions remain;
they are now labelled.

### The matrix this card was found under

The search that produced this card was a complete portable `dev` matrix at HEAD,
run uncontended: **193 of 193 passed** in 1603 s. That is stronger than the
checkpoint evidence on record — [[BPR-0240]] cites 184/186 at `be42eb7` with two
environmental failures — and it was run to look for latent macOS defects that
might constitute unattended work. There were none, which is the direct evidence
behind the board's claim that no unattended work is executable from a macOS
session today.

`ctest` was used without a rebuild, deliberately. The three live attended packets
bind `sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93`
at 51,085,872 bytes, and a relink would have invalidated all three. The digest
was re-measured after the matrix and is unchanged, so
[[BPR-0100]], [[BPR-0391]], and [[BPR-0392]] remain runnable exactly as written.
