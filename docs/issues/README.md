# BetterPullReview Issue Board

Issue IDs use the `BPR-NNNN` sequence allocated by `NEXT_ID`.

## Lanes

| Folder | Status | Meaning |
|---|---|---|
| `10-backlog/` | Backlog | Planned but waiting on another card |
| `20-ready/` | Ready | Unblocked, and portable — any session can start it |
| `21-ready-win/` | Ready | Unblocked, and requires target-native Windows |
| `22-ready-mac/` | Ready | Unblocked, and requires target-native macOS |
| `23-ready-lin/` | Ready | Unblocked, and requires target-native Linux |
| `30-in-progress/` | In Progress | The current implementation slice |
| `40-parked/` | Parked | Nothing is in the way but a person or a live service |
| `50-blocked/` | Blocked | Something is genuinely unresolved, and it is not a platform, a person, or another card |
| `60-done/` | Done | Verified terminal state |

Folder placement is authoritative. Front-matter `status` must agree with the
folder. Move a card rather than copying it, and update dependencies when new
work is discovered. The two-digit prefix exists so that a directory listing
sorts the lanes in workflow order rather than alphabetically.

### Ready, and ready where

A lane has to tell a session what it can actually start. That is the whole
reason the ready lanes split by platform, and it took two wrong answers to get
here.

[[BPR-0319]] created `50-blocked/` for work needing an operating system this
checkout did not have. [[BPR-0358]] then argued the opposite: the project owns a
Windows 11 x64 host, so a Windows card is a queue position rather than a
hardware wait, and it moved nine cards back into active lanes. Both readings are
defensible and both were useless in practice. The first told a macOS session
that work was blocked when the machine existed; the second told it the work was
active when the session could not touch it. Either way, the only way to learn
that nothing here was startable was to read the cards.

So `2X-ready-<platform>/` says the true thing: the work is *ready*. It is
specified, its blockers are gone, and it will run the day a session opens on
that host. It is ready somewhere else.

- `20-ready/` is ready **and portable** — the lane a session picks from when it
  does not care which machine it is on.
- `21-ready-win/`, `22-ready-mac/`, `23-ready-lin/` are ready **on that
  platform**. A session that is not on it skips the lane; a session that is
  reads it as its queue.
- All four carry `status: Ready`. The folder names the platform, and a second
  copy of that fact in front matter is a second thing to keep in sync.
- The platform lanes **defer**, exactly as `50-blocked/` did: a card in one may
  still have an unmet dependency. The platform is why it is not in `20-ready/`;
  the dependency graph is a separate question and stays in `depends_on`.

`50-blocked/` survives that change with a narrower and more honest meaning:
something is genuinely unresolved. Not a platform, which is a lane; not a
person, which is `40-parked/`; not another card, which is `depends_on`. Its
`blocked_by` field is a free token rather than a closed set, because the
blockers worth recording are the ones nobody enumerated in advance — and
`scripts/verify_documentation.py` rejects a `platform-` value there, since
naming a platform would hide the card from the session that could run it.

When a card mixes portable work with work that needs another platform, split the
platform part into its own card so the generic part stays workable. That is
product-owner direction, given 2026-08-22. It has one condition: the portable
half must be *verifiable* here. A slice whose only proof is a target-native run
is not separable, and splitting it manufactures a card that cannot close —
[[BPR-0360]] and [[BPR-0364]] both considered exactly that split and correctly
declined it.

`40-parked/` holds a card whose every dependency is already done, and whose own
remaining work needs a person or a live service. Each one carries a `parked_on`
field: `awaiting-owner-verdict` for a person who must look, click, or return an
exported verdict; `awaiting-live-github` for a real GitHub call against a
designated target; `awaiting-product-decision` for a decision nobody has made
yet. A card returns to a ready lane when the verdict, the session, or the
decision arrives.

A parked card is a *frontier root* — nothing is queued in front of it, so it is
the only kind of open card someone could act on today. That is enforced rather
than asserted: `40-parked/` is not a deferring lane, so the dependency check
fails a parked card that still has an unmet dependency.

Each card links one RFC, names its dependencies, defines completion evidence,
and is committed with the implementation slice it tracks. RFC closeout is
blocked while any required card remains outside `60-done/`.

Verification scope follows the transition. Moving a card to `60-done/` needs
the focused selectors that card names and `clang-format` on the files it
touched.
The full `dev` and `core-only` matrices and the strict `coding-tidy` target are
RFC-closeout gates: together they run for the better part of an hour, and
spending that on every slice buys nothing a focused selector did not already
prove. Run them when a change plausibly reaches beyond its slice, and at
closeout, where a clean full replay is the actual requirement.

### The dev-binary hold — released 2026-08-20

Read this before editing anything under `src/`.

The board held these exact built `dev` application bytes while their attended
questions were actionable:

```
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93
51,085,872 bytes
```

`native-visual-acceptance-v003` is answered: the owner accepted [[BPR-0100]]
against those historical bytes, and its immutable response and validation record
sit beside the packet. The final-head v004 successor is now accepted against
`10899f24…181b28`, clearing the current BPR-0100 checkpoint without widening
either verdict.
`appearance-scheme-foreground-v007` was run on 2026-08-20 but could
not attribute its failure. The final-head v008 successor was returned on
2026-08-21: it directly satisfies [[BPR-0391]]'s owner-return wait and
[[BPR-0410]]'s diagnostic attribution, while [[BPR-0220]] records zero geometry
rebuilds. The notification verdict below clears the exact-byte hold, so
BPR-0220 implemented the supported expectation repair. The owner accepted that
narrow repair without requiring another digest-only review run, so BPR-0220 is
Done; BPR-0114 retains only its terminal closeout procedure behind BPR-0101.

The former third, `native-notification-delivery-v005` ([[BPR-0392]] and
[[BPR-0168]]),
**was run on 2026-08-20 and passed** — exit 0 with all eight required markers, a
real macOS banner delivered, read, and activated onto the intended review. The
final-head v006 successor is now accepted against `10899f24…181b28`: the owner
personally recorded the exact title and body, absence of private content, click,
focused landing selection, and exit-zero result. [[BPR-0392]] is Done.
[[BPR-0168]] returns to Backlog for terminal identity replay behind
[[BPR-0169]], not for another response against unchanged bytes.

The authorized BPR-0409/BPR-0410/BPR-0334 relink batch is now complete at
`145f6d4`. Its settled development executable is 51,383,520 bytes with SHA-256
`10899f24d0a2253eb707c345650522ce1ee9b91a7634dc65a7f1226338181b28`.
The appearance packet `appearance-scheme-foreground-v008` is answered with an
observed failure, visual `native-visual-acceptance-v004` is accepted, and
notification `native-notification-delivery-v006` is accepted. No attended
question now holds those exact bytes. BPR-0220 then relinked them for its
measured zero-expectation repair.

That repair is now committed at `761d416`. The current development executable
is 51,383,520 bytes with SHA-256
`b351456d6ec3090e24a28b13754aa74e7cd971ab1d1cdc1b1a6f46256d11d7f4e`.
The owner accepted the repair and explicitly asked not to repeat appearance,
visual, or notification reviews solely to preserve digest-bound evidence. No
successor packets are required for this slice; terminal RFC closeout retains
its own final replay obligations.

The owner then gave this bounded direction:

> I authorize replacing the protected dev binary to implement BPR-0409,
> BPR-0410, and BPR-0334.

That direction releases the hold for exactly the shared relink work it names.
Before the release, the hold forbade a rebuild but permitted runs against the
already-built tree; [[BPR-0256]] used that distinction at `5245c12` with the
digest checked before and after. The accepted historical identity remains
directly checkable until the first authorized relink:

```sh
shasum -a 256 build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
```

After that relink, no procedure may present v007 as a packet for the new bytes.
Regenerate its whole family only after BPR-0410's diagnostics exist. BPR-0100's
answered v003 packet remains frozen as the record of what the owner judged at
those bytes; the separate final-head v004 replay is now also accepted and clears
the current checkpoint without rewriting v003. BPR-0100 waits in Backlog behind
BPR-0083 for terminal identity replay. A later relink must obtain a successor
verdict rather than inherit either acceptance.

BPR-0409 performed the first authorized relink on 2026-08-21. Its completed
development executable is 51,302,640 bytes with SHA-256
`cb044fcab11545b181ae35ea6b4114d1f5ab0eef4457ec1945a03bc39fa7e8b7`.
The old `7b9d6be9…a78fcd93` identity remains the immutable subject of the accepted
BPR-0100 response, but it is no longer the executable in `build/dev`; v003 and
v007 cannot be reused for the new bytes.

## Current frontiers

RFC-BPR-0001 is `Auto-Verified`, and every linked card is in `60-done/`.
RFC-BPR-0002 is the closure frontier in canonical order. Its planning slice is
complete; its architecture, deterministic-core, normalized-contract,
host-profile/capability, portable/native credential, and hardened GitHub CLI
broker slices are verified. `BPR-0023` is in `60-done/` with its Windows
implementation evidence, while direct Windows x64 execution remains in
`BPR-0036`. `BPR-0025` is in `60-done/`; its production CLI path remains
fail-closed with zero launches. `BPR-0037` is in `60-done/` with the
product-owner-approved milestone-one resolution: native HTTPS is the sole
production authentication transport, every visible CLI choice returns typed
`cli_unavailable` before CLI discovery or process creation, and positive CLI
enablement is deferred outside pack v2 to RFC-BPR-0015. `BPR-0026` is in
`60-done/` with its
macOS/Linux native HTTPS trust, proxy, redirect, cancellation, bounds, and
runtime evidence. `BPR-0027` is in `60-done/` with the application provider,
serialized host, pagination, rate/retry, credential-lifecycle, and staging
surface verified on macOS by the focused 9/9, fresh `dev` 35/35, fresh
`core-only` 17/17, sanitizer 3/3, and tests-disabled Release gates; its final
Ubuntu 26.04 x86_64 full-system QEMU build passed under GCC 15.2 and its
focused matrix passed 9/9 under Qt 6.10.2. `BPR-0028` is in `60-done/` with the
provider-free atomic projection, accessible GitHub Qt window, stable
incremental conversation model, three-scale rendering, fresh macOS 38/38 and
core-only 17/17 matrices, sanitizer 4/4, and Ubuntu x86_64 host/scale 4/4
evidence. `BPR-0038` is in `60-done/` with the fail-closed decision to keep
query-bearing HTTP redirect targets rejected. `BPR-0029` is in `60-done/` with
explicit GitHub.com/GHES profiles and provenance, multi-profile provider
routing, native HTTPS and platform-vault selection, Qt-owned sensitive input,
hostname-bound v2 authority, orderly exact credential deletion, visible
zero-launch CLI unavailability, public GitHub.com/GHES application fixtures,
and a tests-disabled Release seam-absence contract. Its pinned Linux v2 matrix
ran at the BPR-0029 snapshot in an Ubuntu 26.04 x86_64 full-system QEMU guest,
including the native vault, both public fixtures, Release seam absence, exact
cleanup, and dependency/runtime/ELF audits. `BPR-0030` is in `60-done/` with
exact
recorded-method and bounded GraphQL operation policy, canonical-body
backstop, exhaustive provider-failure mapping, a checked-in deterministic Qt
property corpus, and the current 32-scenario native HTTPS verifier. Its final
macOS matrices passed 44/44 and 17/17 plus sanitizer refreshes; its Ubuntu
26.04 x86_64 GCC 15.2 build completed 83/83 targets and the complete
non-native matrix passed 41/41. `BPR-0031` is in `60-done/` with the interim
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
`60-done/` with the current security/authority audit, exact one-row/two-row CLI
self-test, tests-disabled Release symbol gate, native macOS checks, focused
sanitizer matrix, and corrected evidence boundaries complete. BPR-0033 is in
`60-done/` with the immutable-base documentation impact review, corrected native
appearance record, operator-safe troubleshooting, resolved documentation
ledger, and workflow-absence gate complete. BPR-0034 is in `60-done/` with
behavior-preserving identity, capability-cache, accessibility-text,
CLI-automation, and Release-contract simplifications verified by complete
available-platform and focused sanitizer matrices.

BPR-0035 and BPR-0036 returned to `10-backlog/` with the rest of the Windows
set when [[BPR-0358]] recorded that this host is a real Windows 11 x64
machine. BPR-0036 still retains direct real Windows x64 evidence and
BPR-0035 still cannot mark RFC-BPR-0002 Auto-Verified until that evidence
exists; what changed is that the evidence is now reachable here, behind
BPR-0347 and BPR-0350 rather than behind hardware. By product-owner direction, this does not block
planning and implementing later RFCs in canonical order.

RFC-BPR-0003 remains open for explicitly deferred closeout claims while the
replacement RFC-BPR-0004 is the next implementation frontier by product-owner
direction. BPR-0039 is in `60-done/` with the
immutable-base architecture, authority, claim, platform, UX, documentation,
performance, and security plan recorded in the
[RFC-BPR-0003 verification ledger](../verification/RFC-BPR-0003.md).
BPR-0040 is in `60-done/` with the dedicated epoch core/host, account-isolated
bare cache, app-owned protocol-v2 and credential-free `index-pack`, versioned
ref/catalog, exact cause/alias, recovery, Qt projection, lifecycle, and
no-impact architecture selected. BPR-0041 is in `60-done/` with the compact
deterministic epoch reducer, exact identity/completion policy, explicit
catalog-bound projection clears, and direct core evidence complete. BPR-0042
is in `60-done/` with the hardened installed-Git local process boundary, exact
spool consumption, bounded lifecycle, hostile canaries, and direct macOS
evidence complete. BPR-0043 is in `60-done/` with the app-owned
protocol-v2/native-HTTPS, authority-partitioned bare-cache, and retained-ref
component slice verified through commit `2801241`. BPR-0044's durable
epoch-catalog and operation-record persistence slice is in
`60-done/`, with fresh macOS Debug, core-only, sanitizer, tests-disabled Release,
and guarded-Windows cross-compile/link evidence complete. BPR-0045 is in
`60-done/` with the source-free application host, production entropy/clock ports,
exact append/recovery sequencing, repository FIFO/session isolation,
stale/cancel rejection, projection containment, and quiescence directly
verified. BPR-0056 is in `60-done/` with the single revision-bound core cursor,
bounded intent/request/terminal algebra, repository-actor catalog execution,
sanitized incremental page projection, stale/revision/cancellation rejection,
and direct core/manual-executor/SQLite evidence complete. BPR-0055 is in
`60-done/` with deterministic core authorization, exact durable prune/cache-delete
orchestration, every maintenance restart phase, fault-injected failure policy,
incremental availability projection, and real installed-Git/SQLite composition
evidence complete. BPR-0046 is in `60-done/` with the provider/effect-free stable
epoch model, independent exact selectors, alias actions, full OIDs, explicit
availability/history states, accessibility, native-System inheritance, and
three-scale rendered loaded-window evidence complete. BPR-0047 is in `60-done/`
with the exact accepted GitHub handoff, revocable smart authority, concrete
Git/cache/catalog/epoch/Qt composition, two-process GitHub.com/GHES
persistence and cache reuse, tests-disabled Release exclusion, and quiescent
shutdown directly verified. BPR-0048 is in `60-done/` with the manifest-bound
sibling-head force-push fixture, public retained-old-head/cause evidence,
issued-transport revocation, broad combined 25/25 matrix, and fresh sanitizer
coverage complete. BPR-0049 is in `60-done/` with refreshed scale-specific,
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
direct reruns, and independent accuracy review. BPR-0066 is now in `60-done/`
with core-authorized newest/older navigation, one 100-epoch/200-anchor Qt
page, stale-page invalidation, out-of-window exact selection, 1,000+ page
traversal, and real rendered/restart evidence complete. BPR-0052 is now in
`60-done/` after removing the unused catalog-replacement payload, extracting the
application timeout snapshot, deliberately retaining fail-closed authority
structure, and replaying the clean developer, core-only, sanitizer,
tests-disabled Release, documentation, and rendered integration matrices.
BPR-0053 and BPR-0054 are in `10-backlog/`; their real Windows x64 execution
is reachable on this host once the suite it must run stops failing. Done BPR-0067 now exposes confirmed prune/cache-delete commands
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
BPR-0053 reserves target-native Windows evidence for a real Windows machine
without blocking available-platform implementation or later RFC work. That
machine is now this host, so the reservation is a queue position rather than
a hardware wait. BPR-0054 remains blocked by BPR-0035/BPR-0036, BPR-0053, BPR-0069,
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
card is also in `10-backlog/` because it depends on BPR-0054. Its
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
`10-backlog/`. Requirements that need target-native Windows x64 execution, an
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
a credential, or a network service. Its inherited owner-attended gate,
BPR-0100, is accepted against both its historical v003 executable and the exact
final-head v004 executable. It now waits in Backlog behind BPR-0083 for terminal
identity replay; BPR-0101 still makes no closure claim because the ordered
prior-RFC chain remains open.

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
focus, so the final-head v008 run required an attended frontmost transition.
That run is now returned: strict owner return and the System/explicit behavior
pass, while an attributable zero geometry-rebuild delta fails the probe's two
exact-one expectations. Done BPR-0391 and BPR-0410 close the wait and diagnostic
slices. BPR-0220's exact-zero repair is complete and owner-accepted. BPR-0114's
current checkpoint is accepted; it remains open only as the terminal procedure
behind BPR-0101.

Done BPR-0084 follows the completed host-execution slice and modernizes
the shared build foundation before the remaining RFC-BPR-0004 Qt work. It
raises the CMake baseline, centralizes installed-package discovery for prebuilt
dependencies, replaces the tracked SQLite amalgamation with pinned verified
CPM source acquisition, and refreshes presets, direct dependency evidence, and
build documentation without adding an RFC-BPR-0014 package format or release
claim.

Done BPR-0253 retains the supplied BetterPullReview icon as the unchanged
canonical SVG, supplies deterministic PNG/ICO/ICNS platform assets, binds the
fixed PNG family to the running Qt application, embeds the Windows resource,
and gives tests-disabled macOS builds their bundle icon. The Linux desktop and
hicolor assets are retained for RFC-BPR-0014; no install, signing, or package
readiness claim is made.

Done BPR-0324 replaces that canonical source with the owner-supplied
`diff-icon-v1.svg`, regenerates the complete PNG/ICO/ICNS family, and replays
the source-digest, container, runtime-icon, tray, and macOS bundle checks. The
platform bindings and RFC-BPR-0014 package boundaries are unchanged.

Done BPR-0325 gives the application an explicit `BetterPullReview` display
name in both Qt and the macOS bundle metadata so the Dock tooltip no longer
falls back to the `better_pull_review` executable or bundle filename.

Done BPR-0326 optically fits the circular wave artwork to its Dock neighbors
and clips the complete icon to a rounded, transparent-corner mask. All tracked
PNG, ICO, and ICNS derivatives are regenerated from that revised vector.

Done BPR-0327 scales the complete rounded tile to 87.5% of its canvas after
Dock comparison showed that its outer footprint still exceeded neighboring
application icons. The resulting transparent margin applies consistently to
the SVG and every generated platform asset.

Done BPR-0328 names the tests-disabled macOS executable and bundle
`BetterPullReview`, closing the Dock-tooltip fallback that occurs when VS Code
starts the bundle executable directly under LLDB instead of through Launch
Services. Other platform and tests-enabled executable names remain unchanged.

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
BPR-0114/BPR-0115 retain attended and terminal closeout work. Done BPR-0260
restores canonical formatting for the merged compact-navigation composition
source and refreshes its exact integrity digests. BPR-0256's first packet was
invalidated by capability-critical review and split the remaining portable
authority/recovery work into BPR-0261, dependent done BPR-0262, and done
BPR-0263. Its v004 successor now binds committed BPR-0410 source, passes the
capsule/image gates and corrected hermetic matrix, and has a no-blocker fresh
capability-critical review. It is operator-ready for one exact decision. The
operator approved the narrowed replay and
authenticated-containment designs for implementation and hermetic verification
only; no live or package authority was granted. BPR-0261 is now in `60-done/`: one
explicit owner-private replay namespace carries approval identity, reset is an
operator command rather than a startup fallback, and the spend barrier is
`F_FULLFSYNC` after capability-critical review showed bare `fsync` could not
carry the durability claim on macOS.

Done BPR-0390 closes the defect the owner found by looking: a real C++ file
rendered monochrome while every composition marker stayed green. It cost two
corrections along the way — the card's first two diagnoses were both falsified
by its own later measurements — and the answer was adoption, not classification.
Spans and the diff result were two unordered deliveries into one pane, so
whichever landed second decided whether code was coloured. Adoption now happens
at display time against the comparison the surface is actually showing, and
`application_uiforetw_selected_diff_contract` fails if the selected comparison
renders unclassified, which is the check whose absence let the defect ship.
One blob still classifies after the automated course shuts down; that is
measured, explained, and left visible rather than asserted away.

Done BPR-0397 takes the three sibling waits BPR-0390 deliberately left. All four
now carry a deadline and a stop callback that can actually signal. Its negative
proof is the sharpest evidence on this board that the repair matters: reverting
one `wait_for` to `wait` makes `application_epoch_host` time out rather than
fail, because an unbounded wait does not produce a wrong answer — it produces
no answer at all.

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
notes/heads/defect metadata; done BPR-0239 and in-progress BPR-0240 own queue
filters/sorting and complete agent context/process lifecycle. Done BPR-0334
removes the duplicated portable protocol loop from the platform backends while
leaving BPR-0240's target-native Windows proof open. Done BPR-0223 and
BPR-0228 through BPR-0231, plus backlog BPR-0232, own the corresponding
composition; notification request/activation, the review ledger, round trip,
durable defect workflow, and honest observer-rule queue now reach the public
path. BPR-0120 through BPR-0132 supply the review ledger's receipt transitions, schema
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
  incremental landing model. Done BPR-0223 composes honest notification
  requests and exact local activation selection. Done BPR-0247 owns the
  portable explicit-agent invocation and real pinned workspace; done BPR-0250
  adds durable session identities plus explicit checked-note/current-draft
  selection. Backlog BPR-0232 remains the umbrella that requires BPR-0240's
  deferred native lifecycle proof. Component tests alone remain evidence for
  the components, not product reachability.
- BPR-0233 owns the recorded UIforETW selected-diff and intraline-pixel path.
  BPR-0187 serves the recording, but the current registered public test only
  opens the pull request and saves a window grab; it does not request or assert
  the diff.
- Backlog BPR-0235 replaces a structural live-mutation inventory with
  process-boundary default network denial before BPR-0142 can use a live
  provider. Done BPR-0236 supplies the promised appearance fallback when its
  durable store cannot open.
- In-progress BPR-0240 and done BPR-0237, BPR-0238, BPR-0239, and BPR-0334 own
  required component scope missed by the first review pass: theme
  customization/import, durable ledger state, queue filtering/sorting, and
  exact agent context/process lifecycle. BPR-0334 places decoder, follow-up,
  output-bound, and terminal policy in one portable fake-tested loop; only the
  native process primitives remain in the two platform transports.
- Done BPR-0234 hardens the closeout contract. BPR-0241 through BPR-0245 are
  the newly explicit terminal closeout cards for RFC-BPR-0007 through
  RFC-BPR-0011.

This allocation changes no RFC status and waives no closure dependency. The
review reconciled the older ledger prose after reproducing each final-state
finding and left the narrower truth wherever a required component or public
path remains absent.

### Open right now

With BPR-0223, BPR-0226 through BPR-0231, BPR-0234, and BPR-0236 through
BPR-0239 done, the live lane counts come from the card folders rather than a
duplicated number here. The groups below
describe the actual remaining authority or product surface; closure cards may
also depend transitively on more than one group.

#### Frontier roots

Most open cards are waiting on another open card. Only a handful sit at the
front of the graph with every dependency already in `60-done/`, and those are the
only places work can actually start.

This used to be a hand-maintained list, and it drifted — it claimed BPR-0262
was waiting on an operator statement that the card's own text records as
already given. The lanes carry it now. A frontier root is a card in
`40-parked/`, `50-blocked/`, or an active lane — including the per-platform
ready lanes — and the lane says which answer it needs:

| Root | Lane | Needs |
|---|---|---|
| BPR-0414 | `20-ready/` | Nothing. The owner named this gap on the 2026-08-21 verdict: the drawer does not anchor a comment to its line. Its wiring half is portable; the question of whether the body belongs in the code view goes back to the owner |
| BPR-0417 | `20-ready/` | Nothing. [[BPR-0416]] landed the depth, so what is missing is content — and the card now carries the measured cost of both routes to it |
| BPR-0420 | `20-ready/` | Nothing. The portable half of [[BPR-0232]], split out because that umbrella waits on Windows and this assertion does not |
| BPR-0036, BPR-0053, BPR-0311, BPR-0315 | `21-ready-win/` | A Windows session. The project's Windows 11 x64 host runs these; nothing else is in the way. [[BPR-0347]], [[BPR-0362]], [[BPR-0360]], and [[BPR-0350]] closed on 2026-08-22 with the contained matrix at 87 of 89; [[BPR-0036]] and [[BPR-0053]] wait only on that matrix's two carded failures and on the Qt graph. [[BPR-0424]] through [[BPR-0427]] are the four causes that run isolated from the container's own |
| BPR-0310, BPR-0314 | `23-ready-lin/` | **Not a Linux session.** A Linux session ran this lane on 2026-08-22 and removed every Linux reason it had. The tests-disabled Release build links here ([[BPR-0421]], [[BPR-0440]]) and `coding-tidy` runs here ([[BPR-0443]]). What is left is upstream and is not about this platform: [[BPR-0310]] needs RFC-BPR-0016 implemented at all — no `src/` file mentions the device flow — behind [[BPR-0276]]'s owner-attended capture and [[BPR-0318]]'s nonexistent distributable identity; [[BPR-0314]] must observe a typed public-preview-unavailability selection no code produces, and producing it is implementation RFC-BPR-0017 gates on RFC-BPR-0002, which waits on [[BPR-0036]]. BPR-0314 does carry three target-native rows now. A Linux session opening this lane should expect to close nothing |
| BPR-0240 | `30-in-progress/` | A Windows session for its remaining half. Its portable work has already been split out into done [[BPR-0334]] and backlog [[BPR-0332]]/[[BPR-0333]]; `windows_agent_process.cpp` has still never been compiled |
| BPR-0256 | `40-parked/` | A publicly observable designated GitHub target. The owner approved v004 exactly, but its lane failed closed before GUI creation; v004 must not be retried |
| BPR-0276 | `40-parked/` | Owner direction authorizing one recovery poll, then a real GitHub call |
| BPR-0298 | `40-parked/` | The operator's exact statement on the RFC-0017 probe packet, then five credential-free requests |

This table had drifted and is corrected above rather than quietly patched. It
listed BPR-0262 as a `20-ready/` root; that card is in `60-done/`. BPR-0220 was
the only Ready root and is now Done. It also omitted five parked cards: BPR-0391 and BPR-0392,
which parked on operator packets on 2026-08-19, and the three
`awaiting-product-decision` roots, which were reachable the whole time and
listed nowhere.

It drifted again, and [[BPR-0399]] is where that stopped being a matter of
diligence. The table omitted BPR-0310 and BPR-0314 — not stale, never listed —
although the definition four lines above it admits any card in `50-blocked/`.
The count was sixteen because those two were missing.

`scripts/verify_documentation.py` now derives this table's rows from the lanes
and fails when they disagree, so a card entering or leaving `20-ready/`,
`21-ready-win/`, `22-ready-mac/`, `23-ready-lin/`, `30-in-progress/`,
`40-parked/`, or `50-blocked/` cannot be omitted, listed under the wrong lane,
or left behind after it closes.

What the derivation cannot check is whether a lane is the *right* one, and
[[BPR-0413]] found two rows where it was not. BPR-0211 and BPR-0212 sat in
`40-parked/` as `awaiting-product-decision` while their own text, added the same
day, said the disposition had changed to "waiting on a deliverable that this
session owes" — the owner had asked to see mockups before deciding. Nothing was
waiting on a person. A deliverable was owed and no card owned it, so both cards
moved to `10-backlog/` behind [[BPR-0413]], and returned here when it landed the
page. They were parked again when it landed, unparked once
more when an independent review falsified the page's central cost claim before
the owner saw it, parked a third time once [[BPR-0413]] had corrected the batch,
and unparked for the last time when the owner answered it the same day. That is the
same reading that brought [[BPR-0256]] back to an active lane, and it is the
caveat this section already states two paragraphs down: a card whose next action
is unattended does not belong in the parked lane. The churn is the rule working, not drift — the lane
moved each time because what these cards were waiting on genuinely changed, and
the one move that looks like thrash is the one that matters: a page with a false
cost claim was stopped before it reached the person it would have misled.

The verdict came back the same day. The Treeverse map is chosen; [[BPR-0211]] is now the
umbrella for it in `10-backlog/`, broken into [[BPR-0416]] for the reply edge,
[[BPR-0417]] for a conversation with real depth, and [[BPR-0418]] for the map
surface itself; the whole-review view is recorded *not chosen*
rather than rejected, so it stays available without being on the record as
defective. The revision-axis RFC is authorized, and done [[BPR-0415]] wrote it:
`RFC-BPR-0021` is registered `Deferred` behind RFC-BPR-0002, RFC-BPR-0003, and
probe evidence that a real service returns a per-comment commit — a gate the RFC
records rather than assumes, because the review-thread selection this product
issues never asks for one. [[BPR-0212]] is done, superseded by the RFC that now
carries it. The git-graph drawer is
declined — "Don't see the need at this time" — which closes a request that had
been open since 2026-08-08 and that no card had ever put back in front of the
person who made it. And the owner named a gap nobody had carded: the shipped
drawer does not anchor a comment to its line, which is [[BPR-0414]].

The table is the complete frontier rather than a selection from it. Every open
card on this board is either one of these thirteen roots or reaches one
transitively — checked by deriving it from the lanes rather than asserting it:
of the 56 cards in `10-backlog/`, **none** has all its dependencies in
`60-done/`.

The post-BPR-0334 audit on 2026-08-21 found no backlog dependency on that
card, so its move to `60-done/` promoted nothing. All 56 backlog cards still
have at least one direct dependency outside the terminal lane.

So the honest summary is narrower than the one this section used to give.
That summary was written when hardware was the largest single blocker. It no
longer is: a Windows host arrived, and the accepted v006 attended batch closed
BPR-0233, BPR-0257, and BPR-0258 together.

The caveat this section used to carry — "'this host' is the project's, not any
one session's", so Windows work stays in active lanes as a queue position — is
retired by [[BPR-0419]]. It was true and unhelpful. A macOS session still had to
read every card to discover it could start none of them. The lanes say it now:
the Windows program sits in `21-ready-win/`, which is a queue for the machine
that can run it and an empty shelf for every other session.

This section used to draw a stronger consequence: **"No unattended work on this
board is executable from a macOS session today."** [[BPR-0402]] retired that
sentence, and the counterexample was already in the log when it was written. It
landed at `7dd4641`, 2026-08-19 21:30:10. The next three commits — `365c662`,
`e07183d`, and `0c4b3b7` at 21:44:38 — are unattended macOS work on this board,
and they are the commits that retired five of BPR-0347's thirteen taxonomy rows.

What is true is narrower, and it is about *execution*, not about work. **No
target-native execution and no attended verdict is available from a macOS
session.** The three Windows active-lane roots cannot run their suites here, the
parked roots need a person, a product decision, or a live GitHub call, and the
two blocked ones need Linux.

What that leaves is not nothing. A recorded claim can be re-read against the
commits that landed after it from any host, and on the current evidence that is
the highest-yield portable work on this board: it has retired five taxonomy rows
and three board claims across two sessions without a single Windows run. A card
whose *next* action is unattended also does not belong in `40-parked/`, which is
how [[BPR-0256]] came back to an active lane. A macOS session that finds itself
with nothing to do here should check that reading before accepting it.

Everything else in `10-backlog/` reaches one of those roots transitively.

| Group | Cards | What is actually missing |
|---|---|---|
| Requested Dif mascot program | Done BPR-0286 through BPR-0294, backlog BPR-0295 | RFC-BPR-0019 tooling, assets, exact rights and visual verdicts, clean dev/core/tests-disabled boundary verification, immutable-base documentation reconciliation, and behavior-preserving simplification are complete. The accepted verdict preserves the owner's requirement that the plus/minus card not become inseparable from Dif's core base sprite. RFC-BPR-0019 closeout remains blocked by non-waived RFC-BPR-0006/RFC-BPR-0013 closeout; RFC-BPR-0020 cannot begin before RFC-BPR-0019 is Auto-Verified |
| Requested post-pack device authorization | Done BPR-0274/BPR-0275, parked BPR-0276, backlog BPR-0277 through BPR-0285 plus BPR-0318, blocked BPR-0310/BPR-0311 | RFC-BPR-0016 remains Deferred. The private owner-only BPR-Test identity is limited to attended verification; BPR-0318 records a separate distributable identity before any production platform can become available. Bounded real-service capture, RFC-BPR-0002 closeout, implementation, target-native enablement, and capability-critical final replay remain explicit; no partial device-flow mode exists |
| Requested post-pack unauthenticated public read | Done planning BPR-0296, scoping BPR-0297, and demonstration BPR-0313; parked BPR-0298; backlog BPR-0299 through BPR-0309 plus BPR-0312, BPR-0316 and BPR-0317, blocked BPR-0314/BPR-0315 | RFC-BPR-0017 remains Deferred. Live-tree scoping is corrected: the read catalog has no REST review-comment operation, so refusing GraphQL anonymously loses every inline review comment rather than only resolution state, and the mutation authority's own REST review-comment read is recorded as the exception no read path can select. The credential-free transport demonstration exists as a `BUILD_TESTING`-only loopback course with a standing inventory and Release-absence guard; it creates no anonymous profile. Still required are real anonymous-service probes, an explicit positive build gate, RFC-BPR-0002 closeout, full public diff authority, macOS enablement, Linux/Windows unavailability proof, implementation, and capability-critical final replay. `native_designated_review_public_observer` is product-compiled and does perform a credential-free GET, but it duplicates rather than exercises the transport's rules and is not product evidence |
| Real Windows x64 hardware | Answered. In-progress BPR-0347 and BPR-0358; backlog BPR-0036, BPR-0053, BPR-0235, BPR-0311, BPR-0315, BPR-0332, BPR-0350, BPR-0360, BPR-0364, BPR-0412, and transitively BPR-0035, BPR-0054, BPR-0083 | A Windows 11 x64 host answered the blocker, and BPR-0347 has already run target-native on it. Target-native execution is still required and no available-platform substitute counts; what is gone is the wait for the machine. The last target-native matrix predates several repairs, so BPR-0347 owns a fresh core-only replay and any remaining failures, BPR-0412 owns the first Qt-graph run under containment, and BPR-0364 follows that run for the composed-host verifier. BPR-0362/BPR-0360 and BPR-0350 retain the owner-private catalog/temp-root chain |
| A Linux host | Done BPR-0156 and BPR-0235's Linux half; blocked BPR-0310 and BPR-0314 | An Ubuntu 26.04 x86_64 host answered the blocker. BPR-0156 re-established the matrix and BPR-0235's Linux criterion is discharged. BPR-0310 and BPR-0314 stay blocked on their own unmet backlog dependencies rather than on the platform. The run opened BPR-0341 through BPR-0345 |
| A designated live GitHub pull request | BPR-0142, BPR-0143, BPR-0144, BPR-0169, and BPR-0153 | Disposable `MikeMarcin/BPR-Test#1` is identity-bound; the live provider, exact-target lane, owner-authenticated mutations, and attended/mixed-client checks remain |
| Designated live-lane proof blockers | Done BPR-0261/BPR-0262/BPR-0263/BPR-0264, parked BPR-0256 | The approved narrowed replay authority is implemented and independently reviewed. The owner approved v004 exactly, but the lane failed closed before GUI creation; an immediate unauthenticated diagnostic returned HTTP 404, the approval remained unconsumed and was deleted, and no mutation occurred. A publicly observable target and complete successor packet are required |
| Owner-attended or native-display judgment | Backlog BPR-0100, BPR-0114, and BPR-0168; done BPR-0210, BPR-0220, BPR-0251, BPR-0391, BPR-0392, and BPR-0410 | Visual v004, notification v006, and appearance v008 establish the native behavior. BPR-0220 corrected the one stale appearance expectation to the measured zero, and the owner accepted the narrow repair without redundant digest-only reruns. BPR-0114 remains open only as the terminal procedure behind BPR-0101 |
| Product composition | Backlog BPR-0232; done BPR-0223, BPR-0226 through BPR-0231, BPR-0247, and BPR-0250 | Notification request/activation, the workbench host, selected diff, ledger, actionability, fixture-safe review round trip, durable defect workflow, observer-rule queue, portable agent invocation, durable sessions, and explicit private-context selection are composed. BPR-0232 waits for deferred native Windows lifecycle proof and its final replay through that same surface. |
| Missing component and resilience scope | Backlog BPR-0235; in-progress BPR-0240; done BPR-0237 through BPR-0239 and BPR-0334 | Complete target-native live-test isolation and the required agent contract; theme customization, durable ledger state, queue filtering/sorting, BPR-0236's appearance-storage fallback, and the single portable agent protocol loop are done. Native Windows transport proof remains with BPR-0240 |
| Recorded selected-diff proof | Done BPR-0233, BPR-0246, and BPR-0214; BPR-0100 waits on BPR-0083 for terminal replay; still gating BPR-0210's margin analysis | The real selected-file course binds UIforETW commits/blobs, named replacement pairs, semantic roles, and a cut-wire negative. After the v001 through v004 rejections and the accepted v005 connector repair, the combined v006 batch was accepted whole on 2026-08-18; BPR-0100 accepted the assembled workbench, and no interim presentation rerun is required for BPR-0220's probe-only change |
| Closeout rigor and terminal cards | Done BPR-0234 and BPR-0248; backlog BPR-0241 through BPR-0245 | The standing shipped-binary composition inventory is reconciled with exact fixture/production profiles; replay each anchored executable checklist and retain a live terminal card for each Proposed RFC |
| Deferred or not a slice | BPR-0101, BPR-0115 | Blocked by closure ordering. BPR-0211 and BPR-0212 are no longer examples here: done [[BPR-0413]] rendered the mockups the owner asked for, so both are answerable decisions rather than deferred ideas. BPR-0156 is no longer an example here: the Ubuntu host arrived, it re-established the matrix, and it is in `60-done/` |

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
