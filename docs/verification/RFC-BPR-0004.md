# RFC-BPR-0004 Verification Ledger

RFC base: `243295a70756b2e63a785ed11b14647b6b0a4c6d`

Replacement source: `betterpullreview-rfc-bpr-0004-diff-alignment-scroll-replacement-v1`

Status: implementation active. BPR-0072 supplies the directly tested canonical
record and structural-validation foundation. BPR-0073 now supplies the public
bounded deterministic text-computation path and directly closes its core-only
portions of D-01 through D-05 and D-08. Mapping/detail, host, scale, and all
rendered interaction claims remain open on later cards. RFC-BPR-0003 retains
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
