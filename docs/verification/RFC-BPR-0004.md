# RFC-BPR-0004 Verification Ledger

RFC base: `243295a70756b2e63a785ed11b14647b6b0a4c6d`

Replacement source: `betterpullreview-rfc-bpr-0004-diff-alignment-scroll-replacement-v1`

Status: implementation active. BPR-0072 supplies the directly tested canonical
record and structural-validation foundation. BPR-0073 now supplies the public
bounded deterministic text-computation path and directly closes its core-only
portions of D-01 through D-05 and D-08. BPR-0074 supplies the one public
mapping/group/navigation/selection/detail projection and closes the core-only
portions of D-05 through D-08 and I-01. BPR-0075 supplies the bounded
core/application request, materialization, cache, cancellation, shutdown, and
stale-acceptance seam and closes H-01, H-02, and the non-Qt portions of H-03
and H-04. Scale, Qt, installed-Git composition, and all rendered interaction
claims remain open on later cards. BPR-0076 now supplies the Qt-local live-pane
measurement, indexed max-extent axis, endpoint clamp, and explicit geometry-
invalidation seam. BPR-0077 now supplies the Qt-local retained comparison
position, receiver-neutral input translation, shared scrollbar projection,
guarded two-pane frames, and pane-local horizontal routing. Link-map painting
now comes from BPR-0078's custom-painted splitter handle, while selected
application composition remains open on BPR-0079.
RFC-BPR-0003 retains
explicit nonblocking closeout cards, so this RFC cannot become Auto-Verified
until its dependency can close.

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
delete-plus-insert segments with separate metadata only when the whole-segment
match is unique.

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

Whole-segment move metadata is the conservative BPR-0073 boundary. BPR-0080
explicitly retains moved sub-block decomposition and bounded/indexed
many-segment move work before RFC closeout; BPR-0073 does not claim those
later scale/completeness properties.

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

BPR-0080 explicitly retains indexing and bounded high-frequency semantic
lookup work. BPR-0074 does not claim Qt geometry, scrolling, ribbon painting,
or composed visual agreement; those direct surfaces remain on BPR-0076 through
BPR-0080.

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

## BPR-0084 build-foundation evidence

BPR-0084 raises every live build entry point to CMake 4.4.2, replaces the
private test option with `BUILD_TESTING`, and centralizes dependency ownership
before product subdirectories. Caller-installed Qt resolves through its CMake
config package and is never fetched. Python and Git use their standard CMake
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

Populate live commands, results, and evidence paths during implementation. A row remains open until its direct surface would fail or visibly disagree if the claim were false.

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
