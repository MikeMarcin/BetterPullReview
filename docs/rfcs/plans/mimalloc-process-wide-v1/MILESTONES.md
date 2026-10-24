# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **mimalloc-process-wide-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## pinned-macos-integration

The pinned allocator runs process-wide in the selected BPR desktop, CLI, update helper and other helpers on native macOS with observable ownership and safe cross-module release.

**Work:** `BPR-1758`, `BPR-1759`

**Prerequisites:** None

**Completion:**

- Pinned and offline configure controls reject substitute source and distribution refuses the baseline switch.
- Apple Silicon and Intel native probes demonstrate allocation/free ownership in selected product executables and a signed installed candidate.

**Boundary:** A usable macOS product graph and explicit, testable allocator selection; other native platforms retain their own obligations.

## windows-integration

Native Windows 11 x64 desktop and selected helpers or CLI use one coherent mimalloc override with the actual CRT and packaged DLLs.

**Work:** `BPR-1760`

**Prerequisites:** None

**Completion:**

- Installed native Windows probes show C, C++, Qt/loaded-library interception and safe cross-module release through startup and shutdown.

**Boundary:** The Windows process-wide product path, without claiming complete release qualification.

## linux-integration

Ubuntu 26.04 x64 desktop and selected helpers or CLI use mimalloc on the selected WSL2/WSLg station without injecting it into external child programs.

**Work:** `BPR-1761`

**Prerequisites:** None

**Completion:**

- Installed Ubuntu 26.04 WSL2/WSLg probes show C, C++, Qt/loaded-library interception, safe cross-module release and a clean child environment.

**Boundary:** The Linux process-wide product path, without claiming complete release qualification.

## distribution-and-measurement

All selected installers and CLI artifacts carry the verified allocator and matching source/legal inventory, with same-workload performance and memory results.

**Work:** `BPR-1762`, `BPR-1763`

**Prerequisites:** windows-integration, linux-integration

**Completion:**

- Installed runtime closure, notices, SPDX and separate corresponding-source asset agree on the pin; negative package checks reject omissions or extras.
- Composed and sanitizer/fuzzer paths pass their focused checks, and matched Release comparisons plus the unchanged memory course report outcomes and pre-existing failures.

**Boundary:** A distributable candidate and truthful performance record, with no publication or unrelated release/security acceptance.

## closeout

The RFC has criterion-linked evidence, current documentation and explicit native and release limits.

**Work:** `BPR-1764`

**Prerequisites:** pinned-macos-integration, distribution-and-measurement

**Completion:**

- AC01–AC09 are reconciled to direct observations, and documentation and focused integration gates pass.

**Boundary:** RFC closeout only; release approval remains with the existing release workflow.
