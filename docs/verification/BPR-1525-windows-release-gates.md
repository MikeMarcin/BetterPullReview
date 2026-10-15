# BPR-1525: Windows normal release gate repairs

Mandate `authorized-release-continuation-native-gates-r2`; milestone
`bpr-1525-windows-release-gates/native-gate-contracts`.
Original release source: `8f0d25418cd4278065d5a043dabaa728feb202f1`.
Engineering owner `/root/bpr1525_implementer`, native `rfc-implementer`, configured
`gpt-6.1-sol` / `xhigh`. No differing runtime route was exposed.
The initial generic worker's WIP is `a8abc5451`; no historical native route
or acceptance is inferred from that work.

## Original failures and current disposition

The original Windows normal gate is still running. These are retained failures,
not a complete final inventory, and no corrected native pass is claimed yet.

| Contract | Original observation | Repair or remaining diagnosis |
|---|---|---|
| `github_recorded_fixture_contract` | Source Find/PR Search construction bytes and later commit differ | Explicit UTF-8/LF writes in both generators; portable full contract and CRLF negative control pass at `a8abc5451`; Windows pending |
| `test_network_isolation_manifest` | Nine Windows registrations are intentionally unmeasured | Actual contained measurements still pending; no pin is guessed |
| `test_network_isolation_child` | Grandchild `CreateProcess` returns WinError 5 | Selected interpreter is the per-user Python runtime, whose image has no package/container RX; configure probe and conditional named-container runtime grant prepared; native control pending |
| `application_diff_host` | Matcher stop 16.2308 ms exceeds unchanged 10 ms budget | Replace timer polling with synchronized stop notification in the application layer; native public-host and race checks pending |
| `review_cycle_git_readers` | Generic fixture Git command failure after ignore-case row | The literal-prefix pattern's oracle returns nonzero on Git 2.55; probe both known Git behaviors independently and retain capture's conservative exclusion |
| `git_metadata_harness` | 89 unclassified differences, zero checkout writes | 85 C4 findings come from the Windows oracle returning no file identity; four C5/capture findings are the exact literal-prefix compatibility difference below |
| `gh_cli_transport` | First strict acquisition assertion expects credential-unavailable | Test composition expected a successful POSIX private-account seam on Windows; preserve production refusal and add exact zero-effect Windows coverage |
| `github_provider` | 30.04 s timeout | Cause unproven; focused native diagnosis pending |
| `qt_diff_workbench_scale_100` | 120.01 s timeout at original row 418/705 | Cause unproven; only offscreen size-hint warnings observed; no UI source or timeout changed |

Root preserved and stopped the original failed partial gate (426/705,
11 failures/timeouts) and confirmed its process tree absent. Original external
log `C:/Users/mikem/AppData/Local/BetterPullReviewRelease/v021-windows-production.log`
and partial reports remain retained. Root authorized reuse of
`C:/Users/mikem/bpr-v021` on `codex/bpr1525-native-repair-r2` after preserving the
clean baseline as `bpr1525-preserved-original-8f0d2541`. The separate station
`C:/code/git/BetterPullReview` is outside this ownership.

## Git compatibility policy under existing RFC83 H1

RFC-BPR-0083 H1 permits a documented deliberate choice and keeps unknown
differences fatal. No owner waiver is added. Capture continues to exclude
`config/local/a/key.pem` under `config/local**/*.pem`, with either
`core.ignoreCase` value, as the previously qualified Git 2.50.1 behavior does.
Changing installed Git must not silently include material that this deterministic
capture policy previously withheld.

The actual local Apple Git 2.50.1 oracle ignores the path. The original Windows
Git 2.55 oracle keeps it. Upstream's
[2.50.1 `match_pathname`](https://github.com/git/git/blob/v2.50.1/dir.c)
removes the whole literal prefix before matching; the
[2.55.0 implementation](https://github.com/git/git/blob/v2.55.0/dir.c)
retains one prefix character so the matcher can distinguish a directory-component
boundary. The matcher source itself has the same relevant double-star rule.

The reader course independently requires either the exact ignored output with
exit zero or empty output with exit one; every other oracle result fails.
Its capture exclusion assertion remains intact. The harness's deliberate-choice
entry binds K, C5 or C5-capture, the two named ignore-case corpus cases, the exact
path, oracle `kept`, and candidate `ignored` or `excluded`. Unknown paths, cases,
components and reversed answers remain unclassified and fail. Native corrected
passes and a negative control are still pending.

## Selected interpreter containment

The original CTest manifest names
`C:/Users/mikem/AppData/Local/Python/pythoncore-3.14-64/python.exe`.
Read-only ACL inspection shows its image lacks the test-container/package RX
that the Program Files copy has. Checking the Program Files image alone did not
diagnose the selected interpreter. The uncontained launcher can create the first
process; that process's contained descendant must open the interpreter itself.

The prepared configure change probes an actual descendant using the canonical
selected interpreter, in its runtime directory. Only a failed probe grants RX
on that directory to the one named test-container SID, followed by the same
probe. An already accessible machine installation needs no ACL mutation.
The source/build/Qt grants, firewall requirement and zero network capabilities
remain. The existing child counterexample must still prove its grandchild's
AppContainer token and WSAEACCES 10013; a process-creation refusal is no pass.

## Verification and release boundary

The current source is still a repair candidate. Required focused native checks,
nine contained measurements, format/tidy and the provider-timeout diagnosis
remain open. Documentation verification and all 67 documentation verifier tests
pass with the installed
implement-rfc script directory on `PYTHONPATH`; the worktree has no managed
`.agents` link. Formatter 22.1.8 and `git diff --check` pass on the new C++ edits.

These repairs invalidate baseline `8f0d25418` as final release source.
Every release platform must rerun its required gate and packages from the same
corrected final source. This milestone grants no authentication, signing,
publication, native trust or release approval.


## Focused native evidence and further exact causes

Windows candidate `0d5f9cc2c90ecd99571a8dd32aba4347e585596a`, Interactive
Limited session 1, configures and builds successfully. The selected-runtime
probe fails before the named-container RX grant and succeeds after it.
`test_network_isolation_child` passes (0.19 s), as do external (0.15 s), loopback
(0.13 s) and concurrent launch (0.97 s). `application_diff_host` passes (6.06 s)
with its original internal 10 ms stop limit; readers pass (10.01 s), CLI passes
and provider passes (23.22 s) under its unchanged 30 s CTest limit. The original
provider timeout remains unexplained; final-source/full-gate repetition remains
required, and no cause or timeout repair is invented from this focused pass.

The native construction check exposed a second independent byte cause after
LF JSON writes: both JSON files had zero CRLF and equal lengths, but ambient
`core.autocrlf=true` changed the intentionally mixed-ending source blobs and
therefore their Git OIDs. Native regenerated head was `28a3fe88ff0de41b687d4fa750f9fdd247fee055`
instead of committed `755992e699b42939e3bff7abbdd1d66827ce439f`. The shared fixture
pack runner now disables that conversion per command; no global Git setting
changes. The new ambient-autocrlf regression rejects the former implementation
and accepts the corrected exact fixture. The complete portable fixture course
passes. Native corrected repetition is pending.

The harness drops from 89 to one unclassified difference on that native source.
The remaining C4 `format-compat-object-format` case sets repository format 1
and `extensions.compatObjectFormat=sha256`. Apple Git 2.50.1 prints the Git
directory; Windows Git 2.55.0.windows.3 exits 128 with exactly `fatal:
compatibility hash algorithm support requires Rust`. Upstream
[`repo_set_compat_hash_algo`](https://github.com/git/git/blob/v2.55.0/repository.c)
contains that compiled-without-Rust refusal. The existing deliberate discovery
policy opens directories without reading config; all config-reading candidate
components continue to refuse this extension. A new classification binds only
that exact case, component, candidate, observed refusal text and `answers`.
The oracle now retains that exact semantic refusal; every other error keeps its
previous value and cannot match the new entry. The source-backed rule extends
the existing C4 discovery distinction under H1, with no production change or
blanket suppression. Portable readers and harness pass after the change;
corrected Windows and negative controls remain pending.

BPR-1526's separately tracked [Mac action repair](BPR-1526-macos-workshop-action.md)
is part of the coordinator-authorized combined milestone. Its original press
and neighboring public courses now pass on the same application source.


## Nine actual contained registrations

Candidate `cf4f2452b986e2fb26bd481b5acf867d22b7aa67` rebuilt all affected
measurement targets in Interactive Limited session 1. Fixture and complete
metadata-harness normal courses both pass (104.27 s total). The nine original
uncontained commands were then each run through the zero-capability container
with their exact manifest arguments, working directory, environment and deadline:

| Row | Exit | Actual first contained refusal |
| --- | --- | --- |
| sqlite_remembered_connection_storage | 3 | fresh or upgraded profile without metadata authorizes no CLI attempt |
| review_cycle_git_reader_scale | 1 | fixture git command (after H6 measurements) |
| review_cycle_git_readers | 1 | fixture git command |
| git_metadata_harness | 1 | fixture git command (Git 2.55 version probe answered) |
| application_appearance_older_theme_contract | 1 | narrowing owner-private root ACL: Access is denied |
| application_cpp_identifier_appearance | 3 | render course owns its private data root (contrast probe passed first) |
| application_source_find | 3 | private data root |
| application_source_find_scale_1_25 | 3 | private data root |
| application_source_find_scale_2_0 | 3 | private data root |

All actual failures were retained at
`C:/Users/mikem/AppData/Local/BetterPullReviewRelease/bpr1525-contained-r4/`.
All nine normal uncontained positives pass on cf4f2452b (260.32 s total):
SQLite 3.02 s, reader scale 3.31 s, readers 9.19 s, harness 90.16 s,
older-theme 24.32 s, identifier appearance 18.89 s, and source Find
35.42/36.64/39.30 s. The corrected finite cause registry passes all 69 verifier
unit tests. No row, assertion, network capability or deadline changes.

The actual native scratch descriptor and shared storage inline code were then
probed contained and normally in Interactive Limited session 1. DOS and NT
names, std::filesystem canonicalization, the root's private descriptor,
WRITE_DAC and local filesystem admission all succeed for the C++ scratch root.
The storage ancestor walk instead fails opening `\\?\C:\` with
FILE_READ_ATTRIBUTES | READ_CONTROL, Win32 5. The remembered-connection row is
therefore `ancestor-control-denied`, not the historical DOS-name cause.

QTemporaryDir canonicalization succeeds, but opening its own root with
WRITE_DAC returns Win32 5. That root has a different descriptor from the C++
scratch root. The five appearance/Find courses are `owner-private-acl-denied`;
the older-theme script independently reports icacls Access is denied at its
narrowing step. No broad ancestor or machine ACE is added.

The exact isolated `workspace_fixture.hpp::git_runner` init command starts
Git 2.55.0.windows.3 and creates accessible `.git` and `refs`, but exits 1:
`cannot lock ref 'HEAD': unable to create directory for .../.git/HEAD`.
The three Git-fixture courses are `git-reference-parent-denied`. This names
Git's actual first refusal and makes no unobserved Win32-cause claim. The same
probe succeeds normally. Probe source and complete output are retained outside
Git in `bpr1525-native-focused-r9.log`; initial probes with Python's different
TemporaryDirectory descriptor are retained as rejected, nonrepresentative
experiments. The finite registry accepts only these three added cause names.

The selected Python runtime grant remains container-specific RX on
`C:/Users/mikem/AppData/Local/Python/pythoncore-3.14-64`; it grants no network
capability. The child check still requires actual nested process creation,
AppContainer token and WSAEACCES 10013. The separate contained
divider-only course passes in 20.302 s, with zero painted displacement and row
changes for all three shapes. Thus the original whole-workbench 120 s timeout
is beyond that first course; per-course native tracing is now required.


## Workbench and provider duration diagnosis

The original three whole workbench rows each exceeded their unchanged 120 s
CTest limit. A contained divider-only diagnostic passes in 20.302 s. The full
instrumented course reaches the keyboard course (55.268 s), the 100,000-line
gutter course (29.474 s) and later publication before timing out. A selected
keyboard-only diagnostic takes 55.807 s: key delivery 6.710 s and four ordinary
Qt event turns per key 45.515 s. Production instrumentation attributes 37.438 s
to paintEvent calls. These serial functional traces locate work; they are not
final duration acceptance.

The custom text painter exists to place a selected-text wash below syntax.
Unselected text now uses Qt's native painter, while selected text retains the
existing custom path, overlays and ordering. All three whole Mac scale courses
pass at 14.84/14.57/15.69 s. Strict clang-tidy 22 on the changed Qt TU passes.
The required frontend-design skill was read; this preserves the existing visual
contract and makes no redesign. Windows candidate 45520b2dc still times out at 120.02 s on scale100. Its owned
CTest tree was stopped after that repeated result; scale125 is interrupted and
scale200 has no acceptance. A complete functional trace is next; its extended
diagnostic process bound is not a gate pass or a changed CTest deadline.

The original provider timeout was 30.04 s under the normal jobs=4 full gate.
An isolated original focused pass at 23.22 s alone did not explain that failure.
Case tracing shows 20.714 s in the 18-case capability failure/retry matrix;
other cases are small. A representative jobs=4 subset with the original
neighboring HTTPS/paged/capability rows passes at similar cost (matrix 20.790 s),
so the original full-context timeout was not reproduced by that subset. The
matrix repeatedly parsed and split the same schema for every physical fake
response. It now prepares the same nine canonical parts once and returns a
fresh copy to each request. All six failure modes, three failure positions,
full nine-part retries and assertions remain. Mac provider passes at 8.96 s;
Windows candidate jobs=4 passes at 22.39 s. This modest reduction does not yet
close the original timeout; production append/compile/finish tracing is next.
Attributing the original extra delay to full-gate contention
remains an inference, and the final native full gate is still mandatory.

The complete serial functional workbench trace (r10) finishes every original
assertion in 151.87 s: keyboard 55.27 s, 100,000-line gutter 29.59 s, divider
19.20 s and line-details syntax 10.21 s. The ordinary native-painter attempt
changed no dominant duration and is removed. The shared whitespace painter
instead skips per-character classification/all-false vectors when a row has
neither ASCII space nor tab. Every row with possible marks still uses the core
policy, and the text-selection painter remains unchanged. Mac painter and
complete three-scale workbench courses pass (0.51/13.02/13.30/15.09 s).
Strict tidy 22 on the painter passes; native duration acceptance is pending.

Provider production tracing on 45520b2dc (r10) attributes 17.682 s of 20.62 s
total to 289 batch appends; compilation takes 0.005 s total. Each append parsed
the whole serialized prefix again before merging its next part. Request-local
Glaze assembly now retains that admitted tree, moves each validated next part
into it and serializes each prefix under the same output bound. Input-byte,
value, alias, order and sticky-failure checks and final complete normalization
remain. State allocation is inside the existing append exception boundary;
there is no shared attempt cache. Mac provider falls from 8.96 to 3.13 s and
the complete capability course passes in 4.58 s, including cumulative byte/value
refusals, missing/unexpected aliases, duplicate parts, GitHub/GHES fact equality
and malformed Unicode/envelope checks. Native concurrency acceptance is pending.

Windows c35c92e79 passes provider at 7.75 s in the normal jobs=4 subset,
capability checks at 12.03 s, manifest at 7.53 s and child at 0.19 s.
The shared painter passes at 0.24 s. Workbench still exceeds 120 s, and no
acceptance is inferred from the carried diagnostic markers in that test object.
MSBuild had reused an object because Copy-Item preserved the restored source's
older timestamp. Subsequent restores touch source files and verify compilation;
r13 recompiles both the production painter and test before restoring its binary.

The keyboard component profile on c35c92e79 takes 52.38 s: text 25.49 s,
gutter 6.53 s and whitespace 0.81 s. Several setters unconditionally invalidate
whole viewports and gutters even when boundary/color/anchor values do not
change. They now invalidate on change; selection-dependent whitespace
invalidates the visible old/new selected-row difference. Unified gutter
selection explicitly invalidates when its own state changes, so it no longer
relies on an unrelated repeated color assignment. Text band invalidation
continues through Qt, whose [extra-selection implementation](https://raw.githubusercontent.com/qt/qtbase/v6.11.2/src/widgets/widgets/qwidgettextcontrol.cpp)
tracks changed selections. All complete Mac workbench scales pass after this
change (14.46/13.85/15.43 s); Windows acceptance remains pending.

Native Git classification counterexamples (r12, Git 2.55.0.windows.3) change
only the registry's observed `kept` answer and exact Rust-refusal text in a
disposable source edit. `ignorecase-true` fails with two unclassified findings;
`format-compat-object-format` fails with one. Both exit 1. Restoring the exact
source, touching it, recompiling the harness and repeating both cases yields
zero unclassified findings and pass. The core capture assertion stays intact.
The previous full corrected Windows harness and the full Apple Git 2.50.1
harness pass; no unknown answer or broad pattern is suppressed.

The direct contained nested-process proof prints `GRANDCHILD_TOKEN=True:appcontainer`,
`NETWORK_ISOLATION_EXTERNAL_DENIED:10013` and
`REAL_GRANDCHILD_TOKEN_AND_WSAEACCES_PROVED`. Runtime ACL inspection shows only
the named `BprAutomatedTest` SID added as `(OI)(CI)(RX)` to the canonical selected
runtime, inherited as `(RX)` by its interpreter. No all-package ACE was added.
The complete proof and negative/positive controls are retained in
`bpr1525-native-controls-r12.log`; all disposable source changes are restored.

A disposable Windows experiment with Qt's existing layout cache reduces the
same keyboard course from 52.38 to 37.47 s and text painting from 25.49 to
12.18 s. The production implementation retains only visible block caches and
disables them when they leave the viewport. It uses persistent QTextCursor
handles: the first experimental bare-QTextBlock list intermittently crashed
after document clearing, and a signal backtrace plus offline symbolization
localized the stale dereference to eviction. That experiment is rejected.
QObject lifetime is tracked, and persistent cursors follow edits within a
surviving document. No foreground, band, selection or glyph drawing changes.
The actual-layout regression checks nonempty cache use, a one-viewport bound
at both scroll ends, return to the start and document clearing. Complete Mac
scales pass at 10.65/10.67/12.25 s; three focused repeats of the previously
intermittent scale pass. Windows acceptance is still pending. Qt's
[layout-cache API](https://doc.qt.io/qt-6/qtextlayout.html#setCacheEnabled)
provides the cache; no custom shaping engine is introduced.

Candidate 6fc970555 still exceeds Windows scale100's 120 s limit; its remaining
owned copies are interrupted and no whole-course pass is claimed. The detached
100,000-line phase diagnostic attributes 0.233 s to text construction, 2.099 s
to diff computation and 23.572 s to composition (preparation, filling, adoption
and geometry combined). Detached staging now defers Qt layout until adoption
has installed the final plain layout, then re-enables it. The existing 256-line
batch and 4 ms yielding rules stay intact. The cache regression also requires
that its native viewport actually leaves the initial rows. Complete Mac scales
pass after these additions; Windows duration acceptance remains pending.

Production subphase profiling (r19) attributes 18.943 s to geometry measurement,
3.879 s to detached filling, 0.280 s to document adoption and 0.435 s to the
publication adoption path. A font-metrics width prototype is rejected: its
bounding width differs from Qt's native line width. The corrected optimization
keeps Qt's full shaping and exact line dimensions. Entirely plain printable
ASCII rows with default block/character formatting reuse one unbound
QTextLayout with the exact document font and paint device; each new string is
still laid out by Qt. All other rows retain the original document-bound
transient engine. This keeps one temporary HarfBuzz buffer per measurer instead
of constructing one for each short row, consistent with BPR-1032's bounded
measurement rationale. A redundant empty setFormats is also omitted; Qt's
[implementation](https://raw.githubusercontent.com/qt/qtbase/v6.11.2/src/gui/text/qtextlayout.cpp)
marks its bound document changed even for that write. No native metric is
approximated or sampled. All three complete Mac workbench scales and all four
native-geometry comparison scales pass. Corrected Windows acceptance is pending.

The prepared Ubuntu producer has now been inspected read-only while Windows
was quiescent: distro `Ubuntu`, Ubuntu 26.04 LTS, clean detached baseline
8f0d25418, Debug `build/dev`, `/usr/bin/c++`, pinned Qt 6.11.2 and its existing
CPM cache. No Ubuntu build/test, package or authentication operation has run.

### Current whole workbench trace and bounded painting correction

At c4ec0f7ab, r22's diagnostic-only whole run completes every unchanged case
in 124.04 s; the normal 120 s gate remains failed. Keyboard takes 37.29 s,
100,000-line gutter 27.99 s, divider 15.83 s and syntax line details 9.11 s.
Profiles record text draw 18.26 s, gutter paint 13.97 s, whitespace walk 8.00 s
and geometry 28.48 s. No independent Windows or WSL workload overlaps this
measurement; three diagnostic sources are restored, touched and recompiled.

The next candidate removes repeated walks from the first visible block when
painting whitespace. The core exposes its existing line-admission rule so an
unselected line in selection mode prepares no character vectors or cursor
geometry; all six modes, both selection states and invalid mode agree with the
existing actual-mark oracle. Geometry keys visit only present Qt format
properties instead of querying every possible font property per syntax range.
No input, semantic assertion, batch limit or deadline changes.

Self-review corrected a separate cache-lifetime issue: Qt 6.11.2's
`QTextLayout::setCacheEnabled(false)` only changes a flag. Its noncached draw
completion frees glyph layout data. Retired viewport caches now complete a
fully clipped public draw above every line, preserving all line geometry and
painting no pixels; membership uses a viewport-sized set. Persistent cursors
still track document replacement safely. The existing flag-count regression
alone is not evidence of allocation release. The source-backed mechanism is
[Qt's pinned implementation](https://github.com/qt/qtbase/blob/v6.11.2/src/gui/text/qtextlayout.cpp).
The actual large-review resource course also passes on Mac, alongside all
three complete workbench scales, all four native geometry scales and core
appearance policy (9/9). That Mac functional run overlaps strict tidy, so its
durations are not performance evidence. Windows normal performance acceptance
and Ubuntu affected-path verification remain pending.

### Whitespace endpoint projection is the remaining long-line cost

Normal r23 at 5e5f14790 still times out all three workbench scales at 120 s.
The isolated original line-details syntax course under r24 finishes in 9.39 s:
48 whitespace calls consume 6.91 s, whereas 26,616 prefix conversions consume
only 50 ms. The proposed ASCII prefix conversion shortcut was therefore
rejected before commit. A temporary per-draw cache guard was also removed:
Qt's cursor queries already retain their data between queries, so that was
not a demonstrated repair mechanism.

The actual waste is querying endpoints for every whitespace mark far beyond
the horizontal viewport. The revised shared painter asks Qt's two viewport
edge hit tests to bound projection in an unwrapped LTR ASCII row, retaining
one adjacent character at each edge for partially visible spaces/tabs.
The core still classifies the full row; every projected endpoint still comes
from the same native cursor query. Wrapped, RTL, Unicode and control-bearing
text retain the full existing path. No text, mark or format assertion changes.
A new oracle compares visible pixels with projecting all native endpoints at
both ends and the middle of a long tabbed row, and proves Unicode uses the
full fallback. Mac shared-painter and all three complete workbench scales pass
(4/4); native Windows acceptance remains pending.

The required full core-only preset built and ran 276 courses: 272 pass, including
both architecture checks. Four setup failures are retained. Three lacked the
ignored declared MASK deployment; MASK register restored locked links and
local generated roles without tracked declarations or upgrades. One nested
configure tried downloading pinned LibArchive from an unwarmed local cache
inside network containment. Its normal rerun uses the same prepared pinned
cache already used by dev; containment and source assertions stay unchanged.

### Reusing exact plain-row geometry between the two panes

At d897dc239, normal Windows scale100 passes in116.26 s, while125 and200
still time out at120.02 s. The new shared-painter pixel oracle and four native
axis scales all pass. These partial results do not close the workbench gate.

The next candidate lets the right measurement borrow the left pane's existing
16-byte records for same-index, exactly equal plain ASCII rows. It requires
unchanged live peer cache facts, equal full pane/document fonts, margin, tab
stop, DPR, layout direction and the identical native paint-device context.
Both rows must have empty block/fragment/additional formats and the same strict
unwrapped plain options; every mismatch shapes through Qt as before. No text
hash, sampled metric, alignment guess or new per-file store is introduced.
The record stores must outlive the measurement; the workbench's pending
measurement is destroyed before those member stores.

New native-oracle courses prove all40 eligible rows reuse, a changed row alone
falls back, and different fonts/devices or a changed peer revision reuse none.
Every boundary and the horizontal widest-line result are independently read
back from Qt. New tests also exposed Qt retaining its previous font's larger
maximum: the measurer now reconstructs the maximum from current extents, so
that stale maximum cannot suppress visiting the newly measured widest block.
All seven Mac workbench/axis scales pass. Removing the font guards causes a
native-boundary failure; removing text equality causes the widest-line oracle
to change from1958 to13476.8. Both negative controls exit8, and restored,
touched, recompiled source passes all four axis scales. Native Windows
performance and Ubuntu focused verification remain pending.

The four core-only setup failures now all pass after declared MASK deployment
and selecting dev's already prepared pinned cache. Together with the272 initial
passes this covers all276 courses; two dependency-script methods explicitly
skip because the core-only graph does not build KSyntaxHighlighting. Strict
clang-tidy22 passes all six newly changed TUs at d897dc239. The later peer
measurement production/test changes still require their own strict tidy pass.

### Remaining 200% redraw work

At4af0e4e2b, Windows r26 passes the native pixel oracle and4 axis scales, then
normal workbench100/125 at108.36/114.79s. Normal200 still times out120.01s.
A same-source, otherwise idle PC trace completes every200% assertion in127.56s;
keyboard is44.36s, divider22.25s and gutter20.75s. This extended diagnostic
is not normal acceptance. Its disposable cleanup wrapper's single-pair array
flattened in PowerShell; the separate explicit restore verifies the original
bytes, removes only its hash-matched untracked `e`, restores/touches the exact
absolute test path and recompiles the normal binary, exiting0 with clean source.

The next candidate accumulates native block heights once for selected-row
regions, eliminating repeated walks from the first visible block. A unified
selection and native text damage invalidate their mapped gutter rows, including
both complete number columns even in a narrow viewport. The fixed heading stays
fixed; scrolling, origin placement, theme and content changes retain their
full invalidations. No geometry, colour, selection or test assertion changes.
All three complete Mac scales pass10.46/10.71/12.41s and strict tidy22 passes.
The broader current Mac integration passes13/13 (workbench/axis, resources,
workshop press+3 guidance siblings and resealed review round trip).

Strict tidy also corrected the new helper's const local return to allow a
QString move, and mechanically renamed inherited `stale_after` in the axis
TU. No naming suppression or gate relaxation. Exact-source Windows200 and
then100/125, plus the Ubuntu affected-path pass, remain open.
