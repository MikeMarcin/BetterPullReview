---
schema: "implement-rfc.card.v1"
id: "BPR-1933"
title: "Measure the repository skill reader's Windows containment boundary"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"kind":"verification","found_by":"v0.4.2 build34 native Windows manifest gate","mandate":"Owner requires release fixes implemented and landed on main; measure the existing exception without weakening the gate.","milestone_plan":{"id":"bpr-1933-skill-containment","path":"docs/rfcs/plans/bpr-1933-skill-containment/milestone-plan.json"}}
---

# BPR-1933: Native skill-reader containment measurement

## Acceptance

- AC1. Run the unchanged configured repository skill-reader course normally and
  through the maintained zero-capability AppContainer launcher on the native
  Limited interactive Windows producer. Record exact source, commands, images,
  environment, working directory, raw results and first refusal or success.
- AC2. If contained succeeds, retire the exception and registration after native
  proof; if it refuses, record only the observed reason. Preserve the course's
  assertions and the manifest gate. Verify the corrected native manifest.
- AC3. Focused verifier tests and documentation pass, and the correction lands
  on authoritative main. Preserve build34's frozen source and actual failure.

## Bounded milestone

One measurement-and-integration milestone owns these three criteria. Native
build34 Release compiled, then its manifest gate refused the explicitly
unmeasured inventory row. That failure is retained. The release coordinator
owns producer dispatch and final release qualification; this slice publishes no
package and grants no broader native, security or release acceptance.


## Native observation (2026-11-08 UTC)

Frozen build34 source `a30ed6f5b5577b19b074a4df1fe76e7056743e2f`, Windows Limited interactive producer:

- The original registered Release `repository_skill_reader` course passes
  normally (CTest exit 0; the course's assertions are unchanged).
- The same executable, arguments and working directory run through the
  maintained zero-capability AppContainer launcher. `ENVIRONMENT` is applied
  first, then every ordered `ENVIRONMENT_MODIFICATION` through CMake's own
  `-E env --modify` mechanism, including the automated boundary marker,
  sanitization and Qt/xdiff paths. This produces child exit 3, with
  `FAILED: fixture git command` as the first test refusal.
- This is the fixture's Git setup refusal, before the skill-reader checks.
  It establishes no lower-level Win32 or DOS-volume cause and no passing
  contained functional coverage. The existing uncontained registration stays;
  only its inventory changes from unmeasured to the observed predicate.
- Course executable SHA-256:
  `f535cf22aedfcfd973350ba131c3e1f4300b2c61836ebdf1d639620c5c24d624`.
- Maintained sandbox executable SHA-256:
  `493f43ee904213943d392a7ac24a536a8078f00884bdbb4618e1b0acd2e4442f`.
- The receipt confirms unchanged image hashes and clean, unchanged frozen
  source, with no owned descendants after command completion.

External evidence is
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate34/implementer1933/native-measurement-r1`:
`receipt.json`, `original-release.xml`, and the forced-contained raw streams.
Receipt SHA-256: `43518a8685ef36ef0c93de39ac959de4d09399edf0372ad3a468e2669c91b534`.
The receipt retains the complete configured command/environment/cwd and image
identities. No log or transient test output is committed.

## Correction and scope

The inventory now says `fixture-git-command-failed`. The course registration,
assertions, sandbox capabilities and verifier's mandatory measurement rule
remain unchanged. The exact script correction passes the original registered native
Windows Release manifest gate (1.35 seconds). Its scoped patch was reversed,
the script SHA was restored exactly, and frozen build34's source is clean and
unchanged. Evidence is in `native-pin-verification-r1` beside the paired course
measurement. Final release qualification will use the subsequent coherent
source/cohort; build34's actual gate failure remains in its original record.


## Verification and handback

- The existing measured-cause vocabulary test now explicitly includes the
  observed `fixture-git-command-failed` category, preserving its closed set.
  The first focused run correctly rejected the unlisted new category; all
  74 verifier tests pass after this evidence-based vocabulary update.
- The exact source correction also passes the configured Mac manifest (0.55
  seconds). The documentation gate prints `DOCUMENTATION_OK`; its 67 tests
  pass. Only the verifier inventory, its vocabulary assertion and this issue's
  bookkeeping change; no C++ source or test registration changes.
- An independent main writer reserved BPR-1934 for a feature card during this
  slice. Its card/counter/receipt were preserved by merging main before
  documentation validation; no product feature was adopted by this slice.
- The native producer is idle with its frozen build34 tree restored. No package
  or tag was produced by this work. The coordinator owns the next source/cohort
  and final native, artifact, signing, security and publication requirements.
