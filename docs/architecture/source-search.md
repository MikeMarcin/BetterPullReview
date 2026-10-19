# Source search ownership and session contract

[RFC-BPR-0077](../rfcs/RFC-BPR-0077-find-and-find-in-files.md) owns the complete
Find and Find in Files behavior. Its first milestone, [[BPR-1419]], supplies
the deterministic session and Qt-free effect lifecycle. M2 supplies the shared
matcher and M3 the exact epoch corpus. M4 ([[BPR-1422]]) composes native pane
Find in the shipped source reader. M5 ([[BPR-1423]]) composes PR Search, exact
result activation and the large public performance course.

## Matcher ownership

[[BPR-1618]] moves BPR-1431's unchanged pinned RE2 wrapper into core's private
`source_regex_engine.cpp`. `default_source_regex_engine()` supplies it to the
same injectable `source_regex_engine` interface used by pane and PR execution.
Both Qt-free core and the application use that implementation. RE2 and Abseil
types never cross the core API; the regex adapter and its target are removed.

The owner approved this scoped dependency on 2026-10-18 after auditing worker
creation and externally visible determinism. Internal locks, one-time state,
hash seeds and contention clocks may change cache layout/timing; they must not
change syntax, match spans/order, error codes or offsets. Fixed UTF-8 options,
quiet errors, no captures, the existing memory bound and DFA fallback stay as
before. See the [audit](../issues/_checkpoints/BPR-1618/re2-audit.md). Host-owned
cancellation and source reads retain their existing effect boundary.

## Core policy

`core::source_search_state` retains an exact scope, query owner, monotonic
generation, match options, input/status facts, bounded coverage counts and a
cursor. It has no dynamic storage, query text, source bytes or match collection.
It is trivially copyable, checked at compile time against a 2 KiB ceiling, and
measures 1,592 bytes on the M2 Apple Silicon build (1,528 before fixed
selection bounds were added). That is a state measurement,
not an aggregate search-memory measurement.

The scope binds application session, complete PR authority, exact available
comparison anchors/commits, active tab, corpus generation and target. A pane
also binds file, selected comparison digest, available side blobs and optional
fixed original byte bounds on one actual side. Those bounds participate in
request equality and never move during match navigation. A missing
side cannot become a neighbouring source location. `CatalogueGeneration` names
the host's catalogue for this exact pair; it does not name the latest remote
epoch. Re-observing an unchanged scope does no work, so an older pinned tab
does not follow an unrelated newer epoch.

Every query edit or scope change clears obsolete counts before requesting new
work. Empty, oversized or invalid UTF-8 input emits no effect. Query ownership
survives a temporarily unavailable corpus: supplying another application
session or account authority still clears the retained query and options.
Reset explicitly revokes all authority and input. Close clears hits and cancels
unfinished work while retaining query/options for reopening.

Completions must equal the entire current request, including options and query
generation. Stale and duplicate terminals change nothing. Generation exhaustion
fails closed rather than reusing an earlier identity. Completion facts cannot
claim more than 10,000 matches, a current index outside the accepted prefix,
hits without a searched file, or more skipped sides than the eligible corpus
contains. `AdditionalMatchObserved` requires an actually observed extra match
at the 10,000-record cap, with a result-limit terminal.

Only a complete empty search whose eligible files were all searched, with no
skipped sides, can project unqualified No results. Incomplete coverage converts
an otherwise complete terminal to partial. Cancelled, capped, unavailable,
failed and invalid-pattern terminals retain their distinct status. Usable
partial counts support constant-work next/previous traversal and explicit wrap;
navigation emits no matching or source-read request.

## Host lifecycle

`application::source_search_host` owns one independent pane or PR session.
Its public entrypoints run on the required serialized decision lane. The host
admits query input before copying, at 4 KiB, and passes an immutable work packet
to `source_search_execution_port` on a separate effect executor. The execution
port returns a typed terminal and an optional immutable exact record collection.
The pane matcher supplies records through `execute_source_pane_match`; the PR
corpus supplies streamed records/batches under the same identity. Payload size,
preview and location admission runs on the effect lane. Only the serialized
exact-current completion publishes its collection, and query/scope invalidation
clears it synchronously. Constant-work traversal shares that immutable collection.

At most one effect is executing or awaiting decision delivery, with one latest
replacement packet. Supersession requests stop and replaces that queued packet;
each replaced admitted packet gets a cancelled terminal. A stopped effect still
owes its one terminal. A refused effect submission or throwing worker ends in a
typed failure. An answer for another request fails the originating job rather
than leaving the current session stuck searching. Stale terminals publish no
projection. Counters record only structural lifecycle facts, never query text.

Projection feedback is deferred through at most 64 ingress operations. Each
can hold at most one 4 KiB query and a fixed scope; work packets and retained
query text are separately bounded as above. Projection exceptions cannot veto
effects, cancellation, acceptance or quiescence. Shutdown immediately closes
ingress and requests stop, retires queued work and remains nonquiescent until
the active effect's decision terminal is consumed. The execution/projection
ports and both executors must stay alive through `is_quiescent()`.

No persistence, Find pasteboard, logging of content or review mutation capability
is introduced. The M2 limits below bound one matcher/entry execution. M3 adds
epoch-owned acquisition and aggregate admission as described below. The complete RFC criteria remain
open until the composed application and its performance fixture prove them.

## Verification surface

`core_source_search_policy` exercises exact scope/query admission, reversed and
foreign completions, duplication, authority gaps, exhaustion, coverage and wrap
without `QApplication`. `application_source_search_host` exercises the public
host through separate manual effect/decision executors: independent queries,
rapid coalescing, invalid input, cancellation, reentrant feedback/shutdown,
projection/worker/submission failures and observable quiescence. Its 1,000-step
traversal leaves the execution count unchanged. These checks prove session/record acceptance. The M2 checks below prove
matching; M3 checks source membership below. The M4 course below exercises
native pane controls through the installed application composition.

## Shared matching and source plans

`compile_source_match` compiles once and returns an immutable host-owned program.
Matching runs on [Google RE2](../../third_party/re2/PROVENANCE.md) ([[BPR-1431]]),
which replaced a hand-written engine after the 2026-10-13 review found it
silently missed Whole Word regex matches. Core owns the rules and the private
RE2 implementation behind `source_regex_engine`; composition supplies the core
factory to the host ([[BPR-1618]]). RE2 runs in linear
time with no backtracking; literal queries use its literal mode. Case-insensitive
matching uses RE2's Unicode simple case folding, with no locale, accent removal,
normalization or multi-scalar fold. Whole Word treats Unicode letters, marks,
numbers and underscore (`[\p{L}\p{M}\p{N}_]`) as word scalars.

Regular expressions use [RE2 syntax](https://github.com/google/re2/wiki/Syntax):
groups, named groups, alternation, classes (including POSIX classes and Unicode
categories and scripts), `* + ? {n} {n,m} {n,}` with greedy or lazy preference,
`i/m/s/U` flags (global or scoped), `^ $ \A \z \b \B` (with ASCII `\b`, as in
RE2), Perl classes and escapes, and `\Q...\E`. Line-anchor mode defaults on and
dot-all off; flags can override both. Lookaround and backreferences return
unsupported syntax, as does `\C`, which would match one byte and could split a
UTF-8 scalar; class and `\Q...\E` contents are not mistaken for them. Malformed
patterns return invalid syntax; diagnostics retain only code and offset, the
offset being where RE2 stopped. Groups do not publish captures.

Whole Word checks each match's actual neighbours. A refused match resumes the
search at the next start whose previous scalar is not a word scalar, so a refused
earlier start never hides a later whole word. A refused end does not try a
shorter end at the same start.

The scanner builds one logical LF view, reading CRLF as LF and keeping a lone CR
as content ([[BPR-1432]]), while retaining original byte boundaries. Results are nonoverlapping exact original UTF-8 spans;
a zero-width result advances one scalar, with the terminal position emitted once.
Whole Word and anchors inspect actual document context, including outside fixed
selection bounds. Columns are zero-based scalar offsets. Sparse column checkpoints
bound conversion on long lines; previews contain at most 240 complete scalars.

Unified matching borrows the existing validated `diff_semantic_index`. It uses
the same equal/group row order as the native unified reader, and scans actual
versions without concatenating removed/added text. Right text owns equal rows.
An equal row whose ignored whitespace differs excludes that hidden left text
from the left version's contiguous scan ranges. Exact unchanged occurrences
merge only after LF-view span correspondence is proved. A captured unified
selection that crosses visible versions is unavailable. PR matching uses no
semantic diff and merges only identical blob IDs and bytes in the same entry;
different blobs remain separate even when previews agree.

`compile_source_paths` parses include/exclude input before acquisition: commas
inside nested braces/classes and escaped commas remain intact, `*` and `?`
stay within a path component, `**` crosses directories and `**/` accepts zero
directories. Slashless patterns match basenames at any depth; `./` anchors root.
Matching is case-sensitive and either included path admits an entry, unless
any side is excluded. Empty filters admit all valid paths. Corpus callers use
`source_path_filter::evaluate` so an input/resource refusal remains coverage,
rather than treating it as an ordinary nonmatch.

## Matcher resource contract

Callers may tighten these hard defaults; enlargement is refused:

| Resource | Ceiling |
|---|---|
| Query / combined path-filter input | 4 KiB UTF-8 |
| Source side | 16 MiB / 200,000 lines |
| Engine memory per compiled query (program and DFA caches) | 8 MiB, plus 1 MiB per Whole Word class |
| Counted repetition | 1,000, including nested repetition (RE2's limit) |
| Query admission / entry composition work | 1,048,576 / 134,217,728 counted steps |
| Stop polling | Initially, before each engine search, then within 1,024 counted steps |
| Matcher/entry owned storage | 64 MiB conservative capacity reservation |
| Result records plus previews | 16 MiB / at most 10,000 records |
| Preview | 240 scalars / at most 960 UTF-8 bytes |

Engine memory, logical text, indexes, source plans and result capacity are
admitted before allocation. One engine search cannot be interrupted, but it is
linear in a side that is itself bounded at 16 MiB. Resource exhaustion returns a qualified terminal
and any already assembled usable prefix. `AdditionalMatchObserved` is set only
after observing another occurrence beyond the record cap. Borrowed immutable
source/diff material remains under its existing owner/cache limits. The pane
bridge reserves its transfer vector separately inside the same ceiling. These
are capacity/work bounds, not RSS or universal latency measurements; M3 supplies the full PR admission envelope below. RSS and warm/cold timing are measured by the public application course below.

`core_source_matcher` exercises Unicode/LF spans, unsupported syntax, glob policy,
real unified plans, selection, deduplication, Whole Word regex regressions and
resource/cancellation limits on the product's RE2 engine.
`core_source_matcher_reference` compares a portable subset with a bounded Python
oracle. The public host
fixture also proves exact record acceptance, invalid-pattern clearing, wrong-blob
refusal and 1,000 traversals without a matcher call. No QApplication is involved.

## Exact PR corpus and acquisition

M3 ([[BPR-1421]]) adds `epoch_application_host::bind_source_search` beside the
selected-file grant. It binds one exact available retained pair and active tab
under the live workbench authority, with a monotonic catalogue generation. It
neither selects a comparison nor replaces a selected file grant. Catalogue
construction verifies both exact durable anchors and asks the already held Git
session for that pair's complete raw tree changes. No accepted provider file
list, Files view, receipt or opened-file state is used.

Installed Git runs bounded `diff-tree -r -z --raw --no-abbrev` over the two
immutable commits, with external diff/text conversion disabled. Exact 100%
rename/copy records carry both original paths and identical object proof;
changes without that association stay as separate old/new entries. The closed
Git process port sends domain records through a bounded sink. A malformed,
missing, unrepresentable or oversized catalogue refuses atomically. Missing
historical trees never become an empty list or today's intersection. Modes
remain explicit: symlinks and gitlinks are skipped without opening their text
as local paths. The host sorts by the exact right path, or left path when absent.

Each catalogue has at most 10,000 changed entries and 8 MiB of owned metadata.
The worker compiles the query and path filter before material acquisition,
evaluates every exact pair of paths, then streams one entry's full documents.
Invalid input starts no source work. Both valid sides use the shared core matcher;
identical blobs within an entry share a read and preserve both locations.
Different blobs and different entries retain their separate occurrences. No
syntax classification, semantic diff, checkout, search executable or persistent
content index is created.

Exact reads enter the same epoch actor, call gate and job ledger as selected
source loading. A missing sparse blob can cross the existing transport only
with the exact catalogue and durable sparse-anchor proof; received bytes are
checked against their blob ID. One background read is retained per search actor,
with immutable blob and catalogue identity, independent of query generation.
Latest demand is installed before stopping the old matcher, so a replacement
which still needs that blob joins its in-flight read. Clear, close, changed
filters, scope retirement and shutdown stop material that is no longer needed.
Every admitted actor job retires even when its caller stops waiting.

Catalogue work, local reads, network reads and matcher slices yield to selected
navigation. `yield_to_source_navigation` is also the public composition seam
for pane Find; the pane controller invokes it when admitting that work.
Search uses the existing 256-entry / 16 MiB actor material cache. Background
retention can replace only other background entries; it cannot displace a blob
retained for the reviewer. A selected read promotes reused background material.
The diff cache and its selected/prepared capacities are unchanged.

## PR progress, limits and retry

The worker emits ordered batches of at most 128 records, including empty progress
updates for coverage. The host owns one pending progress mailbox, coalesces
projection posts to at most ten per second, and accepts only exact current
monotonic prefixes through core policy. Terminal updates are immediate. A late
progress closure cannot restore obsolete results and remains part of host
quiescence. Source cursor traversal uses the same immutable records and performs
no compilation, scan or acquisition. A cursor chosen during progress survives
the terminal.

The 192 MiB conservative per-search admission envelope is checked before record
allocation and entry reads. It covers a 64 MiB matcher reservation, two 16 MiB
source sides, the existing 16 MiB actor cache, catalogue/conversion reservations,
and bounded result/mailbox/projection copies. Results and previews stay within
16 MiB per collection, with at most 10,000 records. This is a capacity bound,
not measured process RSS or a universal latency threshold.

Every skipped present side retains file, side and typed reason: binary,
undecodable, unsupported mode, file limit, missing material, transport failure
or cancellation. Absent added/removed sides are not counted as skipped. Eligible,
searched and skipped counts qualify partial hits; only complete empty coverage
is No results. At the record cap, the worker stops with a defined ordered prefix.
An observed further hit permits `AdditionalMatchObserved`; exactly 10,000 hits
alone do not. Resource exhaustion and cancellation keep their distinct terminal.
Retry issues a new query generation under the same bound scope, preserving the
original comparison/authority and retrying unavailable material.

`application_source_pr_search` exercises both-side full documents, path filters,
exact deduplication, skipped reasons, partial-prefix retry, batches, capped counts,
reordered progress and no-read traversal. `application_epoch_host` composes the
public search host/worker with the real epoch host and fake narrow capabilities:
historical-only membership, retained read reuse, in-flight replacement, sparse
blob validation, priority cancellation and authority shutdown need no QApplication.
`git_process_contract` and `git_cache_contract` exercise the installed Git seam.
The public Qt courses below exercise these foundations through the native drawer,
ordinary active-tab result activation and full application performance course.

## Native pane composition

`app::pane_find_controller` owns the independent pane host on the existing
serialized reviewer lane and diff effect executor. It captures only the accepted
immutable diff, existing semantic index, original reading origin and complete
scope. Work calls `execute_source_pane_match` on the effect lane. No Qt widget
reads a blob, runs a process, computes a diff or implements matching policy.
Admission yields to the epoch host's existing selected-source priority seam.
Tab/document invalidation retires old hits immediately; a visible bar retains
its query/options and rebinds after accepted source geometry is ready. PR/account
authority invalidation resets the query and options. Shutdown waits for the
pane host's exact terminal before destroying ports or the window.

Core projects Find/Next/Previous legality from bounded reader, material, editor,
modal, query and hit observations. Qt supplies those observations, one set of
View > Find actions and their physical shortcuts. The native bar owns focus,
150 ms typing debounce, IME commit handling and literal source seeding. Obsolete
decorations clear before debounce; oversized native input is refused before
copying it into an effect packet. Escape restores the target source. Captured
original selection bounds never move with match navigation.

Qt caches original UTF-8 endpoint conversions and actual-version visual
fragments when accepting a collection. The mapping walks validated source bytes
without allocating prefix strings, preserving scalar/surrogate and CRLF spans.
Unified multiline occurrences split around opposing-version visual rows. Their
wash/outline covers only actual source fragments; explicit Copy of the current
occurrence yields its original source bytes. An EOF hit with no actual line
never invents a comment anchor. Semantic selection and visibility use the
existing shared controller, with deferred reveals bound to the still-current
match and cancelled by later reading gestures.

The native model indexes decorations by document position with prefix ends;
painting visits visible candidates. Scroll-track painting uses sorted fractions
and at most one binary lookup per track pixel. No occurrence widget or second
link map is introduced. Nonoverlapping accepted spans, the 200,000-line source
ceiling and 10,000-record ceiling bound endpoint/fragment caches. Complete
results can retain one exact-scope/query/options cache entry, sharing the
existing <=16 MiB immutable records. Closing clears active decorations and work;
Next/Previous reopening can reuse that collection and cached endpoints. Query,
document, tab, comparison and authority identity still govern acceptance. These
are structural capacity facts; whole-application RSS is measured separately.

`application_source_find` uses a committed synthetic PR through the real window,
account/PR opening, epoch/cache host and installed Git. It proves both split
targets, offscreen unchanged source, actual unified multiline ordering, native
selection/caret/regex seeding, input admission, IME/multiline options, fixed
selection, Unicode/CRLF/EOF exact spans, wrapping, focus and editor/modality
guards. One thousand traversals and closed-query Next call no matcher, perform
no endpoint conversion and retain the same diff/index/record identities. Native
full-window renders run offscreen at 1.0x, 1.25x and 2.0x; they do not constitute
owner desktop, VoiceOver or other-platform device evidence. The PR courses below extend this verification to the Search surface.


## Native PR Search composition

`app::pr_search_controller` owns a separate source-search host and profile-owned
search effect executor. It binds the accepted active tab's exact comparison,
including a retained pair after remote refresh. Explicit comparison/tab gestures
retire obsolete hits immediately. Initial activation may create the first
ordinary tab without restarting the unchanged corpus query. Query and options
remain independent of pane Find. Account, PR and application authority retirement
clears sensitive query/filter text and records; shutdown drains every admitted
host/epoch terminal before destroying their ports.

The Files drawer contains native Files and Search tabs. View > Find in Files
enters PR Review, expands the drawer and focuses its independent two-row query.
Search Details discloses include/exclude filters, full comparison OIDs and skipped
side reasons. The normal surface shows short OIDs, match count and exact coverage;
unavailable reads offer Retry under the same scope. Typing never opens a file.
Return/click or F4/Shift+F4 explicitly activates an accepted occurrence. Escape
returns source focus while retaining PR results and any already scheduled query;
Close cancels both debounces and host work, restoring the previous drawer state.

`source_search_result_model` borrows immutable corpus/record collections and owns
only numeric file-group indexes. Ordered progress uses bounded row insertions;
it never resets the model or copies all preview/path strings. Data conversion
is lazy and preview-bounded. A selected/focused result row survives later batches.
File rows use native UI font; occurrence previews use configured source font.
File groups retain native disclosure controls. Match rows start at the viewport's
left content inset, with fixed L/R slots before the line-number and preview columns.
Scope, status, limits, skipped sides and wrap share one accessible description
and polite announcement. Palette roles and native style remain inherited.

Result activation validates complete authority, catalogue, comparison, actual
active tab, file identity, exact path, side, blob, query generation and original
span. It uses ordinary workbench file loading and the existing source selection
and reveal controller. A separate monotonically newer reading-gesture generation
cancels pending intent even on A→B→A navigation. Endpoint validation runs again
before reveal; keyboard focus follows the chosen exact side. Supplementary
historical catalogue metadata is bounded to the live tab set (at most 16 tabs
plus the incoming target). Sibling tabs, review drafts, Files filters, excluded
receipts and comparison choices remain under their existing owners. A visible
pane Find keeps its decorations/query while an external PR occurrence owns the
exact selection until another explicit pane or reading gesture.

The PR admission includes an 8 MiB projection/supplemental-tab reserve inside the
existing 192 MiB ceiling. Neither the 256-blob/16 MiB actor cache nor any matcher,
source, record or preview limit is enlarged. Execution exposes only numeric
compile/read/cache-hit/acquired-byte/scan/acquisition-time/matching-time/capacity
counters; queries, source content and credentials are never persisted or logged.

`application_source_pr_find` exercises unopened/filtered/excluded full-file hits,
exact side focus/F4, independent pane decorations, A→B→A cancellation, editor and
modal ownership, Close/Escape and accessible native layouts. Its 1.25x and 2.0x
variants render the complete offscreen window at supported widths.
`application_source_pr_search_scope` uses an actual two-head fixture/public Refresh
to distinguish historical-only membership from today's catalogue, preserve a
pinned pair, prove added/removed/rename deduplication and filters, retain sibling
state, and retire rapid query/tab/comparison/authority results.
`application_source_pr_search_retry` refuses one exact blob through an installed
Git capability wrapper, then restores it and clicks the public Retry button;
the successful retry retains the original scope and changes only query generation.
It represents unavailable material, without claiming a live network outage.

## Public performance regression course

`application_source_pr_search_performance` opens a deterministic 70-entry,
11,779,610-byte synthetic PR through normal account/PR/Review/Files/epoch/cache
and installed-Git paths. Sixty-four modified bulk files have 2,000 unchanged rows
per side. The fresh private profile/actor starts the cold matching/acquisition
phase with ordinary selected/prepared cache entries permitted and reported.
The OS page cache is neither flushed nor claimed cold. A case-changed warm query
then proves immutable-read reuse. Cold navigation changes the selected file
without abandoning the original nonempty query: yielded reads retry and all 68
eligible files reach their truthful terminal. The course also proves stable
focused rows during batches, a visible 10,000-record prefix/narrowing prompt,
1,000 cursor moves with no compile/scan/read/diff/index rebuild, actual in-flight
matcher cancellation, and complete shutdown.

Committed engineering thresholds are in
`tests/fixtures/source-search/regression-v1.json`. They bind time to first/final
results, navigation, clear-to-quiescence and process peak RSS. The separate
conservative owned-capacity envelope remains 192 MiB; process RSS includes the
rest of the application and is not that capacity bound. Measurement receipts
record Debug/native offscreen platform, hardware, actual concurrent load, cache
hits and acquisition versus matching time. They are regression observations,
not universal device latency promises or VoiceOver/other-platform judgments.
