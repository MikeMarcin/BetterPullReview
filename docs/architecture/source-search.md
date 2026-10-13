# Source search ownership and session contract

[RFC-BPR-0077](../rfcs/RFC-BPR-0077-find-and-find-in-files.md) owns the complete
Find and Find in Files behavior. Its first milestone, [[BPR-1419]], supplies
the deterministic session and Qt-free effect lifecycle. The shared matcher,
exact epoch corpus, native commands, source decorations and result reveal are
subsequent milestones; this foundation does not make a source Find command
available in the application.

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
corpus will add streamed records/batches under the same identity. Payload size,
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

No persistence, source cache, Find pasteboard, logging of content, acquisition
or review mutation capability is introduced. The M2 limits below bound one matcher/entry execution. M3 still owes aggregate
corpus/cache/result admission, streamed progress and selected-reader priority;
M2 does not pin or acquire source. The complete RFC criteria remain
open until the composed application and its performance fixture prove them.

## Verification surface

`core_source_search_policy` exercises exact scope/query admission, reversed and
foreign completions, duplication, authority gaps, exhaustion, coverage and wrap
without `QApplication`. `application_source_search_host` exercises the public
host through separate manual effect/decision executors: independent queries,
rapid coalescing, invalid input, cancellation, reentrant feedback/shutdown,
projection/worker/submission failures and observable quiescence. Its 1,000-step
traversal leaves the execution count unchanged. These checks prove session/record acceptance. The M2 checks below prove
matching; source membership, pixels and installed UI behavior remain later work.

## Shared matching and source plans

`compile_source_match` compiles once and returns an immutable host-owned program.
Literal matching uses scalar KMP; regex uses a prioritized Thompson VM with
no recursive backtracking. Both use Unicode 17 simple C/S case folding and
L/M/N plus underscore for Whole Word. No locale, accent removal, normalization
or multi-scalar fold is used. The [Unicode provenance](../../third_party/unicode/PROVENANCE.md)
binds generated tables, hashes, notice and corresponding source.

The supported [RE2 subset](https://github.com/google/re2/wiki/Syntax) is literals,
groups/`(?:...)`, alternation, ranges/classes, `* + ? {n} {n,m} {n,}` with greedy
or lazy preference, `i/m/s/U` flags (global or scoped), `^ $ \A \z \b \B`,
ASCII `d/s/w` classes and their complements, Unicode general categories
(`\p{L}`, `\p{Lu}`, `\P{N}`, etc.), escaped punctuation, `a/f/t/n/r` and
hex escapes. Line-anchor mode defaults on, dot-all off; flags can override both.
Named captures, Unicode scripts, POSIX classes, class-union complements,
octal/byte/quoted escapes, lookaround and backreferences return unsupported
syntax. Malformed patterns return invalid syntax; diagnostics retain only code
and offset. Groups do not publish captures. Nullable repeats retain RE2 priority.

The scanner builds one logical LF view, normalizing CR and CRLF while retaining
original byte boundaries. Results are nonoverlapping exact original UTF-8 spans;
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
| Compiled instructions / nesting / counted repetition | 8,192 / 64 / 1,000 |
| Compile / entry scan work | 1,048,576 / 134,217,728 counted steps |
| Stop polling | Initially, then within 1,024 counted steps |
| Matcher/entry owned storage | 64 MiB conservative capacity reservation |
| Result records plus previews | 16 MiB / at most 10,000 records |
| Preview | 240 scalars / at most 960 UTF-8 bytes |

Compile scratch, VM lists, logical text, indexes, source plans and result capacity
are admitted before allocation. Resource exhaustion returns a qualified terminal
and any already assembled usable prefix. `AdditionalMatchObserved` is set only
after observing another occurrence beyond the record cap. Borrowed immutable
source/diff material remains under its existing owner/cache limits. The pane
bridge reserves its transfer vector separately inside the same ceiling. These
are capacity/work bounds, not RSS or universal latency measurements; full PR
aggregate memory and warm/cold public-path thresholds remain M3/M5 obligations.

`core_source_matcher` exercises Unicode/LF spans, unsupported syntax, glob policy,
real unified plans, selection, deduplication and resource/cancellation limits.
`core_source_matcher_reference` compares a portable subset with a bounded Python
oracle. An additional disposable official RE2 oracle checks nullable loops and
all 4,891 reference spans; Python differs on that priority rule. The public host
fixture also proves exact record acceptance, invalid-pattern clearing, wrong-blob
refusal and 1,000 traversals without a matcher call. No QApplication is involved.
