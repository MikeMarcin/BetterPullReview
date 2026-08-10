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
replay are complete. Its historical independent review falsified no
RFC-BPR-0004 semantic or component claim, and its one medium finding was
closed by the replay at `ca424f7`, which passed 88/88 on `dev` and 33/33 on
`core-only`. BPR-0225 later falsified that review's broader public-path
conclusion: the selected diff was requested only by a tests-only fixture helper
at its reviewed head. Done BPR-0227 now closes that required public path. The
card also remains in backlog because it depends on BPR-0054. Its
owner-authorized native System-theme gate is satisfied: an owner-activated
Cocoa/QMacStyle run at commit `1e581a6` passed active focus, both local and
global Light-to-Dark transitions, and every workbench palette, font,
geometry-rebuild, and changed-render marker, with both 2560x2000 rasters
inspected directly and the owner's original System Dark restored. Done
BPR-0087 closes the two actionable
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

RFC-BPR-0005 is now the implementation frontier under BPR-0088's advanced
product-owner direction. Done BPR-0089 plans it: twelve cards as BPR-0090
through BPR-0101, an architecture allocation across core, host, and Qt, and a
claim matrix built before implementation with one row per acceptance criterion.
The RFC composes the existing RFC-BPR-0004 panes, shared scroll controller, and
`DiffLinkMap` rather than rebuilding them; a second mapping, presentation-side
regrouping, follower-scroll coupling, or synthetic alignment rows would be a
defect. No RFC-BPR-0005 requirement needs Windows execution, a GitHub mutation,
a credential, or a network service. Its one owner-attended gate, BPR-0100, is
inherited from the RFC-BPR-0003 and RFC-BPR-0004 ledgers, which both defer the
integrated presentation-polish and native visual-acceptance pass here; it is
isolated so it blocks no other card. Backlog BPR-0101 makes no closure claim.

RFC-BPR-0006 is now the implementation frontier. Done BPR-0103 plans it as
thirteen cards, BPR-0104 through BPR-0115, and settles the one genuinely forced
decision. RFC-BPR-0006 mandates KSyntaxHighlighting, so only its acquisition
route was open. Caller-installed discovery was investigated first, since the
library is Qt-dependent and the dependency boundary treats Qt as never fetched,
but no Homebrew formula exists for it or for the KDE Frameworks on this host,
which would have made the RFC unimplementable rather than deferred. CPM source
acquisition following the SQLite precedent is therefore the only route that
satisfies the RFC, and it pulls extra-cmake-modules as a transitive build
dependency that BPR-0109 must record. Runtime behaviour is unchanged:
definitions stay packaged as resources with discovery and downloads disabled,
because build-time acquisition and runtime grammar downloading are different
things. Work is ordered so the appearance, typography, and theme surfaces land
before the build foundation changes. Its one owner-attended gate is BPR-0114,
the live scheme change; the fresh-install-defaults-to-System claim is separated
onto BPR-0106 because it is a default-value check needing no appearance change.
What is attended about BPR-0114 has since narrowed: both halves of the claim
were proven on a real display on 2026-08-08 under the owner's standing
authorisation to script the scheme change. That authorisation does not extend to
focus, and the probe requires a frontmost window, so what remains is one click
and an audit of the eight probe terms that root in it.

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

RFC-BPR-0006 has substantial predefined-profile, typography, contrast,
theme-validator, and syntax component work. It is not complete: done BPR-0236
keeps the appearance host, System defaults, session switching, and syntax path
composed when durable appearance storage cannot open, and done BPR-0237 adds
semantic customization and safe theme import. BPR-0233 owns recorded
selected-diff pixels, BPR-0220 owns native rebuild attribution, and
BPR-0114/BPR-0115 retain attended and terminal closeout work.

Done BPR-0180 is the card that changed how this board reads evidence. It
reconciled RFC-BPR-0006's ledger against what the application actually
*composes* and found that several components were linked but never
constructed. A symbol table cannot tell those apart, and for a while nothing
did. The cards that closed the gap are done: BPR-0181 and BPR-0182 for the
durable appearance settings adapter and its host, BPR-0185 for the syntax
adapter reaching the diff panes, BPR-0189 through BPR-0192 for the workbench
layout adapter, host, shell, and observation surface, and BPR-0193 for
operating-system notification delivery. `github_application_status` now
reports composition at runtime — `WorkbenchHostComposed`,
`AppearanceHostComposed`, `SyntaxClassifierComposed` — because linking is not
composing and only the running process can say which happened.

Done BPR-0186 added the attended interactive launch that holds the window open
so a person can inspect the composition. Doing that immediately produced
defects no automated marker had caught: BPR-0194, BPR-0196, BPR-0199,
BPR-0201, BPR-0202, BPR-0204, BPR-0205, BPR-0206, BPR-0207, BPR-0208,
BPR-0209, and BPR-0216 are all closed defects found by looking. BPR-0216 is
the sharpest of them — System mode painted the light palette on a dark desktop
while every marker stayed green, because the markers asked whether a profile
had been applied and never whether it was the *right* profile. BPR-0203
records the process correction that came out of the same period: an A/B/C
choice is not three pass/fail questions, and an attended review page has to
present it as one selection.

RFC-BPR-0007 through RFC-BPR-0011 have substantial component implementations
on available targets, but BPR-0225 found both missing required component scope
and absent public product paths. Done BPR-0238 supplies durable
notes/heads/defect metadata; ready BPR-0239 and in-progress BPR-0240 own queue
filters/sorting and complete agent context/process lifecycle. Done BPR-0228
through BPR-0231 and backlog BPR-0232 own the corresponding composition; the
review ledger, round trip, durable defect workflow, and honest observer-rule
queue now reach the public path. BPR-0120 through BPR-0132 supply the review ledger's receipt transitions, schema
and forward-only migrations, crash recovery, Qt queue projection, and
composition-root evidence. BPR-0134 through BPR-0141 supply the review
round-trip: draft state, stale-head refusal, the mutation coordinator's safe
target and idempotency, and the no-bypass proof. BPR-0145 through BPR-0152
supply defect classification, what verification evidence may and may not
conclude, and outdated/deleted anchor handling. BPR-0158 through BPR-0170
supply observer rules, attention buckets, the refresh scheduler, enrollment
retention across restarts, and metadata-only notifications. BPR-0171 through
BPR-0179 supply the AI agent boundary: explicit-invocation legality, session
identity and revision binding, the environment allowlist and credential
boundary, and pinned workspace preparation.

A run of test-reliability and honesty cards sits under RFC-BPR-0003:
BPR-0119, BPR-0131, BPR-0133, BPR-0154, BPR-0155, BPR-0157, BPR-0200, and
BPR-0213. Two are worth naming. BPR-0200 found that a persisted catalog
survived a fixture identity change and no test noticed. BPR-0213 found the
opposite failure of nerve — `GITHUB_FIXTURE_FAILED` printed while the fixture
was fine.

RFC-BPR-0013 now carries the crash and hygiene cards. Done BPR-0215 and
BPR-0217 replace three ad-hoc shutdown guards with one liveness token owned by
`github_application`, and BPR-0217 is closed by a test that *forces* the
condition — construct the application, arm a poller, destroy it, require the
callback to run and do nothing — rather than by the five runs that failed to
reproduce it. Done BPR-0218 moved the public-path fixture's scratch directory
out of the repository: a relative `QTemporaryDir` template resolved against
the working directory, so crash runs committed 198 files of live SQLite and
Git cache state.

### Claude-era final-state review

BPR-0225 is the done final-state review of the 216 Claude-coauthored
commits after `11fdce0`. It treats the executable and live public path as the
claim surface, rather than inferring reachability from a component test, a
linked symbol, a constructed object, or an intermediate commit narrative.

The planning slice records these directly observed composition gaps without
pretending to complete them inside the review:

- Done BPR-0226 connects the workbench host to the shipped window and accepted
  pull request, including exact invalidation and session-only layout fallback.
- Done BPR-0227 connects the user-selected diff path through the real file-row
  gesture and retained-cache comparison, with zero fixture-helper calls.
- Done BPR-0228 composes the review ledger and actionability without detaching
  the selected diff. Done BPR-0229 composes the fixture-safe shipped review
  round trip, and done BPR-0230 composes the durable multi-defect workflow.
  Done BPR-0231 composes honest local observer-rule enrollment and the
  incremental landing model. Backlog BPR-0232 owns explicit agent invocation.
  Its component tests remain evidence for the components, not product
  reachability.
- BPR-0233 owns the recorded UIforETW selected-diff and intraline-pixel path.
  BPR-0187 serves the recording, but the current registered public test only
  opens the pull request and saves a window grab; it does not request or assert
  the diff.
- Ready BPR-0235 replaces a structural live-mutation inventory with
  process-boundary default network denial before BPR-0142 can use a live
  provider. Done BPR-0236 supplies the promised appearance fallback when its
  durable store cannot open.
- In-progress BPR-0240, done BPR-0237 and BPR-0238, and ready BPR-0239 own
  required component scope missed by the first review pass: theme
  customization/import, durable ledger state, queue filtering/sorting, and
  exact agent context/process lifecycle.
- Done BPR-0234 hardens the closeout contract. BPR-0241 through BPR-0245 are
  the newly explicit terminal closeout cards for RFC-BPR-0007 through
  RFC-BPR-0011.

This allocation changes no RFC status and waives no closure dependency. The
review reconciled the older ledger prose after reproducing each final-state
finding and left the narrower truth wherever a required component or public
path remains absent.

### Open right now

With BPR-0226 through BPR-0231, BPR-0234, BPR-0236, BPR-0237, and BPR-0238
done, the live lane counts come from the card folders rather than a duplicated
number here. The groups below
describe the actual remaining authority or product surface; closure cards may
also depend transitively on more than one group.

| Group | Cards | What is actually missing |
|---|---|---|
| Real Windows x64 hardware | BPR-0036, BPR-0053, and transitively BPR-0035, BPR-0054, BPR-0083 | Target-native execution; no available-platform substitute counts |
| A designated live GitHub repository | BPR-0142, BPR-0143, BPR-0144, BPR-0169, and BPR-0153 | Owner-authenticated sessions against a safe target |
| Owner-attended or native-display judgment | BPR-0100, BPR-0114, BPR-0168, BPR-0210, BPR-0214, and ready BPR-0220 | A person looking, clicking, granting permission, or resolving the unexplained native geometry delta |
| Product composition | Backlog BPR-0232 and ready BPR-0223; done BPR-0226 through BPR-0231 | Connect notification and agent capabilities to public gestures and projections; the workbench host, selected diff, ledger, actionability, fixture-safe review round trip, durable defect workflow, and observer-rule queue are done |
| Missing component and resilience scope | Ready BPR-0235 and BPR-0239; in-progress BPR-0240; done BPR-0237 and BPR-0238 | Enforce durable live-test isolation and complete required queue-filter and agent contracts; theme customization, durable ledger state, and BPR-0236's appearance-storage fallback are done |
| Recorded selected-diff proof | BPR-0233, gating BPR-0214, BPR-0210, and BPR-0100 | BPR-0187 already serves UIforETW; the remaining work is to select its diff and prove semantic ranges and rendered intraline-role pixels |
| Closeout rigor and terminal cards | Done BPR-0234; backlog BPR-0241 through BPR-0245 | Replay each now-anchored executable checklist and retain a live terminal card for each Proposed RFC |
| Deferred or not a slice | BPR-0211, BPR-0212, BPR-0156, BPR-0101, BPR-0115 | Owner-deferred, RFC-scale, or blocked by closure ordering |

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
