---
id: "RFC-DRAFT-0015"
title: "Find in the current pane and across a pull request"
status: "Proposed"
---

# Find in the current pane and across a pull request

## Problem and grounded current behavior

On 2026-10-12 the owner requested efficient Find in the current pane and Find
in Files in the current PR, suggested limiting the latter to the current epoch,
and proposed VS Code as the default interaction model. This is a proposal for
read-only source search, not implementation acceptance.

The [workbench actions](../../../../src/adapters/qt/src/workbench_window.cpp)
have file/change navigation but no source Find commands. Settings already owns
its local Find shortcut, and the [Threads finder](../../../ux/workbench-specification.md#primary-layout)
searches conversations. Neither supplies source-content search.

The [accepted diff](../../../../src/core/include/bpr/core/diff_compute.hpp)
retains immutable UTF-8 documents with byte and line spans. The
[source presentation](../../../../src/adapters/qt/src/diff_workbench.cpp)
has separate left/right documents and a unified row projection that retains
source-side identity. The [reveal contract](../../../ux/diff-link-map-and-alignment-space-scrolling.md#5-programmatic-reveal)
already requires search navigation to use the shared comparison controller.

A [document tab](../../../../src/core/include/bpr/core/document_tabs.hpp)
can retain a comparison older than the window's comparison. The current
[file observation](../../../../src/application/include/bpr/application/workbench_runtime_ports.hpp)
comes from the accepted PR snapshot and separates actionable and withheld
files; it is not an arbitrary historical file catalogue. The
[epoch host](../../../../src/application/include/bpr/application/epoch_application_host.hpp)
owns exact path/blob resolution and sparse acquisition through the retained
repository actor. Its existing changed-path query checks supplied paths against
the current accepted head; it does not enumerate an older comparison's files.

[VS Code's editing guide](https://code.visualstudio.com/docs/editing/codebasics#find-and-replace)
provides the interaction reference: an editor Find control, incremental matches,
selection seeding, case/word/regex options, and file-grouped cross-file results.
Its [keyboard reference](https://code.visualstudio.com/docs/reference/default-keybindings#search)
informs the shortcuts below. BPR's immutable comparisons, source identities,
native presentation and resource limits remain its own contract.

## Proposed behavior and boundaries

### Scope decisions

**Q1 — answered: which file content?** In the authoring reply for this draft,
the owner selected “Full file contents on both sides, merging identical matches
(Recommended).” Search the complete contents of the PR's changed files on both
comparison sides, including unchanged context, with identical source matches
combined as defined below.

**Q2 — answered: which epoch?** In the same reply, the owner selected “Follow
the active comparison (Recommended).” Follow the active tab's exact selected
comparison, even when older than the latest epoch. With no active tab use the
window's accepted comparison. Search only that chosen pair, never all retained
epochs.

Both scope questions are settled. The RFC remains Proposed; these answers
record the content and revision choices, not implementation acceptance.

“Current pane” in this proposal means the source reader: left, right, or unified.
Find in comment bodies, Overview, local Review Cycles and editable draft documents
is deferred. Their existing local shortcuts and text editing keep ownership;
invoking Find there must never search a hidden source pane. The existing Threads
finder remains the way to search conversations. Replace, repository-wide search,
search history across epochs and a persistent index are outside this proposal.

### Find in a pane

Find searches the whole focused source document, including lines outside the
viewport. Split mode searches only the focused side. If invoked from Review's
chrome, use the most recently focused visible source pane, initially the right
pane or the sole available side. Moving focus into the Find bar retains that
target; explicitly invoking Find in the other pane retargets it.

Unified mode searches the source text represented by its rows: unchanged rows
once, removed rows on the left, added rows on the right. Line numbers, diff
prefixes, toolbar text and comments are not search text. Multiline matches
require contiguous lines from one actual source version; visual adjacency of
a removed and added row must not manufacture a match across versions.

Opening Find focuses and selects the query. Seed from a nonempty native source
text selection, otherwise the word at a real text caret, otherwise the previous
query in this window. A semantic whole-line cursor is not a text selection.
Escape regex metacharacters when seeding literal selected text into regex mode.
An oversized seed leaves the query unchanged and reports the input limit.

Use a compact bar attached to the pane's upper edge, with query, three match
options, match position/count, previous/next, Find in Selection and Close.
It must not obscure the active occurrence. Show all visible matches with a
quiet highlight, distinguish the current one with an outline, and mark their
positions on the existing scroll track without altering the link map. Paint
work scales with visible highlights and track pixels, not total occurrences.

Search while typing. Reveal the first match at or after the original caret or
reading position, keeping query focus; editing the query uses that same starting
position until explicit match navigation. Next/Previous move through source
order and wrap with a brief accessible indication. “No results” is distinct
from still searching, invalid input and incomplete results. Empty input clears
the results without scanning or moving the reader.

Find in Selection is off initially. Turning it on captures the selected source
range; match navigation does not move that boundary. It is unavailable without
a valid range and clears when the document changes. In unified view a range
crossing versions is unavailable with an explanation. Escape or Close dismisses
the bar, clears its decorations and returns focus to the target at the current
match. The last query/options remain available for subsequent Find or Next.

### Find in Files

Open a **Search** tab beside **Files** in the existing left drawer; do not add
a fifth review modality or a permanently visible toolbar. From Overview,
Discuss or Threads, the command enters Review, restores its ordinary drawer
arrangement, then opens Search and focuses its query. Unsent work is retained.
Opening an empty search performs no source reads. The Files tab retains its
filters and position; dismissing Search restores the previous drawer tab and
visibility. Escape from Search returns focus to source without discarding results;
the explicit Close action dismisses the view and cancels unfinished work.

Keep separate query/options for the two commands. On first opening PR search,
seed from selected source text, otherwise the pane Find query, otherwise its
previous query, using the same literal-seeding rule. Reinvoking an already open
Search focuses its existing query. Pane Find owns match decorations while its
bar is open; opening a PR result still reveals and selects that exact occurrence
without changing either query.

Show the PR and exact comparison labels above the results, with commit identities
available in the existing disclosure style. The default corpus is all changed
file entries for that comparison, ignoring Files filters, private excluded/read
receipts and whether a file has ever been opened. Search full source documents,
not provider patch excerpts. Include added, removed, renamed and copied entries;
use each side's exact path. A pure rename can contain matches despite no changed
lines. Binary files, submodules, undecodable text and unavailable material are
reported as skipped with reasons; never follow a symlink into the local filesystem.

Membership must be complete and bound to the selected comparison. Reuse an
accepted file catalogue only when its identity proves that scope. Otherwise
derive a bounded catalogue from the exact retained trees through the host's Git
capability. This is a required extension, not something the current changed-path
query already does. A missing historical catalogue/tree makes the scope
unavailable; substituting today's catalogue or silently searching its intersection
is not allowed. Rename association must be evidenced, with unpaired old/new
paths represented honestly when no association is available.

Group results by repository path, with match counts and line previews. Each
occurrence has its source side, line and column; a rename's header shows old
and new paths. Sort by exact repository path, then left before right, then source
offset, independently of completion timing. Insert batches without resetting
the tree or losing its focused row. Do not auto-open the first cross-file result
while typing. A click or Return opens the chosen occurrence; Up/Down moves the
result cursor and Left/Right folds or unfolds file groups.

Combine identical matches only when identity proves equivalence: the same blob
and byte span for the same file entry can be searched once and displayed with
both side locations. Different blobs remain separate side-labelled occurrences,
even when preview text is equal. Do not run a semantic diff for every file just
to merge repeated context or infer correspondence from similar snippets.

Search Details reveals optional **Files to include** and **Files to exclude**
filters. They use case-sensitive repository-relative paths on every platform,
forward slashes, comma-separated alternatives and the documented glob operators
`*`, `?`, `**`, `{a,b}` and character classes. A pattern without `/` matches a
basename at any depth; `./` anchors to the repository root. Exclude wins. An
entry is included when either side's path is included and neither is excluded.
Empty filters include all entries. Parse commas inside braces/classes correctly
and support escaping literal metacharacters; report invalid input before work.
No developer checkout, `.gitignore` or hidden editor settings change this corpus.

### Matching and result navigation

Both commands use the same matching rules: literal, case-insensitive search by
default, with independent Match Case, Whole Word and Regular Expression options.
Whole Word treats Unicode letters, marks, numbers and underscore as word
characters. Case-insensitive matching uses locale-independent Unicode simple
case folding, without accent removal or Unicode normalization. An empty query
has no matches. Matches are nonoverlapping; zero-width regex results advance
by one Unicode scalar after emission, with the terminal position emitted once.

Support pasted multiline queries and Ctrl+Enter to insert a newline in either
query field. Search a logical LF view of the source's line endings and map spans
back to the original bytes. Regex uses the
[RE2 syntax subset](https://github.com/google/re2/wiki/Syntax), including groups,
alternation, classes and quantifiers, with line anchors and explicit newline
matching; dot does not match a newline unless requested by a supported flag.
Backreferences and lookaround report unsupported syntax inline. This deliberate
compatibility limit avoids an unbounded backtracking path; the portable engine
choice is an implementation detail, and both commands must accept the same
syntax. Invalid patterns never retain apparently current hits from the old query.

Selecting a result uses the ordinary active-tab file-open path, retaining the
search's exact comparison and leaving sibling tabs unchanged. Bind the pending
reveal to PR authority, catalogue generation, tab, comparison, path, side, blob,
query generation and source span. After loading, verify the target and select
the exact occurrence, converting UTF-8 offsets to Qt text positions only at
presentation. Reveal vertically through the shared controller and horizontally
only in the target pane. A missing side does not become a neighbouring line.

The source's semantic line selection follows the occurrence, so a subsequent
explicit Comment command has the correct line. Search highlights themselves
are a separate decoration and cannot manufacture a multi-line draft range.
Search performs no review, viewed/excluded receipt, publication or file mutation;
an explicit result open may retain ordinary reading-position behavior.

Switching files within the same search scope keeps PR results. Changing the
comparison or active tab retargets the visible search, retains query/options,
clears obsolete hits and schedules a new search. A newer remote epoch does not
silently move a pinned older tab. Any authority or corpus invalidation rejects
old results immediately. A later user navigation cancels a pending result reveal.
Changing PR/account/profile clears query text and results; nothing is persisted
across application restarts or copied into the system Find pasteboard or logs.

### Commands, focus and native presentation

Expose the actions in **View > Find**, with their availability projected from
one command state. Mirror them disabled in the windowless macOS menu.

| Action | macOS | Windows/Linux |
|---|---|---|
| Find | Command+F | Ctrl+F |
| Find in Files | Command+Shift+F | Ctrl+Shift+F |
| Next / previous pane match | Command+G / Command+Shift+G; F3 / Shift+F3 | F3 / Shift+F3 |
| Next / previous PR result | F4 / Shift+F4 | F4 / Shift+F4 |

Enter/Shift+Enter in the pane query navigate matches. Enter in the PR query
flushes the pending search immediately; it does not open an arbitrary result.
F4 navigation keeps source focus for successive reading and wraps with an
announcement. Next/Previous commands are disabled when there is no valid query
or navigable hit. They do not hijack keys in an unrelated text editor or modal
dialog. Find in Files remains a PR command, including from a PR composer;
Home, local cycles and Settings do not search a retained hidden PR.

The design applies the required frontend-design skill to BPR's existing native
workbench. Use inherited palette, native UI font and the configured source font
for snippets; distinguish scope, active match and partial status by wording and
structure as well as color. Reuse native toolbar/toggle styling and focus rings.
No fixed palette, QSS, new font family or animated result reordering is needed.
At narrow widths wrap controls into a second row instead of hiding options or
reducing the source to an unusable sliver. Label every icon and toggle, expose
counts and sides to accessibility, and coalesce announcements after each accepted
query/result update rather than announcing every arriving row.

```text
Left drawer                         Source pane
Files | Search                      [query  Aa  Word  .*  2/8  ↑ ↓  … ×]
[query       Aa Word .*]             source with current occurrence outlined
PR #42  Base → Update 3
Search Details ▸
8 matches in 3 files                 Results preserve exact source locations;
▾ src/cache.cpp  4                   they never replace the comparison's
  Left   18:7  acquire_cache(...)    scroll authority or line mapping.
  Right  24:7  acquire_cache(...)
```

### Efficient execution and ownership

The [existing architecture](../../../architecture/system-architecture.md#performance-contract)
applies: core owns query/scope policy, deterministic match semantics, compact
session identities and stale acceptance; the host owns immutable material,
matcher execution, indexes, scheduling, cancellation and caches; Qt owns focus,
controls, incremental result models and visible decorations. Text/corpus/result
collections stay outside compact interaction state. Workers return typed batches
through the serialized decision lane and never read widgets.

Use the accepted pane document for Find. PR search streams exact file blobs
through the existing epoch repository actor and acquisition boundary, without a
checkout, external search executable or whole-repository content index. Missing
sparse blobs may be acquired under the PR's existing read authority; report that
progress and honor transport limits/offline state. Material acquisition is keyed
by immutable identity, independent of query text, so successive query generations
reuse completed reads and coalesce a still-needed in-flight read.

Compile a query once, use a literal fast path, and scan on demand. Do not compute
syntax or semantic diffs for unopened files. Existing immutable caches supply
reuse, bounded by their current budgets; search must not pin every visited blob
or evict the selected comparison to accumulate an entire corpus. Navigation and
pane Find outrank the PR scan. Bound acquisition to one outstanding background
batch per repository actor and yield between blobs/scan slices.

The proposed operational defaults are:

| Limit | Behavior |
|---|---|
| Typing debounce | 150 ms; Enter bypasses it; IME composition waits for committed input. |
| Query size | 4 KiB UTF-8; refuse excess with an inline explanation. |
| Stored results | 10,000 match records per search, each with at most two exact side locations; stop with “10,000 shown; limit reached” and a narrowing prompt. Use “10,000+” only after observing another match. |
| Result preview | At most 240 Unicode scalars around the occurrence; full identity/span is retained separately. |
| Publication batches | At most 128 results; incremental UI delivery coalesced to at most 10 updates/second, plus terminal updates. |
| File admission | Existing selected-source byte/line ceilings, currently 16 MiB and 200,000 lines per side, or any stricter effective transport limit. |

Also require explicit bounded aggregate memory for matcher state, results,
previews and search caches, with an honest resource-limit terminal. Choose and
record that ceiling during implementation measurements; do not silently enlarge
the [existing host cache](../../../../src/application/include/bpr/application/diff_runtime_ports.hpp).
Compile and execution budgets must cover regex work as well as literal scanning.
Replacing a query cancels its old matcher; a shared immutable read may continue
only while the replacement still needs it. Closing or clearing stops new work
promptly. Every admitted request still reaches its terminal through normal host
lifecycle rules. Limit results are a visible prefix in the defined result order;
wrap navigation describes that limited set rather than implying full coverage.

Progress names searched/eligible files and skipped sides. A complete empty
search may say “No results”; a stopped, capped, failed or partly unavailable
search instead says what was searched and what remains. Already accepted hits
remain usable with the partial label. Offer Retry for unavailable reads and
narrowing by query/path for limits. Never silently sample the corpus or count
unsearched material as zero matches.

Performance evidence must separate warm local matching from cold acquisition.
Record fixture size, cache state, hardware, cancellation latency, peak memory
and time to first/final results under the
[existing measurement convention](../../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#performance-budgets).
Commit measured regression thresholds during implementation. Independently of
timing, scrolling, pane match traversal and moving the result cursor perform no
text rescan, source read, query recompilation or diff recomputation. Opening a
cross-file hit may perform that file's ordinary exact load and comparison, never
a new PR scan. A query edit may rescan its bounded corpus on a worker; it must
not rebuild every document or retain the whole corpus. Core and host checks need
no QApplication.

Implementation updates the command map, source-search behavior and resource
ownership documentation. Engine selection, precise data structures and measured
performance thresholds are engineering work, not additional owner questions.

## Acceptance

- **AC01:** Find in either split pane reaches offscreen unchanged text only in
  that pane. Unified Find follows its defined source order without duplicate
  unchanged rows or a fabricated cross-version multiline match.
- **AC02:** The shipped Find bar demonstrates selection seeding, shared match
  options, multiline input, fixed Find in Selection bounds, match counts,
  wrapping and focus restoration as specified. Unicode/line-ending mappings
  select the exact source span; invalid or unsupported regex cannot freeze the
  UI or leave an old result labelled current.
- **AC03:** PR search finds full-file matches on both sides of the active
  comparison in unopened, filtered, excluded, added, removed and renamed
  source. A historical-only path distinguishes correct membership from reuse
  of today's catalogue.
- **AC04:** Real result activation and F4 traversal open the exact file, side,
  span and comparison through the existing selection/reveal path, preserve
  sibling tabs and review state, and follow the defined deduplication and path
  filters.
- **AC05:** Rapid query, tab, epoch and authority changes, followed by reversed
  completion order, cannot publish stale matches or redirect a newer reading
  gesture. Clearing/closing a search quiesces its work; refresh preserves a
  still-valid pinned comparison without searching history.
- **AC06:** Incomplete material, binary/unsupported sides, cancellation,
  transport failure and search limits visibly qualify coverage. Only a complete
  empty corpus reports an unqualified no-match result, and retry uses the same
  exact authority rather than a newer revision.
- **AC07:** The composed action/key paths have unique shortcuts, correct
  modality/editor ownership, accessible match/scope/status descriptions and
  usable native layouts at supported widths. Rendered highlights coexist with
  syntax, selection and thread markers without altering comparison geometry.
- **AC08:** A large-review public-path fixture records warm/cold performance,
  cancellation and bounded memory, and proves the structural execution rules
  under “Efficient execution and ownership.” Source navigation remains usable
  during a cold scan; core-only and architecture-boundary checks pass.
