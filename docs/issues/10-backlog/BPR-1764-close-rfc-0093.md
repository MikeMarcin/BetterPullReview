---
schema: "implement-rfc.card.v1"
id: "BPR-1764"
title: "Verify and close RFC-BPR-0093"
status: "Backlog"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758", "BPR-1759", "BPR-1760", "BPR-1761", "BPR-1762", "BPR-1763"]
closeout_for: ["RFC-BPR-0093"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# BPR-1764: Verify and close RFC-BPR-0093

## Scope and acceptance

Reconcile AC01–AC09 against exact native, public-path, package/source and
measurement evidence. Update architecture, build, package and operator
documentation (AC09), run the required documentation gate and focused
integration check, and record remaining platform/release limits. Do not close
with link-only or developer-build evidence in place of native interception.

## Implementation checkpoint

RFC base: `e82537ad15b32871a8375b6143abe08e16a68b87`.

Mandate r1: implement the supplied pack, including the owner-confirmed process-wide scope. Current milestone: pinned-macos-integration (BPR-1758 and BPR-1759). No native or package acceptance is presumed.

## Closeout checklist

| Criterion | Direct check / evidence | Result | Producing commit |
|---|---|---|---|
| **AC01:** Configure obtains only the pinned v3.5.3 source and licence bytes; offline, substitution and target-collision controls reject wrong inputs. | Native/public-path/package course required | Pending | — |
| **AC02:** Every selected shipped BPR executable has the override in normal and distribution graphs; disabled optional families create no phantom requirement, a core-only graph remains allocator-independent, non-distribution baseline selection works, and distribution refuses it. | Native/public-path/package course required | Pending | — |
| **AC03:** Native Apple Silicon and Intel macOS courses demonstrate C and C++ interception, representative Qt/loaded-library interception and safe cross-module allocation/free in the desktop and every selected helper/CLI, including a signed installed candidate. | Native/public-path/package course required | Pending | — |
| **AC04:** A native Windows 11 x64 course demonstrates the same behavior for the desktop and every selected helper/CLI with the actual CRT and packaged DLL closure; startup, shutdown and cross-module frees remain sound. | Native/public-path/package course required | Pending | — |
| **AC05:** An Ubuntu 26.04 x64 course on the selected WSL2/WSLg station demonstrates the same behavior for the desktop and every selected helper/CLI with the installed ELF closure; external child processes do not inherit allocator injection. | Native/public-path/package course required | Pending | — |
| **AC06:** Focused composed workflows and an allocator-boundary stress course preserve ordinary behavior and ownership; sanitizer/fuzzer builds use a compatible allocator path and allocation failure does not gain abort mode. | Native/public-path/package course required | Pending | — |
| **AC07:** Installed macOS, Windows and Ubuntu artifacts, standalone CLI where selected, notices, SPDX and the separate versioned corresponding-source asset agree on the pinned source and actual runtime files; existing release verification rejects a missing or extra mimalloc artifact. | Native/public-path/package course required | Pending | — |
| **AC08:** Same-workload Release comparisons and the unchanged large-review memory course record candidate identity, native platform, measurements and failures without attributing pre-existing BPR-0967 limits to mimalloc. | Native/public-path/package course required | Pending | — |
| **AC09:** Architecture, build, package and operator documentation state the actual override scope and limits; focused public-path and documentation checks pass, with all native and release claims tied to their evidence. | Native/public-path/package course required | Pending | — |
