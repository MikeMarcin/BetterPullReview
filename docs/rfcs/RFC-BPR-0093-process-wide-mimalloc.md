---
id: "RFC-BPR-0093"
title: "Process-wide mimalloc for BetterPullReview executables"
status: "In Progress"
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Process-wide mimalloc for BetterPullReview executables

## Problem and grounded current behavior

BetterPullReview currently has no mimalloc dependency or allocator override.
`CMakeLists.txt` centralizes dependency acquisition and creates all first-party
executables through `bpr_add_executable`; `src/app/CMakeLists.txt`,
`src/adapters/mcp/CMakeLists.txt` and `src/adapters/agent/CMakeLists.txt` define
the desktop, optional `bpr` CLI, MCP context helper, process supervisor and
macOS update helper.
The package graph ships the desktop and selected helpers, while the standalone
TourCLI install can ship `bpr` and its supervisor. The controlled dependency
boundary is described in `docs/architecture/system-architecture.md` and the
runtime/source inventory in `packaging/Install.cmake`,
`packaging/package_runtime.py` and `packaging/dependencies.json`.

Existing large-review measurements distinguish object ownership from allocator
retention. The current macOS 160 MiB sampled-growth failures remain on
`BPR-0967`; changing the allocator is not evidence that they are fixed
(`docs/verification/RFC-BPR-0013.md`).

Upstream [mimalloc v3.5.3](https://github.com/microsoft/mimalloc/releases/tag/v3.5.3)
recommends building from source and offers standard-allocation overrides.
Its [MIT licence](https://github.com/microsoft/mimalloc/blob/v3.5.3/LICENSE)
permits BPR distribution with notices. Upstream documents distinct
[Unix/macOS and Windows override mechanisms](https://github.com/microsoft/mimalloc#overriding-standard-malloc),
so a link line alone does not prove process-wide interception.

## Proposed behavior and boundaries

Use the official v3.5.3 source archive at
`https://github.com/microsoft/mimalloc/archive/refs/tags/v3.5.3.tar.gz`.
The inspected archive is 1,455,942 bytes with SHA-256
`3b4a15153a59905995f7070296ed604bb5ccc00cabb8b93446931aff77224d47`;
its `LICENSE` has SHA-256
`82ade5d7d9b029044b5fbbc326207fc47c2d44ee4dd71e8a559c36d728217de9`.
Acquire and build that source through BPR's pinned, offline-capable provider
boundary. Reject caller-supplied substitutes, unverified cache bytes and
unexpected target collisions. Build only the mimalloc variants the selected
product graph uses. A version change requires a new pin and source/licence
review, not a silent fallback.

Select mimalloc for the standard C allocation family and global C++
`new`/`delete` in every selected, BPR-built shipped executable: desktop,
`bpr` CLI, MCP context helper, process supervisor and macOS update helper.
An agent-free or
automation-off variant includes only the executables it actually ships.
Use the platform's supported process-wide override path so allocations in
eligible loaded Qt and other libraries are also intercepted. Establish one
coherent allocator instance per process and preserve matching allocation/free
across executable and library boundaries. A native probe must establish
actual interception and safe cross-module release; mere linkage, a version
string or allocator statistics are insufficient. Document any unavoidable
OS-library exclusions precisely; a process with unproven or mismatched
allocation/free paths cannot satisfy this RFC.

Keep the override at executable/runtime composition and distribution
boundaries. It adds no mimalloc API to core, application, adapter or Qt
domain contracts; a core-only graph acquires no mimalloc. It does not change
product state, persistence or
request semantics. Do not inject the allocator into external child programs
such as Git, `gh` or user agents; any launch environment used for BPR must
be confined to BPR-owned processes. The normal developer product graph uses
mimalloc by default and offers an explicit non-distribution baseline switch
for comparisons. Distribution builds require mimalloc. Sanitizer/fuzzer
graphs must retain their supported allocator instrumentation without an
incompatible process-wide override. Preserve normal allocation-failure
semantics; no abort-on-OOM mode or automatic heap trimming is proposed.

Carry every new runtime artifact into the installed app and standalone CLI
where applicable, with correct load path, signing/hardening, removal and
package-closure checks. Add the exact source, MIT notice, SPDX entry and
corresponding-source material to the existing version-bound release flow.
Do not claim a release, vulnerability disposition or native qualification
from a successful developer build.

Compare equivalent Release workloads before and after the override on the
same platform and hardware. Record launch, representative large-review
navigation, sampled and peak memory where measurable, and close/prune
behavior. Re-run the existing memory course without rewriting its limits
to manufacture a pass. Report regressions and the current BPR-0967 failures
separately; no fixed speedup or memory saving is required by this adoption.

## Acceptance criteria

- **AC01:** Configure obtains only the pinned v3.5.3 source and licence bytes;
  offline, substitution and target-collision controls reject wrong inputs.
- **AC02:** Every selected shipped BPR executable has the override in normal
  and distribution graphs; disabled optional families create no phantom
  requirement, a core-only graph remains allocator-independent,
  non-distribution baseline selection works, and distribution refuses it.
- **AC03:** Native Apple Silicon macOS courses demonstrate C and C++
  interception, representative Qt/loaded-library interception and safe
  cross-module allocation/free in the desktop and every selected helper/CLI,
  including a signed installed candidate. Universal arm64/x86_64 slices and
  static override evidence are verified; native Intel execution is not
  required or claimed (owner amendment r5).
- **AC04:** A native Windows 11 x64 course demonstrates the same behavior for
  the desktop and every selected helper/CLI with the actual CRT and packaged
  DLL closure; startup, shutdown and cross-module frees remain sound.
- **AC05:** An Ubuntu 26.04 x64 course on the selected WSL2/WSLg station
  demonstrates the same behavior for the desktop and every selected helper/CLI
  with the installed ELF closure;
  external child processes do not inherit allocator injection.
- **AC06:** Focused composed workflows and an allocator-boundary stress course
  preserve ordinary behavior and ownership; sanitizer/fuzzer builds use a
  compatible allocator path and allocation failure does not gain abort mode.
- **AC07:** Installed macOS, Windows and Ubuntu artifacts, standalone CLI where
  selected, notices, SPDX and the separate versioned corresponding-source asset
  agree on the pinned source and actual runtime files; existing release
  verification rejects a missing or extra mimalloc artifact.
- **AC08:** Same-workload Release comparisons and the unchanged large-review
  memory course record candidate identity, native platform, measurements and
  failures without attributing pre-existing BPR-0967 limits to mimalloc.
- **AC09:** Architecture, build, package and operator documentation state the
  actual override scope and limits; focused public-path and documentation
  checks pass, with all native and release claims tied to their evidence.

## Owner acceptance amendment — mandate r5, 2026-10-24

The owner explicitly answered “Amend Intel criterion”: accept the verified
universal binary/static x86_64 evidence and native arm64 observations without
requiring an Intel run. AC03 and the live macOS milestone/card now express that
requirement. This is an acceptance amendment, not evidence of native Intel
execution. The [current candidate and amendment ledger](../verification/RFC-BPR-0093.md#intel-acceptance-amendment--mandate-r5)
binds the universal app/CLI producers, static proof and native arm64 observations.
Actual Developer-ID signed-installed and delivered native Cocoa acceptance
remain pending on [[BPR-1759]]; [[BPR-1762]] and RFC closeout remain incomplete.
Other criteria and the immutable source pack are unchanged.

## Owner verification amendment — useful resource tests, 2026-10-24

The owner requires mimalloc and useful passing tests, and directed an audit of
each affected row's purpose before changing its gates. The bounded follow-up
did not establish a fair allocator-neutral process-memory envelope. The owner
therefore authorizes moving only the baseline-calibrated current process footprint
and its native lifetime-peak pass/fail predicates to explicit telemetry in the four default
resource CI rows. The original numbers, raw failures and plain benchmark flags
remain available for legacy qualification; none is retroactively called a pass.

The CI rows retain cache capacities/accounting, exact source and anchors,
eviction/revisit/supersession, public maintenance/shutdown and owner-release
assertions. Every observed pressure comparison's diff/index release is checked,
with a deliberately retained owner as a failing control. Launch, response and
clock/stall limits are unchanged. Native allocated-heap bounds remain enforced
where their counter covers the allocator. Mimalloc's uncovered native heap
observations are explicitly unavailable, rather than zero-byte passes.

This is an amendment to the CI verification policy, not a memory-use reduction
or proof that arbitrary process/allocated-heap growth is bounded. [[BPR-1768]]
owns that unqualified risk as a separate follow-up, not a new RFC closeout
dependency. AC08 remains reporting acceptance: its original unchanged-course
comparison and actual failures are retained. No plan outcome or exit is removed.
No product default, automatic collection, CI fixture input or memory limit is
changed. The [purpose audit and exact native evidence](../verification/RFC-BPR-0093.md#useful-resource-tests--owner-verification-amendment)
bind the implementation and practical limits. Signed-installed/Cocoa Mac,
package agreement and final closeout requirements remain pending under r5.

## Proposed work

Cards [[BPR-1758]], [[BPR-1759]], [[BPR-1760]], [[BPR-1761]], [[BPR-1762]], [[BPR-1763]] and [[BPR-1764]] and the [canonical milestone plan](plans/mimalloc-process-wide-v1/milestone-plan.json) derive
from this RFC. The [decision record](assets/mimalloc-process-wide-v1/DECISIONS.md) distinguishes confirmed
scope from authoring choices and execution evidence.
