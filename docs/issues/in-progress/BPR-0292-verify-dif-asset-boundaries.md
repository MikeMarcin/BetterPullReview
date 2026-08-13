---
id: BPR-0292
title: Verify Dif asset isolation and security boundaries
status: In Progress
rfc: RFC-BPR-0019
depends_on: [BPR-0290]
---

# Verify Dif asset isolation and security boundaries

Prove that RFC-BPR-0019 remains an offline Qt-adapter asset contract and does
not smuggle runtime composition, active content, private material, or an art
toolchain into ordinary product graphs.

## Required verification

- Configure, build, and test the developer and core-only graphs from clean
  state; configure and build the tests-disabled application graph.
- Inspect selected source/resource/dependency graphs and binaries for absence
  of core Dif assets, generator execution, runtime JSON parsing, model/network
  dependencies, and RFC-BPR-0020 widget/timer/preferences.
- Audit the Dif tree for strict path inventory, PNG metadata/chunks, active or
  remote content, private paths, prompts, credentials, repository data, GitHub
  marks, and Nib/Mel/AllTheThings product material.
- Confirm provenance records the notice/SBOM disposition without adding package
  install, signing, notarization, release, or certification work.

## Completion evidence

- Direct clean commands and scoped scans pass at the proving commit.
- Cutting the CTest registration, admitting an unlisted file, selecting an art
  dependency, or exposing a private path makes the corresponding gate fail.
- No screenshot or source-art inspection is claimed as runtime composition.
