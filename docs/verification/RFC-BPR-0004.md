# RFC-BPR-0004 Verification Ledger

RFC base: `243295a70756b2e63a785ed11b14647b6b0a4c6d`

Immutable replacement gate: `0ec1136a7ce418b11480d7ae90faee2ec59f96d1`

Replacement source: `betterpullreview-rfc-bpr-0004-diff-alignment-scroll-replacement-v1`

Status: implementation active. BPR-0072 through BPR-0075 supply the canonical
record, deterministic computation, one semantic projection, and bounded host
seam. BPR-0076 through BPR-0079 supply live-pane measurement, one retained
comparison position, custom link-map painting, selected application
composition, installed-Git/cache-session integration, rendered public-path
evidence, and shutdown safety. BPR-0080 supplies bounded moved-sub-block
discovery, retained semantic indexes, and instrumented large-fixture/high-
frequency Qt evidence. BPR-0081 reconciles documentation and conformance;
BPR-0082 supplies the conservative simplification pass. BPR-0083's final
available-platform selected-composition replay now supplies 19 rendered
frames and a machine-readable two-direction interaction trace; its
owner-activated native System-theme transition and predecessor closeout remain
open. RFC-BPR-0003
retains explicit nonblocking closeout cards, so this RFC cannot become Auto-
Verified until that dependency can close.

## Final-state correction from BPR-0225

The selected diff components and fixture-driven composition below are real,
but the phrase `public path` was too broad. At the reviewed head the only
caller of the composition's `request_diff()` is the
`BPR_BUILD_TESTING`-only `github_application_fixture_access::request_fixture_diff`.
No shipped window gesture derives a selected file comparison and calls it.
`application_diff_workbench_public_path` therefore proves the real epoch/cache,
installed-Git, diff-host, Qt projection, interaction, rendering, and shutdown
path *after a fixture helper injects the request*; it does not prove a reviewer
can select a file and reach that path.

BPR-0227 is now a mandatory RFC-BPR-0004/0005 integration blocker. Every older
statement below calling that test a genuine user public path is read with this
correction. The semantic, host, rendered-component, and lifecycle evidence is
not retracted; the missing gesture-to-request surface is.

## BPR-0072 canonical-record evidence

The public core model now provides retained normalized diff options and their
digest, a full-authority exact file-comparison cache key, half-open spans and line/boundary
references, ordered alignment segments, contiguous group intervals and
summaries, line pairs, separate move metadata, typed diagnostics, and
domain-separated SHA-256 record identities. `core_diff_model` directly covers
empty/equal/insertion/deletion/replacement/mixed/move tables, an entire added
file without fake source lines, `UINT32_MAX` boundaries, malformed invariant
mutations, stable replay, and 512 fixed-seed valid/malformed cases.

The structural validator closes the BPR-0072 portion of D-02 and D-03: both
documents must be covered exactly and monotonically, and one-sided changes
must retain an empty opposing span. It also validates record identity, group
membership/summary, pair order and option policy, equal-extent one-to-one move
topology, diagnostics, and bounds. It
does not inspect original line text, so exact normalized equality, primary
operation canonicalization, deterministic secondary-pair construction, and
intraline ranges are supplied separately by BPR-0073.

## BPR-0073 deterministic-computation evidence

The public `compute_file_diff` boundary consumes exact optional byte spans
under the BPR-0072 comparison key and normalized options. It retains exact
display bytes and logical line/terminator metadata, rejects malformed UTF-8,
classifies NUL as binary, distinguishes absent from empty blobs, enforces
explicit byte/line/matrix/record bounds, constructs one ordered semantic path,
and validates every text result before publication. Accepted similar pairs
alone receive original-line-relative intraline ranges; every endpoint is
directly checked as an original UTF-8 boundary. The public computed-result
validator also rejects retained-document topology/count/presence mismatches,
orphan/exact-pair/out-of-bounds/nonmonotonic intraline mutations, and any
retained text records on a non-text result. Moves remain monotonic
delete-plus-insert segments with separate metadata.

The clean core-only, developer, and Darwin ASan/UBSan matrices each pass
`core_diff_compute`, `core_diff_model`, both core dependency/boundary tests,
and both architecture boundary tests. The computation fixture covers exact
empty/equal/start-middle-end insertion/deletion, unequal replacements,
internal exact/similar anchors, deterministic repeated-line replay, weak
rejection, whitespace and LF/CR/CRLF/final-newline policy, multibyte and
malformed UTF-8, absent/binary/unavailable states, stable file identity,
unambiguous/ambiguous moves, exact and exceeded limits, and generated replay.
Windows execution remains intentionally open with the repository's existing
target-native Windows evidence cards and does not block this slice.

BPR-0073 supplied conservative whole-segment move metadata. BPR-0080 replaces
that scan with bounded/indexed moved-sub-block discovery; this section does not
retroactively claim those later scale/completeness properties for BPR-0073.

## BPR-0074 canonical-semantics evidence

The public `diff_semantics` boundary accepts only the validated BPR-0073
result. It maps equal and accepted replacement pairs exactly, reports similar
pairs distinctly, gives an unpaired replacement line the complete opposing
span, and gives a one-sided line the precise empty-side boundary. Exact move
correspondence remains separate metadata and never replaces that primary
mapping. The computed-result validator now also proves normalized equality for
equal segments, exact pairs, and move relations before any semantic projection
can publish them.

Change projection copies the canonical ordered group vector and exact member
slices; navigation reads the validated diff directly rather than trusting that
mutable projection. Line and group selections retain stable segment/group
identity, lower detail re-resolves the canonical mapping, and nontext,
comparison-mismatched, out-of-range, mutated, or fabricated input fails closed
without forwarding caller-controlled identity or diagnostics.

The clean core-only, developer, and Darwin ASan/UBSan focused matrices each
pass 7/7, including `core_diff_semantics`, `core_diff_compute`,
`core_diff_model`, both core dependency/boundary tests, and both architecture
boundary tests. The semantic fixture covers every mapping/detail kind,
one-sided and unpaired states, 4:7 anchors and tail boundaries, canonical
group order, stop/wrap navigation, selection preservation/clearing, separate
moves, fail-closed mutations, and a generated small-file/options corpus.

BPR-0080 supplies indexing and bounded high-frequency semantic lookup work.
BPR-0074 itself does not claim Qt geometry, scrolling, ribbon painting, or
composed visual agreement; those direct surfaces are proven by BPR-0076
through BPR-0080.

## BPR-0080 scale and bounded-interaction evidence

The public deterministic move path now indexes normalized lines from all
one-sided primary segments. Only anchors occurring once on each side may seed
a move; accepted anchors extend to maximal exact unclaimed runs and split only
their source deletion and destination insertion segments. Repeated ambiguous
text therefore remains unrelated, while a unique two-line block beside
unrelated deletion/insertion text receives stable separate relation metadata.
The primary alignment remains ordered delete plus insert. A deterministic
structural budget charges bounded map probes, sorting, equality probes, fills,
and refinement; the exact budget succeeds, one unit below fails closed with no
move metadata, and a separate segment ceiling reports refinement refusal
without mislabeling it as work exhaustion. The application host rejects zero
or product-maximum-exceeding move-work limits.

`diff_semantic_index` performs one complete computed-result validation, then
retains sorted segment/group/pair/move identities and per-pair intraline spans
beside that immutable result. Indexed mapping, move resolution, detail,
projection, navigation, selection, reconciliation, and interaction transitions
return the same semantic answers as the fail-closed unindexed overloads. The
composed workbench builds one index per accepted publication and performs no
semantic query or index rebuild during ordinary scroll or resize.

The Qt scale fixture publishes 2,000 lines per side containing 250 small
changes (500 semantic segments), dispatches 1,000 pixel-wheel inputs before one
event drain, issues 100 resizes before one event drain, and separately
publishes a six-relation selected move group and a 5,000-line one-sided
insertion. Direct counters prove the exact summed/clamped subpixel position,
1,000 applied frames with one delivered repaint, zero semantic queries/index
rebuilds/axis rebuilds/pane measurements during scroll, one two-pane
measurement/rebuild with zero semantic queries after the resize storm, and
unchanged semantic selection. The link map uses four binary range searches per
frame, merges the already ordered left/right candidate ranges, uses retained
visibility marks for constant-time move endpoint resolution, and grows no
retained hot-path capacity. Peak and cumulative candidate, path-build, and
paint counters constrain every rapid-input frame rather than only its final
frame; the move-enabled selected fixture resolves all six visible overlays.

The selected headless workbench and link-map executables passed serially at
1.0x, 1.25x, and 2.0x under macOS Sequoia 15.7.7 arm64, Qt 6.11.1, and 14
reported ideal threads. The factual workbench timings were respectively
59/8/1 ms, 59/5/1 ms, and 59/22/1 ms for publish/input dispatch/event drain;
they are observations of this machine, not frame-rate guarantees. Directly
inspected rasters are in `build/dev/tests/adapters_qt/`: workbench images are
1080x700, 1350x875, and 2160x1400, and link-map images are 1240x520,
1550x650, and 2480x1040. The selected core compute, core semantics,
application-host, workbench, and link-map executables pass. Target-native
Windows execution remains open and nonblocking; offscreen forced-scale output
does not claim native System-theme integration or the RFC-BPR-0005
presentation-polish pass.

## BPR-0075 bounded-host evidence

The public `diff_transition` core boundary owns compact exact request,
application-session, generation, full comparison-key, cancellation,
invalidation, and acceptance state. Every newer request first revokes older
pending work, including when the replacement key, options, or algorithm is
invalid. The application host alone materializes blobs, owns immutable worker
bytes, invokes the fixed supported algorithm revision, validates the exact
key/options/digest/result, and returns typed completions to a separate
serialized decision lane.

The exact cache includes full authority, repository, pull request, both
anchors, both optional blobs, stable file identity, option digest, and
algorithm revision. Jobs, entries, retained capacity bytes, compute limits,
and pre-allocation materialization requests are bounded. Cache hits remain
serialized; authority invalidation clears core, cache, and presentation state.
Fixed-snapshot feedback prevents projection callbacks from nesting delivery or
monopolizing one application turn, and the lifetime gate rejects terminals
after host destruction.

Validated binary and unavailable computation records publish with their
canonical content state and diagnostic rather than being replaced by fake
text or discarded. Transient port, missing-object, generated-policy,
cancellation, and host failures remain exact typed failures. The host checks
for stop before acquisition, between sides, and after pure computation; the
architecture permits that cooperative cancellation to suppress publication
without adding nondeterministic observation to the functional core.

The distinction is deliberate: binary or unavailable bytes that reach pure
computation produce a validated nontext result, while blob-port missing,
generated-policy, cancellation, or materialization failures publish no
fabricated computed result.

The deterministic `core_diff_transition` and `application_diff_host` targets
cover cache hit/miss and identity collisions, reordered and same-key
generations, malformed replacement requests, cancellation stages, shutdown,
pending-effect destruction, fixed-snapshot reentry, byte/job/cache bounds,
absent-versus-empty sides, nontext results, typed port/compute failures,
authority invalidation, and projection exceptions. The real installed-Git
blob materializer/cache-session arbitration and destroyed-Qt composition remain
explicit BPR-0079 work; BPR-0075 makes no selected-application-path claim.

## BPR-0076 measured-axis evidence

The public Qt adapter now measures accepted logical-line boundaries from a
live natural-layout `QPlainTextEdit` without adding source rows. Measurement
walks the document blocks once, treats Qt's optional trailing visual block as
at most the final semantic boundary, and retains the pane's document extent,
viewport extent, and device-pixel ratio as Qt-local facts.

The immutable measured axis consumes only a structurally validated text
alignment plus finite monotonic pane measurements. Every semantic segment ID
is retained. Each comparison extent is the maximum of the two measured side
extents; an empty side therefore projects to one constant boundary, and
unequal replacements converge at one comparison endpoint. Zero-height
segments remain inspectable but do not enter the positive-extent lookup index.
Projection uses an indexed segment lookup, clamps to the exact start/end
domain, and never divides by zero. The cache clears invalid geometry,
coalesces accepted-diff, file/epoch, font, zoom, line-spacing, tab, DPI/screen,
viewport/divider, wrap/fold, document-layout, and variable-height invalidation
facts, and advances one generation only after a successful rebuild.

The three selected Qt-scale tests pass at 1.0x, 1.25x, and 2.0x. They directly
cover continuity, right-insertion/left-deletion pinning, 4:7 and 7:4
replacement ratios, empty and zero-height geometry, endpoint reachability,
invalid-measurement rejection, stable semantic identity, coalesced
invalidation, independent viewport heights, and live font/tab/resize/DPI
remeasurement. Each test generates a live two-pane raster; the 1.25x and 2.0x
artifacts were directly inspected. These results close only the Qt-local
mathematical and measurement portions of Q-03 through Q-05, Q-08, Q-14, Q-15,
and A-02.
They do not claim a retained shared `q`, wheel/trackpad equivalence, scrollbar
authority, painted ribbons, connector attachment, or public workbench
composition; those claims remain on BPR-0077 through BPR-0079. Target-native
Windows execution remains open and no GitHub or GHES mutation is performed.

## BPR-0077 shared-controller evidence

The Qt adapter now owns one finite subpixel comparison position and accepts
vertical input only through that authority. Event filters installed on real
left-pane, right-pane, and link-map `QWidget` surfaces bind both text viewports
for wheel delivery and focused editors for Page/Home/End delivery. They prefer
pixel deltas, fall back to platform-scaled angle deltas, preserve begin/end
phases, and use Qt-delivered inverted values without applying a second
inversion. Identical input sequences over all three receivers therefore
produce identical retained position traces with no leader election or
receiver-handoff reset.

Every accepted position projects both panes through the BPR-0076 measured
axis, then derives independent document origins around a 0.43 viewport rail
with exact leading/trailing clamps. Frame publication is guarded against
recursive controller input. Repaint delivery is queued and coalesced. The
integer shared scrollbar is only a 100,000-step presentation projection;
retained `q` remains authoritative across pixel/angle input, Page Up/Down,
Home/End, scrollbar single/page/minimum/maximum actions and drag values, and
stable-segment reveal. Native discrete scrollbar actions carry their exact
`qreal` target across the Qt action/value signal pair, so opposing page or
single steps do not round-trip through the integer projection; a user drag
alone selects a new projected position. Same-comparison axis replacement
retains stable segment/fraction before recomputing origins from changed
geometry. Horizontal and Shift-wheel paths mutate only the receiving pane's
horizontal scrollbar, including inward residual accumulation at either bound.

The three selected Qt-scale controller tests pass at 1.0x, 1.25x, and 2.0x.
They install the controller on live event receivers and a real shared
`QScrollBar`, then directly cover identical left/right/link-map traces,
right-insertion and left-deletion pinning, 4:7 and 7:4 proportional motion,
1,000 forward/reverse subpixel cycles without drift, fine-angle retention,
Qt inversion and phase handling, continuous positive-axis travel, first/final
endpoint reachability, all controller and scrollbar navigation entries,
non-commensurate exact page/single reversals, same-comparison remeasurement,
serialized reentrant axis replacement/clear, callback exception/destruction
safety, recursive-feedback suppression, coalesced repaint, and horizontal
isolation. The same three runs build their axis from live `QPlainTextEdit`
measurements, send wheel events through real text viewports and Page/Home/End
through focused widgets, assert that neither native pane scrollbar becomes
authority, and confirm the selected DPR. A separate ASan/UBSan build passes
the same 3/3 matrix, including synchronous controller destruction from a
shared-scrollbar action callback.
The focused core/host/architecture/axis/controller matrix passes 13/13, strict
analysis and formatting pass, and no Qt scroll state appears in core or
application code.

These results close the controller-level portions of Q-01 through Q-09. This
slice performs no custom painting and makes no ribbon, rendered-workbench,
system-theme, public runtime, or presentation-polish claim. BPR-0078 retains
link-map paint and hit testing; BPR-0079 retains selected rendered composition
and integration traces; RFC-BPR-0005 retains its dedicated presentation-polish
and native visual-acceptance pass. Target-native Windows execution remains
open without blocking available-platform work.

## BPR-0078 DiffLinkMap evidence

The Qt adapter now provides one `diff_link_map` custom-painted
`QSplitterHandle` through `diff_comparison_splitter`. It consumes an immutable
copy of the measured alignment axis, exact core move relations, and
same-comparison frames from the shared controller. The frame carries the exact
file-comparison digest; a missing, different-comparison, nonfinite, or negative-
origin frame is rejected without replacing visible geometry. The handle never
owns a second scroll position, semantic diff, line-pair heuristic, or child
widget per ribbon.

Accepted geometry retains only changed entries in semantic order. Separate
ordered left- and right-interval searches find the union of visible and near-
visible candidates; painting and path-derived hit testing consume cached
visible paths rather than rescanning the complete segment vector. Replacement
paths open on both pane edges. Insertion and deletion paths collapse only the
empty edge, producing filled cubic wedges. A single combined ordinary fill
avoids cracks between adjacent paths, while hover and selection add a palette-
derived outline/fill. Exact moved-from and moved-to relation IDs produce paired
markers at the precise source-left and destination-right interval midpoints;
only a selected or hovered relation may add one cached temporary dashed overlay
between those same endpoints, so ordinary correspondence remains monotonic and
noncrossing. Frame, resize, geometry-replacement, and clear paths reconcile the
last pointer position against rebuilt visible paths and publish a downstream
hover replacement or reset when required.

The direct Qt fixture uses an actual two-pane `QSplitter` with live natural-
layout `QPlainTextEdit` documents, the production handle, one shared scrollbar,
and the BPR-0077 controller. At 1.0x, 1.25x, and 2.0x it proves pane-edge path
coordinates, replacement/insertion/deletion topology, stable segment/group hit
identity after shared scroll, resize, and same-comparison remeasurement,
platform drag-threshold click-versus-resize behavior, hover/click/double-click,
stationary-pointer hover replacement after shared scroll, explicit downstream
hover reset after geometry clear, group-aware keyboard next/reveal, focusable
accessible name/status, one captured polite selection announcement, strongly
displaced move markers and temporary-overlay endpoints on their exact pane-side
intervals, absence of a normal crossing overlay, bounded near-visible candidate/
path counts, no geometry rebuild during ordinary scroll, and no synthetic source
blocks. Each run writes `diff-link-map-{100,125,200}.png` under
`build/dev/tests/adapters_qt/`; all three selected-composition rasters were
directly inspected for clipping, pane-edge seams, and scale behavior.

The same fixture changes the live application palette and sends an explicit Qt
theme-change event, proving that inherited presentation repaints while path
geometry and stable selection remain unchanged. This is palette-inheritance and
event-boundary evidence, not a target-native System-theme claim:
BPR-0079 retains the selected composed-workbench theme path, target-native
Windows remains open, and RFC-BPR-0005 still owns the planned integrated
presentation-polish and native visual-acceptance pass.

## BPR-0079 selected-composition evidence

The composition root now constructs the production `diff_application_host`
with the accepted epoch host as its blob port and the real GitHub window's
`diff_workbench` as its projection. The selected workbench contains two exact
natural-layout `QPlainTextEdit` documents, the production link-map splitter,
one BPR-0077 controller and visible shared scrollbar, previous/next semantic
change commands, core-owned compact semantic selection, source-line
highlights, and one full-width lower detail pane. Qt retains only presentation
geometry, focus, hover, and rendered selection; it does not perform Git,
compute a diff, regroup changes, or decide navigation order.

Blob acquisition uses the installed Git and repository-cache adapters. The
process adapter first asks `git cat-file --batch-check` for exact object type
and declared size, rejects a non-blob or an object above the caller's retained
16 MiB ceiling, then reads exactly that many bytes under a per-process output
limit. The cache adapter validates its owned repository boundary before and
after the read. The epoch host queues the synchronous diff-worker request on
the existing repository actor, and admits it only when the complete requested
cache authority equals that actor's already retained lifetime-exclusive
session. It neither opens a competing session nor switches the actor back to a
stale PR; direct host evidence keeps the maximum active session count at one.

Accepted GitHub handoffs cancel and clear the selected diff generation only
when authority or anchor-driving semantic revision facts change; a
metadata-only refresh with identical facts preserves the exact result,
context, and semantic selection. Epoch comparison-selection intents still
cancel the selected diff. Whole-cache deletion invalidates the exact
cache authority before maintenance. The workbench admits a publication only
for its current exact loading context; exact failure, invalidation, and clear
close that context, so a late result cannot recreate geometry. Composition
shutdown closes the diff host first, closes epoch blob ingress, waits for the
diff, epoch, and GitHub hosts plus every cache actor to quiesce, then drains the
diff effects before the epoch effects and destroys Qt later by member order.

`qt_diff_workbench_scale_{100,125,200}` passes at 1.0x, 1.25x, and 2.0x. The
fixture sends wheel input through both source viewports and the divider; proves
right-insertion and left-deletion pinning, unequal-replacement proportional
motion, 1,000 exact reversals, shared-scrollbar movement, and routed
Home/Page/End against the native pane scroll values and captured viewport
pixels rather than controller frames alone. It covers stable-segment reveal,
pane-local horizontal motion, ribbon click, source-line click,
detail/link-map/navigation identity agreement, command-to-pane focus order,
accessible names/descriptions, one polite deduplicated workbench announcement,
and unchanged natural source-row counts. It also replaces file/epoch/options
contexts, delivers stale results and failures, changes the internal detail
splitter without resizing the outer widget, proves one coalesced viewport-axis
rebuild and native tail reachability, changes resize/font/DPI/palette facts,
and preserves stable semantic selection through Qt-local remeasurement.

`application_diff_workbench_public_path` traverses the recorded GitHub.com
public fixture, accepted epoch and retained real cache actor, installed Git,
diff host, production workbench, rendered full window, and orderly shutdown.
It replays an accepted metadata-only revision and proves that the exact
result/context/selection survive, then changes the accepted head and proves
that all three clear before replacement geometry can appear.
The generated `build/dev/tests/end_to_end/diff-workbench-public-path.png` was
directly inspected: the selected composition visibly contains the two source
documents, an insertion wedge attached to both pane edges, the selected later
line, the one shared scrollbar, and matching lower detail. The test emits
`PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:github.com`, `DIFF_WORKBENCH_OK`, and
`RENDER_OK` only when those runtime facts agree.

`application_diff_workbench_shutdown_public_path` begins shutdown immediately
after submitting a real selected diff and passes only after all three hosts
quiesce without a late Qt terminal. The existing three-process
`application_epoch_maintenance_public_path` now materializes and renders a
selected second-head diff before whole-cache deletion and requires the
workbench's exact result, context, and geometry to be synchronously invalidated
when durable deletion completes. Direct `git_process_contract`,
`git_cache_contract`, and `application_epoch_host` fixtures cover exact
SHA-1/SHA-256 and empty bytes, declared-size rejection before content
retention, wrong object type, imported-cache reads, one-session arbitration,
stale same-repository authority rejection, and closed blob ingress.

The target-native native-appearance public probe now requires this same
selected diff before capturing its initial full-window raster. A successful
Cocoa run additionally requires inherited palette/style/font state, stable
semantic selection, a post-theme geometry rebuild, and differing rendered
pixels. The deterministic probe-contract test passes, but it proves only that
invalid and offscreen invocations fail closed without writing evidence; it is
not System-theme evidence. The owner-activated Cocoa/QMacStyle Light-to-Dark
capture and target-native Windows execution remain open and nonblocking.
RFC-BPR-0005 still owns the dedicated integrated presentation-polish and native
visual-acceptance pass; BPR-0079 makes no claim to that later polish.

## BPR-0081 documentation and conformance evidence

The documentation-impact range begins at the original predecessor
`243295a70756b2e63a785ed11b14647b6b0a4c6d`. RFC-BPR-0004 implementation
changes the documented core diff record, computation, semantic index,
interaction transition, host request/cache/cancellation lane, installed-Git
blob materialization, repository-cache actor arbitration, Qt measurement and
scroll authority, link-map painting, selected workbench composition,
accessibility, limits, diagnostics, fixtures, and build/test commands. The
architecture, product UX, fixture catalogs, manual rendered protocol, master
matrix, tracker, and this ledger therefore have direct impact.

GitHub/GHES read and mutation semantics, credential ownership, the durable
review ledger, AI execution, and packaging policy receive no RFC-BPR-0004
semantic change. Their documents can appear in the predecessor-range inventory
because the replacement RFC pack and independent completed foundation work are
also in that history; they are not evidence that the diff implementation
changed those contracts. Downstream RFC-BPR-0005 and RFC-BPR-0006 consume the
one semantic model and selected composition, but retain presentation polish,
native visual acceptance, selectable themes, syntax presentation, and high-
contrast behavior. RFC files are immutable after replacement import: the exact
`0ec1136a7ce418b11480d7ae90faee2ec59f96d1..HEAD` RFC-tree diff must remain
empty.

| Scope | Disposition | Direct source/build/test/runtime evidence |
|---|---|---|
| Core diff records, computation, semantics, interaction, transition | Updated | `src/core` diff headers/sources; C1 passes 5/5 without Qt/providers |
| Application materialization, cache, cancellation, completion, shutdown | Updated | `diff_application_host`; C1 host cases and C3 shutdown contract |
| Installed-Git/cache actor and composition root | Updated | `installed_git_cache`, `github_application`, public fixture; C3 markers and selected-composition raster |
| Qt measured axis, shared controller, link map, workbench, accessibility | Updated | Qt diff sources; C2's 12 selected scale cases and inspected adapter rasters |
| Build/test instructions, fixtures, diagnostics, limits | Updated | Build guide and both fixture catalogs; C1-C4 plus documentation validation |
| Product UX and rendered/manual protocol | Updated | Diff/workbench UX contracts and manual protocol; C2/C3 geometry, stable-ID, and raster evidence |
| GitHub/GHES read/mutation and credential contracts | No RFC-4 semantic impact | Provider boundaries remain inward; recorded read fixture is reused by C3; no remote mutation occurs |
| Durable epoch/SQLite policy | No RFC-4 semantic impact | Existing epoch authority is consumed as a blob/cache capability; no catalog schema or durable decision is added |
| Review ledger, AI, packaging/release policy | No RFC-4 semantic impact | No RFC-4 intent, request, persistence, provider, or delivered-package claim enters these regions |
| Downstream RFC-BPR-0005/0006 contracts | Updated at import; immutable now | They consume the one model/composition and retain polish/theme work; post-`0ec1136a` RFC diff is empty |
| Native Windows, native System-theme, final replay/polish | Unresolved and assigned | BPR-0083 retains final selected-composition replay/native-theme evidence; RFC-BPR-0005 retains polish; Windows remains open |

The architecture verifier now treats composition-root worker-executor
infrastructure as a standard-C++ region with no direct Qt or concrete-provider
ownership. Its hostile contract rejects directly owned `QWidget`,
`QAbstractItemModel`, and provider objects, while the compiled worker fixture
proves host-owned effect closures run on the owned worker rather than the
calling thread. Existing boundary checks keep core Qt/provider/process-free
and keep Qt from owning provider effects. Source and documentation scans
retain one semantic mapping/index, natural source rows, one comparison
position and shared vertical controller, visible-set link-map work, and no
follower-scroll fallback or permanent crossing ordinary move ribbons.

The selected rendered/composed test command is explicit, headless, and serial
so it cannot fan out redundant application instances. BPR-0083 extends that
same selected public-path process with the final two-direction rendered replay
described below; it does not add a second application test. Target-native
Windows, the owner-activated native System-theme transition, and RFC-BPR-0005
presentation polish stay open and do not block available-platform conformance
work. GitHub Actions remain prohibited and both tracked and on-disk workflow
counts remain zero.

## BPR-0082 simplification evidence

The conservative RFC-BPR-0004 range review removes intermediate change-group
and nested intraline collections plus their replay traversals, builds the same
canonical identities and ordered edits directly, and avoids a second semantic
mapping lookup when line detail begins from an already canonical lookup. The
public caller-supplied mapping overload still recomputes and compares the
canonical mapping before returning detail, so malformed or stale mappings
continue to fail closed. The application host reuses one context-to-job search,
and the Qt adapter consolidates link-map identity matching while dropping
state assignments already guaranteed by exact-context admission or geometry
clearing.

The pass deliberately retains the two borrowed temporary semantic-index
adapters because a shared lifetime-bearing helper would add more private
machinery than it removes. It also rejects a privileged prevalidated
measured-axis route: retaining the one public validation path keeps authority
and failure behavior explicit.

The affected core model, compute, semantics, transition, and application-host
tests pass 5/5 in `build/core-only` and 5/5 in the ASan/UBSan
`build/bpr0077-asan` tree with:

```sh
ctest --test-dir build/core-only --output-on-failure \
  -R '^(core_diff_model|core_diff_compute|core_diff_semantics|core_diff_transition|application_diff_host)$'
ctest --test-dir build/bpr0077-asan --output-on-failure \
  -R '^(core_diff_model|core_diff_compute|core_diff_semantics|core_diff_transition|application_diff_host)$'
```

The selected Qt link-map/workbench scale cases and selected public composition
pass 8/8 with the exact bounded command below. After the final source edit, the
same command narrowed to the two `application_diff_workbench` cases passes 2/2
on the post-simplification source:

```sh
CTEST_PARALLEL_LEVEL=1 QT_QPA_PLATFORM=offscreen \
  ctest --preset dev --output-on-failure \
  -R '^(qt_diff_(link_map|workbench)_scale_(100|125|200)|application_diff_workbench(_shutdown)?_public_path)$'
```

Those exact invocations set Qt's offscreen platform and allow only one test at
a time; they are the only BPR-0082 Qt/application executions claimed here.
They write the component rasters below `build/dev/tests/adapters_qt/` and the
selected-composition raster to
`build/dev/tests/end_to_end/diff-workbench-public-path.png`. The independent
mechanical replay launched no Qt, GUI, end-to-end, or application test. Strict
touched-file clang-tidy and the repository formatting gate pass. This evidence
does not replace BPR-0083's owner-activated native System-theme evidence or
final rendered two-direction replay.

## BPR-0083 available-platform closeout evidence

The final selected public-composition fixture now supplies replacement,
deletion, insertion, matching, and asymmetric-tail regions through the frozen
installed-Git pack/cache/application path. The existing
`application_diff_workbench_public_path` process accepts one additional
test-build-only evidence-directory argument; no extra application test or
process was added. Its 19 full-window PNGs and JSONL trace prove initial
selection, matching motion, left-pane input, insertion pinning and reversal,
right-pane and divider input equivalence, deletion pinning, unequal
replacement, ribbon selection, divider resize, and both comparison endpoints.
Every trace sample records exact projected and applied pane coordinates,
selected group/segment identity, link-map visibility work, geometry
generation, and the unchanged semantic index generation.

The replay directly exposed a composition defect that the taller component
fixture had not made visible: the correspondence map received a zero document
origin after the native panes scrolled. The workbench now derives the applied
pixel origin from the vertical scrollbar value it actually set and publishes
that quantized origin to link-map paint and hit testing. A focused regression
requires nonzero applied origins for the late unequal replacement. In the
final rendered sequence, insertion and deletion are attached wedges, the 4:7
replacement is a filled attached ribbon, its click agrees with the semantic
selection and detail pane and advances exactly one selection callback, one
resize advances exactly one geometry
generation without rebuilding the semantic index, and the start/end frames
show both asymmetric document tails without dead travel.

The deterministic offscreen replay does not claim native System-theme
behavior. The test-only native probe records the selected workbench's inherited
palette and font before/after the real Light-to-Dark transition, stable semantic
selection, visibly differing full-window pixels, and an exact one-rebuild
geometry delta.

That probe has now been executed with explicit desktop-owner authorization on
2026-08-03 at commit `1e581a6`, on macOS 15.7.7 build 24G720 arm64 with
Qt 6.11.1. The owner authorized changing and restoring the global appearance
and supplied the activating click; the probe called no activation API and
synthesized no input. It reported `PUBLIC_PATH_OK`,
`GITHUB_FIXTURE_OK:github.com`, `GITHUB_FIXTURE_QT_PLATFORM:cocoa`,
`GITHUB_FIXTURE_QT_STYLE:QMacStyle`, `NATIVE_APPEARANCE_ACTIVE_FOCUS_OK`, both
local and global `Light->Dark` transitions, and the signal, state,
pixel-difference, changed-render, and all four workbench-specific
`NATIVE_APPEARANCE_DIFF_*` success markers. Neither evidence file existed
before the transition.

The recorded workbench palette moved from
`#ffececec:#ffffffff:#ff000000:#ff000000` to
`#ff323232:#ff171717:#ffffffff:#ffffffff`; the interface font was identical
before and after at `.AppleSystemUIFont,13`; and
`NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA` was exactly 1. Both rasters are
2560x2000. The Light frame has SHA-256
`7bc943363cd78c8342cece6ef22ec35e30b7e03f70886e8bc8cfabaf5af22742` and the Dark
frame `0d0010bd8b99c9e513bfd26903b6bf14493906cf1cd283a0f08d708cedeffcf4`. The
owner's original System Dark `true` was restored by the shell trap. The rasters
were retained only in a transient local directory and are not checked in.

Direct inspection of both frames confirms the same loaded pull request, the
same epoch rows and complete left/right OIDs, `Whole PR` still selected, the
selected-file comparison with `old-a` still highlighted, the same attached 4:7
replacement ribbon across `old-a..old-d` and `new-a..new-g`, the same
`Change 1 of 3` detail text, native Cocoa controls, and readable contrast in
both appearances.

One property of an attended run is recorded so a later reader does not
misread the frames. The two captures are not a palette-only delta: the outer
window scroll offset differs between them. This is owner interaction during the
attended probe, confirmed by the desktop owner, not a post-theme relayout
effect. The probe requires the owner to activate and focus the real window, and
the owner remains free to interact between the Light capture and the appearance
transition. The inner diff panes are identical in both frames, semantic
selection is stable, and every gate requirement is met. The probe deliberately
asserts semantic and geometry invariants rather than outer scroll position,
which an attended run cannot hold fixed.

Target-native Windows stays open and nonblocking. RFC-BPR-0004 cannot become
Auto-Verified or release RFC-BPR-0005 while RFC-BPR-0003 remains open.

The final rebuilt public-composition replay passes 1/1 in 12.90 seconds with
one offscreen process and no leftover application process. Its JSONL
records and PNGs were regenerated after the final simplification. The
ribbon-selection record proves one callback rather than inferring a click from
an already-selected identity; the following resize advances only the geometry
generation while the semantic index remains at one. The selected component
matrix passes 12/12; the refreshed ASan/UBSan selected matrix passes 7/7; the
RFC utility lane's Qt-free core and mechanical groups pass 5/5 and 11/11. The
format, changed-file strict-tidy, documentation, immutable-RFC, original
22-file replacement-pack, workflow-absence, and diff-hygiene gates pass.

The root review removed the unnecessary test-encoder reserve, moved the native
geometry baseline after the probe's own resize event drain, and required an
exact link-map callback on the rendered ribbon click.

An independent critical review completed after two earlier bounded attempts
returned no report. It falsified no RFC-BPR-0004 semantic or component claim,
confirmed the single retained position, pinning, exact reversal, proportional
advance, stable semantic generations, and inspected ribbon geometry. Its
conclusion that the replay was a genuine public selected-diff path is
superseded by BPR-0225: the fixture-access helper is the only final-HEAD caller
of the composition's diff request. The replay still directly verifies the
production host, cache, model, and rendered interaction after test-only
injection, but it cannot close the missing reviewer gesture owned by BPR-0227.

The review recorded one medium finding that is now closed: the evidence on the
card predated three later RFC-BPR-0003 commits that touch the epoch host
composed into the same process. The complete matrix was replayed at
`ca424f7` and passes 88/88 on `dev` in 685.29 seconds and 33/33 on
`core-only` in 83.27 seconds, including
`application_diff_workbench_public_path` at 12.36 seconds and the
tests-disabled Release composition contract. The superseded 00:48 counts are
replaced by that replay.

Its two actionable evidence-strength findings are closed by done BPR-0087. The
composed wheel steps now require the retained position to have advanced past
the input origin before the receiver-equivalence steps compare against it, and
each receiver's own post-wheel position must also have advanced, so three
receivers can no longer agree trivially at the origin. The composed ribbon
click now first clicks a different visible group through the same public link
map, requires the selection to be that other group, and then requires the
replacement click to replace it; the replay records both states, so the
artifact shows a click changing semantic selection rather than only agreeing
with a selection that was already current. Both strengthened assertions were
confirmed to fail when the behavior they constrain is neutered.

The reviewer also noted that PNG content is inspection-dependent rather than
machine-gated, and that the reported applied origin is derived from the
scrollbar fraction rather than observed from block geometry. BPR-0087 records
those as accepted, with the second belonging to RFC-BPR-0005 polish.

A fresh independent report is therefore no longer outstanding. The
owner-authorized native transition and predecessor status remain part of the
blocked BPR-0083 closeout.

## BPR-0084 build-foundation evidence

BPR-0084 raises every live build entry point to CMake 4.4.2, replaces the
private test option with `BUILD_TESTING`, and centralizes dependency ownership
before product subdirectories. Caller-installed Qt resolves through its CMake
config package and is never fetched. (Superseded by BPR-0337: Qt is now
acquired by a pinned provider and still resolved through the same config
package. The rest of this entry is unaffected.) Python and Git use their standard CMake
find modules, Linux libsecret uses `pkg-config`, and the centralized provider
requires the conventional imported targets before product targets are
declared. A collision with any provider-owned target name is fatal before
discovery, preventing an injected target from being mislabeled in the audit.
Python, Git, Threads, and `PkgConfig` use CMake's built-in module providers;
Config-mode preference cannot change that selection, and package redirects for
owned names are rejected without execution. BetterPullReview configures as the
top-level project, rejects `CMAKE_PROJECT_TOP_LEVEL_INCLUDES`, and loads its
repository, CTest, and built-in discovery modules from trusted absolute paths,
so dependency providers and caller module paths cannot replace audited logic.
Discovery follows graph selection: a Qt-free graph performs no Qt, libsecret,
or Git lookup, and a tests-disabled graph performs no Python or Git lookup. A
generated audit records each dependency, provider class, and imported target
selected by the product graph.

CPM.cmake 0.43.1 is reserved for source-managed dependencies. Its bootstrap
verifies the pinned loader SHA-256, disables local-package and package-lock
escape hatches, serializes cold initialization of the shared loader cache, and
uses one explicit `CPM_SOURCE_CACHE`. SQLite 3.53.4 is download-only CPM source;
its shared acquisition cache is serialized independently. The provider verifies
the official archive SHA3-256 plus `sqlite3.c`, `sqlite3.h`, `SQLITE_VERSION`,
and `SQLITE_SOURCE_ID`, stages only the two verified files into a controlled
build-tree directory, then builds the unchanged static
`BetterPullReview::SQLite3` compile/security contract without exposing other
cache contents to the compiler. Offline mode preselects an already verified CPM
cache directory so CPM cannot populate a missing source from the network;
installed packages remain available to local discovery. The tracked
amalgamation and its whitespace exception are removed, and no system SQLite
lookup exists.

The direct 20-case dependency-modernization contract and 104-case architecture
boundary contract pass. They cover the CMake baseline and presets, standard
options, centralized target ownership, package-only Qt, graph-conditional
discovery, provider auditing and target/config/redirect collision rejection,
dependency-provider and module-path interception refusal, CPM/SQLite pins,
cached offline reuse and uncached refusal, provider-path archive/source tamper
rejection, poisoned cache-sibling exclusion, source identity,
static-definition parity, and absence of vendored or system SQLite fallback.
The clean macOS `dev` matrix passed 74/74 tests, the hardened `core-only`
matrix passed 33/33, and the tests-disabled Release graph passed its self-test,
binary, and runtime audits. Ubuntu execution was explicitly deferred and is
not claimed; Windows coverage is structural only.

## BPR-0085 C++ coding-standard evidence

BPR-0085 renders the project-neutral standard with the stable
BetterPullReview/BPR/bpr identifiers and keeps the rendered policy under
`_coding-standard/`. Root `.clang-format`, `.clang-tidy`, and `.editorconfig`
files make that policy immediately available to editors and command-line
tools. The installed discovery scripts cover first-party source and tests
while excluding build, generated, and vendored trees. No unrendered package
token remains.

The complete 167-file C++ surface is canonically formatted. Public types and
namespace-level functions carry API briefs, project headers use deterministic
BPR include guards, and first-party identifiers follow the rendered naming
policy. Documented local analysis adaptations exclude Qt-generated names,
compiler/toolchain conflicts, and three intentional moved-from or nontrivial-
destructor test probes; the final warnings-as-errors audit reports no
first-party diagnostic.

CMake exposes `coding-format`, `coding-format-check`, and `coding-tidy` when
coding targets are enabled, reusing the centralized Python interpreter rather
than rediscovering it. Application and test targets now use their required
`app_` and `test_` categories while `OUTPUT_NAME` preserves every existing
binary name. Structural source scanners accept canonical whitespace but retain
their original semantic assertions. The behavior-preserving simplification
review found no further justified cleanup.

Final local evidence passed: the developer matrix built and passed 74/74
tests; the Qt-free core-only matrix built and passed 33/33; strict clang-tidy
completed with zero first-party diagnostics; `coding-format-check` checked all
167 files; the documentation validator reported `DOCUMENTATION_OK`; and
`git diff --check` was clean. The tests-disabled Release composition contract
also rebuilt and audited the stable `better_pull_review` output through the
renamed `app_better_pull_review` build target. Available-platform execution is
macOS; Windows and Linux credential implementations retain structural contract
coverage without a new target-native claim.

## Direct claim matrix

The current command groups are:

```sh
# C1: pure computation, canonical semantics, and bounded host
ctest --preset core-only --output-on-failure \
  -R '^(core_diff_(model|compute|semantics|transition)|application_diff_host)$'

# C2: selected measured/rendered Qt surfaces; always serial and headless
CTEST_PARALLEL_LEVEL=1 QT_QPA_PLATFORM=offscreen \
  ctest --preset dev --output-on-failure \
  -R '^(qt_diff_(axis|link_map|workbench)|qt_comparison_scroll_controller)_scale_(100|125|200)$'

# C3: selected production composition and shutdown; always serial/headless
CTEST_PARALLEL_LEVEL=1 QT_QPA_PLATFORM=offscreen \
  ctest --preset dev --output-on-failure \
  -R '^application_diff_workbench(_shutdown)?_public_path$'

# C4: structural and compiled worker/core boundaries
ctest --preset dev --output-on-failure \
  -R '^(core_dependency_boundary|core_boundary_contract|architecture_boundaries|architecture_boundary_contract|application_worker_executor)$'
```

C1 and C2 retain the focused semantic, host, and rendered-component evidence
described above. C3's final selected-composition case passes serially under
`QT_QPA_PLATFORM=offscreen`, writes
`build/dev/tests/end_to_end/diff-workbench-public-path.png`, and writes 18
rendered interaction frames plus `interaction-trace.jsonl` under
`build/dev/tests/end_to_end/bpr-0083-diff-interaction/`; the selected shutdown
case remains the H-03 lifecycle surface. C4 retains the hostile architecture
and compiled worker evidence. Exact final available-platform pass counts and
the remaining closeout gates are recorded on BPR-0083.

Every implementation row below, including I-03, is now directly covered by
those groups. A-03 cannot close before the tracker and RFC closeout are
complete. Target-native Windows and native System-theme/presentation-polish
claims are intentionally outside this RFC-BPR-0004 matrix, while BPR-0083
retains the explicit native-theme closeout gate described above.

A row remains open until its direct surface would fail or visibly disagree if
the claim were false.

| ID | Claim | Direct surface | Required proof |
|---|---|---|---|
| D-01 | Exact inputs/options yield deterministic canonical diff | Public/core file-diff computation used by host | Repeat and randomized replay compare segments, IDs, groups, pairs, diagnostics |
| D-02 | Segments cover both files monotonically | Alignment validator through public result | Positive fixtures plus generated invalid/valid property cases |
| D-03 | Insertions/deletions use empty spans | Public alignment result | Assert zero-length side and no synthetic line identity |
| D-04 | Secondary pairs are monotonic and deterministic | Public line-pair result | Repeated-line and tie fixtures with fixed expected pairs |
| D-05 | Weak lines remain unpaired | Mapping/detail projection | Fixture proves no false exact/similar counterpart |
| D-06 | Mapping reports exact, similar, span, boundary, move, unavailable truthfully | Public mapping API and detail pane | Known fixtures assert kind, target, and visible detail state |
| D-07 | Groups drive navigation and visual selection | Core commands plus composed UI | Previous/next IDs equal selected link-map/detail IDs |
| D-08 | Move detection does not break monotonic path | Public diff and rendered link map | Delete+insert path plus relation metadata; ordinary ribbons do not cross |
| H-01 | Cache identity includes exact blobs/options/algorithm | Host cache path | Controlled hit/miss matrix; path-only collision fixture |
| H-02 | Stale completion cannot replace active comparison | Real host with manual executor | Reorder completions across file/epoch/options/repository changes |
| H-03 | Cancellation/shutdown cannot touch destroyed UI | Host/composition lifecycle | Delayed completion after close is ignored safely |
| H-04 | Worker is Qt-free | Build/dependency/thread instrumentation | Core/worker build without Qt plus affinity assertion |
| Q-01 | One retained comparison position is vertical authority | Composed diff widget | Instrument `q` and pane origins during all input paths |
| Q-02 | Wheel over either pane follows the same path | Real viewports | Identical signed input sequence on left and right yields identical `q`/origins |
| Q-03 | Right insertion pins left | Composed rendered fixture | Frame/geometry trace shows constant left projected boundary while right advances |
| Q-04 | Left deletion pins right | Composed rendered fixture | Mirror of Q-03 |
| Q-05 | Unequal replacement moves proportionally | Measured controller and rendered fixture | Expected slope ratios and common endpoint |
| Q-06 | Reverse input retraces without cumulative drift | Shared controller | Forward then reverse sequence returns within stated tolerance repeatedly |
| Q-07 | Shared scrollbar represents comparison space | Public workbench | Drag, page, home, end all update `q`; no independent vertical authority |
| Q-08 | All content is reachable with no dead travel | Public workbench | Beginning/end and asymmetric-tail fixtures |
| Q-09 | Horizontal scroll remains pane-local | Public workbench | Horizontal action moves only intended pane and leaves `q` unchanged |
| Q-10 | Top panes contain no fake alignment rows | Rendered panes and source-line model | Row/line identity inspection on insertion fixture |
| Q-11 | Ribbons attach to true interval boundaries | Rendered link map | Geometry assertions and reference screenshots across scroll/resize |
| Q-12 | Empty spans render as wedges | Rendered link map | Pixel/path geometry on insertion/deletion fixtures |
| Q-13 | Hover/click resolves stable semantic ID | Link-map interaction | Hit test selects expected segment/group after scroll and resize |
| Q-14 | DPI/font/resize invalidation rebuilds measured geometry | Public widget | Change metrics, assert one rebuild and correctly attached paths |
| Q-15 | Routine scroll does not rebuild semantic diff or full measured axis | Instrumented public path | Counters remain unchanged during scroll; visible path count bounded |
| I-01 | Detail, link map, top selection, and navigation agree | Composed workbench | One interaction trace checks all stable IDs and visible state |
| I-02 | Epoch/file switches reject old geometry/results | Composed workbench | Delayed old result never flashes into new selection |
| I-03 | Real application demonstrates both scroll directions | Packaged app or closest deterministic composition | Recorded/manual evidence, not static mockup |
| P-01 | Large-file routine work scales with visible/changed set | Instrumented representative benchmark | Lookup/paint counts and latency trace with environment recorded |
| A-01 | Core remains Qt/provider free | Target graph/include scan | Direct failing dependency check |
| A-02 | Qt owns pixel geometry and current scroll state | State/ownership audit | No core/host canonical field for qreal origins or widget scroll values |
| A-03 | Documentation and live tracker agree | RFC/index/issues/docs validators | No unresolved documentation ledger row; all required cards done |

## Minimum rendered evidence sequence

The final interaction evidence must contain:

1. a matching region moving together;
2. a right-only insertion with the left side pinned;
3. continuation into the following matching region;
4. reversal through the same insertion;
5. input initiated over the other pane without a jump;
6. one unequal replacement;
7. selection of a ribbon and matching lower-detail update;
8. a resize or DPI/font metric change showing connectors remain attached.

The regenerated 2026-08-28 trace satisfies the sequence directly: items one
through eight are steps 02/03, 05/06, 07, 08, 09/10, 13/14, 15/16 with the
lower detail pane in agreement, and 17.

## Final available-platform closeout replay — 2026-08-28

The BPR-0083 available-platform replay ran at head `7160c283` on macOS 15.7.7
build 24G720 arm64 from the existing same-day clean trees. Every commit this
closeout adds after that head is documentation/tracker-only and cannot affect
a row. Code identity with the same-day RFC-BPR-0003 closeout matrices was
verified directly: `git diff --stat 27e50ded..HEAD` over `src`, `tests`,
`cmake`, `CMakeLists.txt`, `CMakePresets.json`, `third_party`, `packaging`,
`_coding-standard`, and `scripts` shows exactly one changed line — BPR-0473's
test-only edit in `tests/git_contract/git_process_contract_tests.cpp`, which
owns no RFC-BPR-0004 selector and whose own selector re-ran green after the
fix. The fresh clean-tree matrices recorded in the RFC-BPR-0003 final
closeout replay — `dev` 218/218 in 1756.78 s, `core-only` 98/98 in 282.04 s,
the WSL2 Linux replay at `17c8950d`, and the complete 246-TU partitioned
strict-tidy audit — are therefore current for every RFC-BPR-0004 claim
surface at this head and are consumed with that delta analysis. Every
RFC-BPR-0004 closeout row additionally received the fresh focused final-head
replay below, executed through three read-only `rfc-utility` lanes plus
direct integrator inspection.

Fresh focused runs at `7160c283`, all passing:

- C1 (`core-only` preset): `core_diff_model` 0.41 s, `core_diff_compute`
  0.11 s, `core_diff_semantics` 0.27 s, `core_diff_transition` 0.06 s,
  `application_diff_host` 0.08 s — 5/5.
- Dev-preset boundary/core group — `core_dependency_boundary` 0.20 s,
  `core_boundary_contract` 0.47 s, `architecture_boundaries` 7.18 s,
  `architecture_boundary_contract` 64.74 s, `documentation_structure`
  0.40 s, `core_diff_model` 0.35 s, `core_diff_compute` 0.10 s,
  `core_diff_semantics` 0.27 s, `core_diff_transition` 0.06 s,
  `core_workbench_interaction` 0.07 s, `application_diff_host` 0.08 s,
  `application_worker_executor` 0.08 s — 12/12, plus
  `documentation_issue_graph_contract` 0.36 s — 1/1.
- C2, serial and offscreen: all twelve `qt_diff_axis`, `qt_diff_link_map`,
  `qt_diff_workbench`, and `qt_comparison_scroll_controller` scale cases —
  12/12.
- C3, serial and offscreen: `application_diff_workbench_public_path`
  14.42 s and `application_diff_workbench_shutdown_public_path` 13.37 s —
  2/2. The post-run process scan found no surviving application process.
- Tests-disabled Release seam: `application_composition_contract` 120.06 s,
  rebuilding the `BUILD_TESTING=OFF` Release composition fresh and scanning
  it for forbidden bytes.
- ASan/UBSan selected matrix in the same-day fresh
  `build/rfc3-closeout-sanitize` tree (`ninja: no work to do` — all ten
  test binaries already current at this head): the six Qt-free selectors
  6/6 in 3.39 s and the twelve serial offscreen Qt scale selectors 12/12 in
  19.80 s. The sanitizer is active, not silently dropped: the diff-host
  test binary links `libclang_rt.asan_osx_dynamic.dylib` and exposes 341
  `__asan` symbols.
- Gates: `coding-format-check` passes over 425 files; `DOCUMENTATION_OK`;
  `BINARY_DEPENDENCIES_OK` and `RUNTIME_SURFACE_OK` against the dev
  executable; `git diff --check` clean over a clean worktree; tracked and
  on-disk GitHub Actions workflow counts 0/0; the original replacement-pack
  validator reports 22 validated files with the canonical
  `RFC-BPR-0004 -> rfcs/RFC-BPR-0004-diff-computation-line-mapping-grouping.md`
  replacement; `shasum -a 256 -c PACK_CONTENTS.sha256` verifies 22/22 pack
  files; the live RFC file's last commit remains import commit `190b301b`;
  and strict tidy re-verified the one TU changed since the full audit.

The regenerated composed evidence was inspected directly by the integrator.
The 19-record `interaction-trace.jsonl` and 19 full-window PNGs, all written
by this run, show: forward and reverse motion with clamped endpoints (`q`
0.0 leading-clamped to 1785.0 trailing-clamped, no dead travel); left-pane,
right-pane, and divider input all reaching `q` 835.2 with identical pane
origins; insertion pinning (left applied origin held at 754.0 while the
right advanced 754.0 to 799.0) and deletion pinning (the mirror, right held
at 559.0); the unequal replacement advancing the panes at different
measured rates; exact reversal (step 08 restores step 05's origins at both
panes); the ribbon click sequence advancing the link-map selection callback
from one to two while replacing a different selected group with the 4:7
group; exactly one geometry rebuild (generation 2 to 3) at the divider
resize with `semantic_index_builds` and `semantic_diff_generation` constant
across all nineteen records; and nonzero late applied origins on both
panes. Frames 06 and 16 were viewed directly: the insertion renders as an
attached wedge from the left pane's empty span into the right pane's
inserted block with the left pane pinned, and the selected 4:7 replacement
renders as a filled attached ribbon from left lines 30-33 to right lines
30-36 above the deletion wedge, with the lower detail pane and semantic
selection in agreement.

### Final replay record keyed by closeout criterion

Each BPR-0083 criterion below is keyed to the runs above; every selector its
row command names ran green at `7160c283` under the row's preset and
serial/offscreen conditions.

| Criterion | Final replay record |
|---|---|
| Identical inputs and options produce identical canonical segments, groups, mappings, and stable IDs. | Passed — `core_diff_model`/`core_diff_compute`/`core_diff_semantics` in the fresh core-only, dev, and sanitizer runs. |
| Segment coverage and monotonicity validators pass for representative and generated cases. | Passed — `core_diff_compute` in the fresh core-only, dev, and sanitizer runs. |
| Equal, replacement, left-only, and right-only regions are explicit. | Passed — `core_diff_model`/`core_diff_compute` fresh at head. |
| Empty spans represent insertions/deletions without fake source lines. | Passed — `core_diff_model`/`core_diff_semantics` fresh at head. |
| Secondary pairing is deterministic and monotonic. | Passed — `core_diff_semantics` fresh at head. |
| Weak/unpaired lines remain explicit rather than being falsely paired. | Passed — `core_diff_semantics` fresh at head. |
| Intra-line ranges attach only to accepted pairs and remain in valid text bounds. | Passed — `core_diff_semantics` fresh at head. |
| Move relations preserve a monotonic primary path. | Passed — `core_diff_semantics` fresh at head. |
| Previous/next change uses the same deterministic groups displayed in the link map. | Passed — `core_diff_transition` plus the three `qt_diff_link_map` scale cases fresh at head. |
| The detail pane reports exact, similar, span, boundary, and unavailable mapping honestly. | Passed — `core_diff_transition` plus the three `qt_diff_workbench` scale cases fresh at head. |
| Diff work uses exact blob/epoch/file/options/algorithm identity. | Passed — `core_diff_transition`/`application_diff_host` fresh at head. |
| A completion for an old repository, PR, epoch, file, options set, or generation is rejected. | Passed — `core_diff_transition`/`application_diff_host` fresh at head. |
| Cancellation and shutdown cannot update destroyed Qt state. | Passed — `application_diff_host`/`application_worker_executor`/`architecture_boundaries` plus the C3 shutdown case fresh at head. |
| Cache selection is directly proven rather than inferred from target names. | Passed — `application_diff_host` fresh at head. |
| Workers do not include or touch Qt. | Passed — `architecture_boundaries`/`application_worker_executor` fresh at head. |
| One retained comparison-space position drives both panes. | Passed — `core_workbench_interaction` plus the three `qt_comparison_scroll_controller` scale cases fresh at head; the trace's single `q` authority across every input receiver. |
| Wheel/trackpad input over either pane advances the same position with no handoff jump. | Passed — same selectors; trace steps 04/09/10 agree at `q` 835.2 with identical origins. |
| A pure right insertion pins the left pane at its boundary while the right content advances. | Passed — same selectors; trace steps 05 to 06 hold the left applied origin at 754.0. |
| A pure left deletion pins the right pane while the left content advances. | Passed — same selectors; trace steps 11 to 12 hold the right applied origin at 559.0. |
| Unequal replacements advance at proportional measured rates and meet at the same segment boundary. | Passed — same selectors; trace steps 13 to 14 advance the panes at different measured rates through the 4:7 region. |
| Reversing the same input path retraces the same pane origins within defined numeric tolerance and without cumulative drift. | Passed — same selectors; trace step 08 restores step 05's origins exactly. |
| Beginning and end content on both sides are reachable with no dead scrollbar region. | Passed — same selectors; trace steps 18/19 reach `q` 0.0 and 1785.0 with leading/trailing clamps on both panes. |
| Page, Home, End, change navigation, and shared-scrollbar drag use the same controller. | Passed — `core_workbench_interaction` plus the three `qt_comparison_scroll_controller` scale cases fresh at head. |
| Horizontal scroll remains independently usable in each pane. | Passed — the three `qt_diff_workbench` scale cases fresh at head. |
| Changed intervals render as filled cubic ribbons attached to actual pane line boundaries. | Passed — `core_diff_semantics` plus the three `qt_diff_link_map` scale cases fresh at head; frame 16 inspected. |
| Insertions and deletions render as wedges through empty spans. | Passed — `core_diff_semantics` plus the three `qt_diff_link_map` scale cases fresh at head; frames 06 and 16 inspected. |
| The top documents contain no synthetic alignment blanks. | Passed — the three `qt_diff_workbench` scale cases fresh at head. |
| Ribbons remain attached while scrolling, resizing, changing DPI, and changing font metrics. | Passed — all six `qt_diff_link_map`/`qt_diff_workbench` scale cases fresh at head; trace step 17's post-resize attachment. |
| Hover and click select stable semantic identities. | Passed — the three `qt_diff_link_map` scale cases fresh at head; trace steps 15/16's stable group identities. |
| Move relationships do not create a permanent crossing tangle. | Passed — `core_diff_semantics` plus the three `qt_diff_link_map` scale cases fresh at head. |
| Painting is bounded to visible/near-visible runs and does not instantiate one widget per ribbon. | Passed — the three `qt_diff_link_map` scale cases fresh at head; trace `visible_ribbon_count` stays viewport-bounded (0-3). |
| Splitter resizing and ribbon selection are both usable. | Passed — the three `qt_diff_workbench` scale cases fresh at head; trace steps 15-17. |
| The real BetterPullReview composition or closest deterministic rendered integration harness demonstrates the behavior in both scroll directions. | Passed — `application_diff_workbench_public_path` 14.42 s fresh at head through BPR-0227's accepted-review file-selection gesture; both-direction trace regenerated. |
| The lower detail pane, change navigation, selected line, and link map agree on identity. | Passed — `core_diff_transition`, the three `qt_diff_workbench` scale cases, and `application_diff_workbench_public_path` fresh at head; frame 16's four-surface agreement. |
| Large-file evidence proves no full-file semantic recomputation per wheel event. | Passed — `core_diff_compute`/`application_diff_host` plus the three `qt_diff_link_map` scale cases fresh at head; `semantic_index_builds` and `semantic_diff_generation` constant across all nineteen trace records. |
| High-DPI and supported platform rendering checks pass. | Open — target-native; owned by [[BPR-0474]] on the designated Windows x64 host. Not replayed here. |
| Product, architecture, UX, agent, and verification documentation is current. | Passed — `DOCUMENTATION_OK`, `documentation_structure`, and `documentation_issue_graph_contract` fresh at head. |
| A conservative simplification pass is complete. | Passed — `coding-format-check` over 425 files fresh at head; the complete same-day 246-TU partitioned strict-tidy audit consumed with the one-TU delta re-check; `git diff --check` clean. |
| All live RFC-BPR-0004 issue cards are in the terminal done lane before Auto-Verified closeout. | Blocked on [[BPR-0474]] — BPR-0054 is `Done` in `60-done/` and RFC-BPR-0003 is Auto-Verified; BPR-0474 remains in `21-ready-win/`, so this row completes at close. |

The owner-activated 2026-08-03 native System-theme evidence at `1e581a6`
remains anchored at its own commit per the BPR-0083 section above, with the
deterministic `application_native_appearance_probe_contract` green in the
same-day dev matrix. RFC-BPR-0004 therefore remains open solely on
[[BPR-0474]]'s Windows x64 high-DPI course; when that card reaches
`60-done/`, the closing slice ticks the checklist and flips the RFC status
with no further available-platform replay required unless a code-bearing
commit lands first.
