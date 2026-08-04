---
id: BPR-0119
title: application_diff_workbench_public_path fails intermittently in diff evidence
status: In Progress
rfc: RFC-BPR-0003
depends_on: []
---

# `application_diff_workbench_public_path` fails intermittently at stage 20

Observed on macOS 15.7.7 arm64, Qt 6.11.1, during BPR-0116. The test failed
once in three consecutive full `ctest --preset dev` runs against the *same*
build tree, and passed in isolation immediately after failing. Nothing else was
running: the suite is serial and no concurrent build was in flight.

This is recorded rather than dismissed. An intermittent failure in the real
application's public path is exactly the kind of defect that a rerun makes
disappear and a user does not.

## Captured evidence from the failing run

```text
PUBLIC_PATH_FAILED
PUBLIC_PATH_STAGE:20
PUBLIC_PATH_STATUS:Pull request ready.
PUBLIC_PATH_DIAGNOSTIC:
PUBLIC_PATH_ACCOUNT:Octo Reviewer (@octo-reviewer)
PUBLIC_PATH_EPOCH_STATUS:Review history is inactive.
PUBLIC_PATH_EPOCH_COMPARISON:Whole PR. No exact pair is resolved.
PUBLIC_PATH_EPOCH_PAIR:::-1:-1:0:0
PUBLIC_PATH_EPOCH_CORE_STATUS:-1
GITHUB_FIXTURE_FAILED:github.com
GITHUB_FIXTURE_EVIDENCE:11111110:-1:10:9
GITHUB_FIXTURE_SMART_EVIDENCE:1:1:1:1
DIFF_WORKBENCH_FAILED
DIFF_INTERACTION_EVIDENCE_FAILED
RENDER_FAILED
```

The downstream `DIFF_WORKBENCH`, `DIFF_INTERACTION_EVIDENCE`, and `RENDER`
markers fail because the run stopped, not independently.

The pull request itself reached "ready" and the account resolved, so the
failure is confined to review history: the epoch control reports **inactive**
with core status `-1` and no resolved pair, and the fixture evidence bitmask is
`11111110` — one bit short of complete, with every smart-evidence field set.
The suspicion is therefore an epoch-catalog activation that had not completed
when stage 20 sampled it, rather than a wrong result.

## Reproduction rate

Measured during BPR-0124, six isolated runs per configuration:

| Tree | Result |
|---|---|
| `acc9fa3` (before the ledger work) | 5 pass, 1 fail |
| `acc9fa3` plus the BPR-0124 ledger and shared SQLite runtime | 5 pass, 1 fail |

So it reproduces roughly one run in six *in isolation*, not only under full
suite load, and the SQLite connection-policy extraction and shared runtime
initialisation do not change the rate. The earlier note that it "passed in
isolation" was a single sample and should not have been read as evidence that
load was the trigger.

A one-in-six failure needs no clever harness to reproduce. Run the test in a
loop.

## Corrected diagnosis

The original reading of this card was wrong on three counts, and the
corrections matter more than the original guesses.

**Stage 20 is `shutdown`, not an epoch stage.** The printed stage falls back to
the *current* stage when `FailedStage` was never set, and every run ends at
shutdown. So the stage told us nothing; it named the one stage that could not
have caused the failure.

**`11111110` is the healthy value.** The trailing zero is `SecretRetained`,
which should be 0 after the credential is deleted. It was never "one bit short
of complete".

**The epoch text is post-shutdown state.** "Review history is inactive" with
core status `-1` is what the control reads after teardown, not the condition
that failed.

Two diagnostics were added to make the failure locatable:

- `fail_automated_run` now records the stage, as the timeout path already did.
- The failure output prints `PUBLIC_PATH_VERDICT`, naming each of the six
  conjuncts of the verdict. Five of them were previously invisible.

With those in place the failure reports:

```text
PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0 \
  epoch_projection_failed=0 epoch_handoff_failed=0
DIFF_WORKBENCH_FAILED
DIFF_INTERACTION_EVIDENCE_FAILED
```

Every status flag is healthy and `FailedStage` is still unset, which means no
failure path ran at all: a stage assigned `PresentationPassed` *conditionally*
and it evaluated false without reporting anything. The candidates are the diff
stages that assign it from `DiffRequested` and from
`DiffInteractionEvidencePassed`.

So this is a **diff-workbench evidence** flake, not an epoch-catalog one. The
title and the epoch framing of this card are both wrong and are kept only so
the earlier reasoning is auditable.

## What this is not

The first observed run happened while BPR-0116 was in progress, and that slice
touched `diff_workbench`. It is very unlikely to be the cause: BPR-0116 adds a
typography publication whose only effect is to invalidate measured geometry
when the tab stop moves, the real application changes no tab width during this
run, and the first publication is adopted as a baseline without rebuilding.
The failing stage is epoch history, which the change does not reach. Confirm or
rule this out rather than assuming it.

## Required direct evidence

- The exact stage-20 precondition is identified and named — which bit of
  `GITHUB_FIXTURE_EVIDENCE` is clear, and what sets it.
- Whether the failure is a missing wait on an asynchronous epoch-catalog
  activation, or a real ordering defect in the activation itself. If it is the
  latter, the application is wrong and not just the test.
- The failure is reproduced deliberately rather than by rerunning until it
  reappears — for example by delaying the activation the stage depends on.
- Any fix is shown to fail when the behaviour it constrains is neutered, and a
  repeated-run loop of the suite is clean afterwards.
