---
schema: "implement-rfc.card.v1"
id: "BPR-0013"
title: "Remove hosted GitHub Actions builds from RFC-BPR-0001"
status: "Done"
rfcs: ["RFC-BPR-0001"]
depends_on: ["BPR-0011","BPR-0014","BPR-0015"]
---

# Remove hosted builds from RFC-BPR-0001 verification

Remove GitHub Actions from the BetterPullReview build path. Preserve useful
platform-local CMake presets and runtime inspectors, but make current-host
foundation evidence distinct from the clean native package evidence owned by
RFC-BPR-0014.

## Completion evidence

- No GitHub Actions workflow builds, tests, packages, or releases the product.
- Documentation and structural validation enforce that boundary.
- The current-host native public path and Qt-free core remain directly passing.
- RFC-BPR-0014 explicitly retains clean Windows, macOS, and Linux package
  install/launch/runtime/uninstall evidence.

## Result

- Deleted the hosted build workflow and its action-specific tests.
- Replaced action pin validation with a structural prohibition on GitHub
  Actions product builds.
- Retained the portable Ninja preset, Windows Visual Studio preset,
  cross-platform dependency inspector, and cross-platform runtime inspector.
- Clarified the RFC split: RFC-BPR-0001 proves the architecture and
  current-host native path; RFC-BPR-0014 proves delivered packages on clean
  native environments.

## Evidence

- GitHub Actions workflow files selected by the repository: zero.
- Platform configuration contract: 4/4 passed.
- Complete Qt-enabled local matrix: 14/14 passed.
- Complete Qt-free local matrix: 9/9 passed.
- Documentation validator: `DOCUMENTATION_OK`.
- Source-pack validator: `OK: 14 RFCs, 17 normative documents, architecture v2
  enforced`.
- `git diff --check`: passed.
