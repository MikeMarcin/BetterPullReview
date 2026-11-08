---
schema: "implement-rfc.card.v1"
id: "BPR-1933"
title: "Measure the repository skill reader's Windows containment boundary"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"kind":"verification","found_by":"v0.4.2 build34 native Windows manifest gate","mandate":"Owner requires release fixes implemented and landed on main; measure the existing exception without weakening the gate."}
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
