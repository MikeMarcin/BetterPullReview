# BetterPullReview Issue Board

Issue IDs use the `BPR-NNNN` sequence allocated by `NEXT_ID`.

## Lanes

| Folder | Status | Meaning |
|---|---|---|
| `backlog/` | Backlog | Planned but blocked by dependencies |
| `ready/` | Ready | Unblocked and available to start |
| `in-progress/` | In Progress | The current implementation slice |
| `done/` | Done | Verified terminal state |

Folder placement is authoritative. Front-matter `status` must agree with the
folder. Move a card rather than copying it, and update dependencies when new
work is discovered.

Each card links one RFC, names its dependencies, defines completion evidence,
and is committed with the implementation slice it tracks. RFC closeout is
blocked while any required card remains outside `done/`.

## Current frontiers

RFC-BPR-0001 is `Auto-Verified`, and every linked card is in `done/`.
RFC-BPR-0002 is the closure frontier in canonical order. Its planning slice is
complete; its architecture, deterministic-core, normalized-contract,
host-profile/capability, portable/native credential, and hardened GitHub CLI
broker slices are verified. `BPR-0023` is in `done/` with its Windows
implementation evidence, while direct Windows x64 execution remains in
`BPR-0036`. `BPR-0025` is in `done/`; its production CLI path remains
fail-closed with zero launches. `BPR-0037` is in `done/` with the
product-owner-approved milestone-one resolution: native HTTPS is the sole
production authentication transport, every visible CLI choice returns typed
`cli_unavailable` before CLI discovery or process creation, and positive CLI
enablement is deferred outside pack v2 to RFC-BPR-0015. `BPR-0026` is in
`done/` with its
macOS/Linux native HTTPS trust, proxy, redirect, cancellation, bounds, and
runtime evidence. `BPR-0027` is in `done/` with the application provider,
serialized host, pagination, rate/retry, credential-lifecycle, and staging
surface verified on macOS by the focused 9/9, fresh `dev` 35/35, fresh
`core-only` 17/17, sanitizer 3/3, and tests-disabled Release gates; its final
Ubuntu 26.04 x86_64 full-system QEMU build passed under GCC 15.2 and its
focused matrix passed 9/9 under Qt 6.10.2. `BPR-0028` is in `done/` with the
provider-free atomic projection, accessible GitHub Qt window, stable
incremental conversation model, three-scale rendering, fresh macOS 38/38 and
core-only 17/17 matrices, sanitizer 4/4, and Ubuntu x86_64 host/scale 4/4
evidence. `BPR-0038` is in `done/` with the fail-closed decision to keep
query-bearing HTTP redirect targets rejected. `BPR-0029` is in `done/` with
explicit GitHub.com/GHES profiles and provenance, multi-profile provider
routing, native HTTPS and platform-vault selection, Qt-owned sensitive input,
hostname-bound v2 authority, orderly exact credential deletion, visible
zero-launch CLI unavailability, public GitHub.com/GHES application fixtures,
and a tests-disabled Release seam-absence contract. Its pinned Linux v2 matrix
ran at the BPR-0029 snapshot in an Ubuntu 26.04 x86_64 full-system QEMU guest,
including the native vault, both public fixtures, Release seam absence, exact
cleanup, and dependency/runtime/ELF audits. `BPR-0030` is in `done/` with
exact
recorded-method and bounded GraphQL operation policy, canonical-body
backstop, exhaustive provider-failure mapping, a checked-in deterministic Qt
property corpus, and the current 32-scenario native HTTPS verifier. Its final
macOS matrices passed 44/44 and 17/17 plus sanitizer refreshes; its Ubuntu
26.04 x86_64 GCC 15.2 build completed 83/83 targets and the complete
non-native matrix passed 41/41. `BPR-0031` is in `done/` with the interim
rendered-state, accessibility, and target-native macOS appearance evidence
complete. Its authorized same-process probe observed both Qt and the global
operating-system setting change from Light to Dark under Cocoa/QMacStyle,
preserved focus/selection/collapse/model state, persisted distinct inspected
rasters, and restored the original Dark setting. This is not a final
visual-polish claim: RFC-BPR-0005 owns the dedicated presentation-polish and
native visual-acceptance pass, while RFC-BPR-0006 owns selectable themes,
fonts, and syntax presentation.

The open Windows task does not block later implementation, tests,
documentation, security review, or simplification on available targets.
BPR-0036 blocks only its affected Windows claims and final RFC-BPR-0002
Auto-Verified closeout. It does not stop work on later RFCs. BPR-0032 is in
`done/` with the current security/authority audit, exact one-row/two-row CLI
self-test, tests-disabled Release symbol gate, native macOS checks, focused
sanitizer matrix, and corrected evidence boundaries complete. BPR-0033 is in
`done/` with the immutable-base documentation impact review, corrected native
appearance record, operator-safe troubleshooting, resolved documentation
ledger, and workflow-absence gate complete. BPR-0034 is in `done/` with
behavior-preserving identity, capability-cache, accessibility-text,
CLI-automation, and Release-contract simplifications verified by complete
available-platform and focused sanitizer matrices.

BPR-0035 and BPR-0036 remain in `backlog/`: BPR-0036 retains direct real
Windows x64 evidence, and BPR-0035 cannot mark RFC-BPR-0002 Auto-Verified
until that evidence exists. By product-owner direction, this does not block
planning and implementing later RFCs in canonical order.

RFC-BPR-0003 remains open for explicitly deferred closeout claims while the
replacement RFC-BPR-0004 is the next implementation frontier by product-owner
direction. BPR-0039 is in `done/` with the
immutable-base architecture, authority, claim, platform, UX, documentation,
performance, and security plan recorded in the
[RFC-BPR-0003 verification ledger](../verification/RFC-BPR-0003.md).
BPR-0040 is in `done/` with the dedicated epoch core/host, account-isolated
bare cache, app-owned protocol-v2 and credential-free `index-pack`, versioned
ref/catalog, exact cause/alias, recovery, Qt projection, lifecycle, and
no-impact architecture selected. BPR-0041 is in `done/` with the compact
deterministic epoch reducer, exact identity/completion policy, explicit
catalog-bound projection clears, and direct core evidence complete. BPR-0042
is in `done/` with the hardened installed-Git local process boundary, exact
spool consumption, bounded lifecycle, hostile canaries, and direct macOS
evidence complete. BPR-0043 is in `done/` with the app-owned
protocol-v2/native-HTTPS, authority-partitioned bare-cache, and retained-ref
component slice verified through commit `2801241`. BPR-0044's durable
epoch-catalog and operation-record persistence slice is in
`done/`, with fresh macOS Debug, core-only, sanitizer, tests-disabled Release,
and guarded-Windows cross-compile/link evidence complete. BPR-0045 is in
`done/` with the source-free application host, production entropy/clock ports,
exact append/recovery sequencing, repository FIFO/session isolation,
stale/cancel rejection, projection containment, and quiescence directly
verified. BPR-0056 is in `done/` with the single revision-bound core cursor,
bounded intent/request/terminal algebra, repository-actor catalog execution,
sanitized incremental page projection, stale/revision/cancellation rejection,
and direct core/manual-executor/SQLite evidence complete. BPR-0055 is in
`done/` with deterministic core authorization, exact durable prune/cache-delete
orchestration, every maintenance restart phase, fault-injected failure policy,
incremental availability projection, and real installed-Git/SQLite composition
evidence complete. BPR-0046 is in `done/` with the provider/effect-free stable
epoch model, independent exact selectors, alias actions, full OIDs, explicit
availability/history states, accessibility, native-System inheritance, and
three-scale rendered loaded-window evidence complete. BPR-0047 is in `done/`
with the exact accepted GitHub handoff, revocable smart authority, concrete
Git/cache/catalog/epoch/Qt composition, two-process GitHub.com/GHES
persistence and cache reuse, tests-disabled Release exclusion, and quiescent
shutdown directly verified. BPR-0048 is in `done/` with the manifest-bound
sibling-head force-push fixture, public retained-old-head/cause evidence,
issued-transport revocation, broad combined 25/25 matrix, and fresh sanitizer
coverage complete. BPR-0049 is in `done/` with refreshed scale-specific,
public-path, Linux XCB/QFusionStyle, and macOS Cocoa/QMacStyle epoch UX and
accessibility evidence. Its fresh Ubuntu 26.04 GCC 15.2 build exposed the
intentional aggregate-default warning policy now resolved by done BPR-0057,
then directly exposed SQLite rejecting the catalog's deliberate retained
`/proc/self/fd` directory path. Done BPR-0058 now preserves that descriptor
path through a private per-catalog VFS, retains the directory chain, and proves
a live replacement namespace receives no effect. BPR-0049's final probe now
requires a real active macOS window and existing focus; its prior inactive
native run is not counted for that claim. Done BPR-0059 now supplies that exact
owner-activated proof through the real Cocoa/QMacStyle composition: active
focus, stable epoch identities/OIDs/Whole PR selection, readable native
palettes, zero model resets, distinct inspected 2200x1440 Light/Dark rasters,
and restoration of the owner's original System Dark setting.
BPR-0050's fresh Ubuntu 26.04 x86_64 GCC run exposed a real actor-capture
evaluation-order defect. Done BPR-0060 now gives the scheduler and worker
distinct actor owners before the call, directly passes the prior GCC
reproduction, and corrects the stale-terminal regression that had hidden an
empty-optional access under libc++. BPR-0050's resumed 26-case GCC matrix then
exposed an eager catalog-optional dereference when a stale preparation
terminal arrives during conflict reload. Done BPR-0061 now rejects stale
phase/request/generation values before forming the catalog-dependent context
and directly covers all three preparation terminals. BPR-0050's slow
target-native group then exposed a stale independent review database-ID
expectation. Done BPR-0062 now asserts the exact nonzero review identity in
both host recordings and the independent normalized-read expectation.
BPR-0050's refreshed installed-Git/cache group then exposed a dangling
canonical-OID view in the force-push fixture. Done BPR-0063 now retains
canonical-hex owners across fixture Git argument construction and both Qt OID
renderers; the full focused matrices pass under Clang, GCC, and sanitizers.
BPR-0050 then reached its fresh tests-disabled GCC Release composition, which
exposed optimizer-only protocol parser bounds and expected-conversion
warnings. Done BPR-0064 now makes the retained section-header bounds local to
every write and materializes successful client results without
cross-error-type conversion. The final parser/transport matrix passes 4/4
under Clang Debug, GCC Debug, GCC Release, and ASan/UBSan; fresh tests-off
composition, dependency, runtime, architecture, and critical-review gates are
complete. Done BPR-0050 now completes the current macOS and Ubuntu x86_64
security matrix, including platform vault failure modes, public runtime and
rendering, secret scans, source-manifest binding, and removal of the transient
QEMU key/image/log harness. Its independent critical review has no unresolved
blocking or high-risk implementation finding. Done BPR-0065 now preserves the
verified SQLite amalgamation bytes while excluding only their upstream
blank-at-EOL bytes from Git's path-specific diagnostic. Done BPR-0051 now
completes the 186-path immutable-base documentation-impact classification,
product/authority/operator updates, explicit no-impact dispositions, focused
direct reruns, and independent accuracy review. BPR-0066 is now in `done/`
with core-authorized newest/older navigation, one 100-epoch/200-anchor Qt
page, stale-page invalidation, out-of-window exact selection, 1,000+ page
traversal, and real rendered/restart evidence complete. BPR-0052 is now in
`done/` after removing the unused catalog-replacement payload, extracting the
application timeout snapshot, deliberately retaining fail-closed authority
structure, and replaying the clean developer, core-only, sanitizer,
tests-disabled Release, documentation, and rendered integration matrices.
BPR-0053 and BPR-0054 remain in `backlog/` pending their explicit board
transitions. Done BPR-0067 now exposes confirmed prune/cache-delete commands
through the selected application and real Git/SQLite projection path, with
default-Cancel confirmations, unrelated-ref canaries, durable restart proof,
and no additional provider traffic. Done BPR-0068 makes the epoch-host job
ledger an explicit global admission bound, includes actor-serialized blob
materialization, separately bounds ordinary reentrant ingress, and preserves
fixed terminal priority for invalidation/shutdown. Its focused flood,
sanitizer, and sequential component closeout matrices pass without launching
Qt or the application. Done BPR-0086 completes the nonblocking
allocation-failure and invalid-limit hardening. A `BUILD_TESTING`-only seam
drives each of the three recoverable admission allocation boundaries to fail
and requires zero partial admission, lease revocation, unchanged active
authority, exactly one `unavailable` diagnostic, and shutdown quiescence; the
limit table now also rejects zero deferred-ingress capacity and values above
both hard maxima. The seam's two symbols are in the tests-disabled Release
forbidden-byte contract, which passes, and are present in the tests-enabled
library, so the guard is proven in both directions. Done BPR-0069 completes
the final reconciliation: every ambiguous `Partially verified` claim now states
exactly what was executed and names target-native Windows as its only
remainder, the bounded-resource claim no longer implies that repository-actor,
prepared-observation, recovery, or identity-retry capacities were flooded when
only the job ledger and ordinary ingress are, the owner-activated macOS probe
has recorded provenance bound to its own commit with its attestation limits
stated, and the complete 374-path immutable-base classification is resolved.
Its clean serial replay passes 88/88 on `dev` and 33/33 on `core-only` with the
format, strict tidy, dependency, runtime, pack, workflow, and diff gates green.
Done BPR-0070 completes the
fail-closed command-projection refinement. `core::prunable` is now the single
definition of prune eligibility, shared by the command projection and the
catalog port, so an already pruned, cache-deleted, or never-observed selection
is no longer offered a destructive confirmation it could only fail. Authority
did not move: the core transition and host catalog remain the fail-closed
decision point, and a directly injected intent still reaches them. Its focused
serial matrix passes 7/7 including the real Git/SQLite maintenance composition
and the selected-executable public path, and the new core case was confirmed to
fail without the fix. BPR-0054 makes
no closeout claim while its remaining blocking dependencies remain open.
BPR-0056 supplies the bounded core/host history-page algebra consumed by the
BPR-0046 Qt selector model without a reset or an unbounded core copy.
BPR-0053 reserves target-native Windows evidence for the product owner's real
Windows machine without blocking available-platform implementation or later
RFC work. BPR-0054 remains blocked by BPR-0035/BPR-0036, BPR-0053, BPR-0069,
and BPR-0070, so final RFC-BPR-0003 Auto-Verified closeout and every
affected unverified claim remain intact without delaying RFC-BPR-0004.

RFC-BPR-0004 revision 2 is now the implementation frontier from immutable base
`243295a70756b2e63a785ed11b14647b6b0a4c6d`. Done BPR-0071 imports the
validated replacement pack in place, preserves the canonical ID/filename and
all downstream RFC numbers, updates active architecture/UX/verification and
downstream contracts, and records that no early RFC4 code or old follower/fake-
row design exists to remediate. The external P4Merge screenshot/video remain
non-product references outside the repository.

Done BPR-0072 supplies canonical comparison/alignment records, normalized
option and exact cache identity, stable IDs, compatibility-aware adjacency,
one contiguous group range, canonical move order, and the public structural
validator. Done BPR-0073 supplies byte-faithful decoding, bounded deterministic
primary alignment, monotonic pairing, UTF-8-boundary intraline edits, unique
whole-segment move metadata, mechanical changed-run groups, and public
retained-result validation. Done BPR-0074 supplies the one exact/similar/span/
boundary mapping source, separate move correspondence, canonical group
projection and navigation order, semantic selection, fail-closed detail
inputs, and retained exact-content validation. Done BPR-0075 supplies exact
request/generation acceptance, byte-bounded immutable blob materialization,
separate effect/decision lanes, bounded exact caching, cancellation/shutdown,
fixed-snapshot reentrant feedback, and deterministic stale-result rejection.
Done BPR-0076 supplies live natural-layout pane-boundary measurement, the
Qt-local indexed max-extent axis, zero-extent-safe projection, exact endpoint
clamping, stable semantic identity, and explicit coalesced invalidation, with
three-scale live-pane evidence. Done BPR-0077 supplies the sole retained
subpixel comparison position, shared scrollbar projection, rail-based pane
frames, receiver-neutral pixel/angle/phased input, guarded frame application,
same-comparison geometry anchoring, exact discrete scrollbar actions,
coalesced repaint delivery, and pane-local horizontal scrolling through a real
three-scale Qt event composition including live text viewports. It makes no
painted or selected-runtime claim. Done BPR-0078 supplies the custom-painted
`DiffLinkMap` splitter handle, cached filled ribbons and wedges, indexed
visible-path selection, stable path-derived interaction, accessible group
navigation, exact move markers, palette inheritance, and directly inspected
three-scale live two-pane renders. It makes no public application or target-
native System-theme claim. Done BPR-0079 composes the production selected-file
workbench, core semantic interaction state, exact-context loading/failure/
invalidation, same-actor installed-Git blob materialization, full-window
rendered public path, pending-diff shutdown, and live cache-deletion
invalidation. Its deterministic appearance contract is not target-native
System-theme evidence, and it leaves RFC-BPR-0005's presentation-polish pass
open. Done BPR-0080 supplies bounded/indexed moved-sub-block discovery,
retained immutable semantic indexes, semantic-free high-frequency geometry,
linear visible move-overlay resolution, and instrumented three-scale evidence.
Done BPR-0081 reconciles documentation/conformance and adds the worker
boundary. Done BPR-0082 removes reviewed duplication and avoidable work.
BPR-0083's available-platform closeout preparation and selected-composition
replay are complete, and its outstanding independent review is now satisfied:
an `rfc-critical-reviewer` pass returned non-blocking findings only, falsified
no RFC-BPR-0004 semantic claim, and found no stub or helper-API-only proof
presented as public-path evidence. Its one medium finding is closed by the
replay at `ca424f7`, which passes 88/88 on `dev` and 33/33 on `core-only`
including the selected public diff path. The card still remains in backlog
because it depends on BPR-0054. Its owner-authorized native System-theme gate
is now satisfied: an owner-activated Cocoa/QMacStyle run at commit `1e581a6`
passed active focus, both local and global Light-to-Dark transitions, and every
workbench palette, font, geometry-rebuild, and changed-render marker, with both
2560x2000 rasters inspected directly and the owner's original System Dark
restored. Done BPR-0087 closes the two actionable
evidence-strength findings, which were assertion weaknesses in the composed
replay rather than defects: the wheel steps now require the retained position
to have advanced before the receivers are compared, and the ribbon click now
first selects a different visible group through the public link map and
requires the replacement click to replace it. The replay records both states,
so the artifact shows a click changing semantic selection; it is now nineteen
records and nineteen images. Implementation can progress under the
product-owner exception without an out-of-order Auto-Verified transition.

By further product-owner direction the implementation frontier now advances
past RFC-BPR-0004 through the remainder of the pack on available targets, so
RFC-BPR-0005 onward may be implemented while earlier RFCs remain `Proposed`.
Each of those RFCs ends its available-target work at a closeout card in
`backlog/`. Requirements that need target-native Windows x64 execution, an
owner-attended check, or external authority are recorded on live cards naming
exactly what is missing rather than being silently skipped.

Done BPR-0084 follows the completed host-execution slice and modernizes
the shared build foundation before the remaining RFC-BPR-0004 Qt work. It
raises the CMake baseline, centralizes installed-package discovery for prebuilt
dependencies, replaces the tracked SQLite amalgamation with pinned verified
CPM source acquisition, and refreshes presets, direct dependency evidence, and
build documentation without adding an RFC-BPR-0014 package format or release
claim.

Done BPR-0085 adopts the rendered project-neutral C++ standard as the
repository source of truth. It applies canonical formatting, naming, include
guards, and public API documentation across the first-party source and test
tree; integrates local format and strict tidy targets; preserves emitted
binary names; and updates structural contracts to assert semantics without
depending on incidental whitespace. The developer and core-only matrices,
strict first-party clang-tidy audit, format gate, and documentation gate pass.

The RFC-BPR-0003 comparison controls are an interim reusable surface, not
final visual acceptance. RFC-BPR-0005 retains a distinct integrated-workbench
presentation-polish and native visual-acceptance slice. RFC-BPR-0006 owns
selectable System/Light/Dark/high-contrast appearance, fonts, and syntax
presentation; a fresh installation must default to System.

The public application has no durable credential selector or enumeration
path. Orderly shutdown exact-deletes the retained credential before worker
teardown; an abrupt crash can leave an item that a later process cannot
recover. No restart-recovery claim is made.

GitHub Actions is not a BetterPullReview build, test, package, release, or RFC
verification surface. Platform-specific claims require local or target-native
evidence.
