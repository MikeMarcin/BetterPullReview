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
measures 1,528 bytes on the M1 Apple Silicon build. That is a state measurement,
not an aggregate search-memory measurement.

The scope binds application session, complete PR authority, exact available
comparison anchors/commits, active tab, corpus generation and target. A pane
also binds file, selected comparison digest and available side blobs. A missing
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
port currently returns terminal coverage/count facts; later matcher/corpus work
will supply exact records and incremental batches under this same identity.

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
or review mutation capability is introduced. M2/M3 still owe the explicit
aggregate matcher/result/preview/cache memory ceiling, cancellation budgets,
streaming order and selected-reader priority. The complete RFC criteria remain
open until the composed application and its performance fixture prove them.

## Verification surface

`core_source_search_policy` exercises exact scope/query admission, reversed and
foreign completions, duplication, authority gaps, exhaustion, coverage and wrap
without `QApplication`. `application_source_search_host` exercises the public
host through separate manual effect/decision executors: independent queries,
rapid coalescing, invalid input, cancellation, reentrant feedback/shutdown,
projection/worker/submission failures and observable quiescence. Its 1,000-step
traversal leaves the execution count unchanged. These checks prove the session
foundation, not matching, source membership, pixels or installed UI behavior.
