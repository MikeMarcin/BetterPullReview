---
schema: "implement-rfc.card.v1"
id: "BPR-1759"
title: "Verify process-wide mimalloc on native macOS"
status: "In Progress"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Verify process-wide mimalloc on native macOS

## Scope and acceptance

Implement and observe the macOS override in the selected desktop, CLI,
update helper and other helpers (AC03). Probe actual C, C++, Qt/loaded-library allocation and
cross-module release on Apple Silicon and Intel. Check the signed installed
candidate, startup and shutdown. Include allocator-boundary stress needed by
AC06; a linked binary or stats report alone is insufficient.

## Candidate prerequisite amendment, 2026-10-24

The coordinator authorized moving only the package prerequisites needed for
AC03 into this milestone. This card therefore owns the macOS runtime
producer/deployment declaration, retained MIT notice and source-inventory entry
needed to prepare a tests-disabled installed candidate through `tools/release.sh`.
The source basis is `packaging/Install.cmake`'s fixed private-library producer
map, `packaging/package_runtime.py`'s closed runtime/notice inventory and the
release script's dependency, signing and relocated-launch checks. Without those
entries the installed allocator cannot load or pass the package audit.
Final AC07 for every platform, standalone CLI/source closure, negative package
qualification and release acceptance remain on [[BPR-1762]]. No criterion or
milestone exit is waived; signed installed and native Intel evidence stay
required. `--prepare-only` is an ad-hoc candidate, never signed-install acceptance.
