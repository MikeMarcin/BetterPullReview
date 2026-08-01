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

RFC-BPR-0003 is the implementation frontier. BPR-0039 is in `done/` with the
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
native run is not counted for that claim. Ready BPR-0059 owns only that
owner-activated transition proof rather than silently narrowing BPR-0049.
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
traversal, and real rendered/restart evidence complete. BPR-0052 is unblocked
in `backlog/` pending its explicit next-task transition. BPR-0053 and BPR-0054
remain in `backlog/` pending their explicit board transitions.
BPR-0056 supplies the bounded core/host history-page algebra consumed by the
BPR-0046 Qt selector model without a reset or an unbounded core copy.
BPR-0053 reserves target-native Windows evidence for the product owner's real
Windows machine without blocking available-platform implementation or later
RFC work. BPR-0054 depends on BPR-0035, BPR-0053, and BPR-0059, so canonical
closeout, every Windows claim, and the native macOS active-focus claim remain
intact.

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
