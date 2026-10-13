# Header/source navigation

[RFC-BPR-0074](../rfcs/RFC-BPR-0074-header-source-navigation.md) owns best-effort
counterpart navigation inside one accepted pull-request review. The owner’s r4
amendment keeps exact same basenames and resolves an obvious closest candidate:
same directory first, otherwise the longest shared trailing directory-component
sequence. Equal best candidates leave the view unchanged. There is no extension
preference, chooser, cycling or source/include-folder substitution.

`core::classify_header_source_path` borrows bounded path bytes and classifies the
eight suffixes with ASCII-only extension folding. Basenames and directory
components keep exact spelling. Core defines the reversed component order and
resolves bounded candidate summaries; interaction state retains no catalogue
or path. `header_source_candidates` summarizes either zero, one distinct stable
identity or ambiguity.

The workbench host builds `header_source_index` when a bounded accepted catalogue
lands. The index groups exact stems and traverses directories from their final
component toward their root. Each node summarizes candidates of each family,
and its exact-directory group is separate from descendants. A query uses the
opposite family, retains the deepest nonempty suffix group and asks core to
prefer an exact-directory group where present. The index’s root represents
zero-overlap candidates. Flat node storage avoids recursive tree destruction;
lookup visits directory components, without scanning matching candidates or
reading their contents.

Both Actionable and Withheld entries participate. Stable identities are deduplicated
before indexing; only each entry’s accepted CurrentPath is used, including a
removed file’s reported path. PreviousPath is never an alias. Paths cross the
native filesystem representation as explicit UTF-8 bytes, preserving non-ASCII
basenames on every platform. Counterpart lookup neither reads a checkout nor
queries Git, a provider, includes, an IDE or an LSP.

`workbench_application_host::header_source_target` is a decision-lane query with
explicit matched, no-match, ambiguous, unsupported, no-selection and
unavailable-catalogue results. Authority replacement and invalidation clear its
index before replacement effects begin; failed or unobserved replacement data
cannot appear as an empty accepted catalogue. Receipt-only changes to actionable
order keep the same complete catalogue. Accepted catalogue refresh rebuilds the
index once. Ordinary exact selection can load a withheld entry through the
existing selected-diff path without changing the actionable order or receipt.

The host supplies one bounded `HeaderSource` availability observation to core's
workbench state. Core projects `go_to_header_source` legality with distinct
no-selection, unsupported-path, unavailable-catalogue and inactive-modality
reasons. Publishing availability reuses the retained selected entry; it neither
runs counterpart lookup nor adds a catalogue scan or selected-file index query.

Qt presents one View action and window shortcut, with no toolbar or setting.
The native menu equivalent is disabled while an editable field or composer has
focus. Successful navigation focuses the visible split or unified source editor;
no-match and ambiguity announce polite nonmodal status. Application focus ingress
is disconnected before window children destruct, including child settings windows
whose teardown changes focus after the actions have gone.

Composition resolves synchronously on the decision lane, then queues an exact
open bound to the window lifetime/context, accepted PR authority, catalogue
generation, active tab, file, selected-diff request and comparison. It revalidates
all identities and membership before dispatching the existing exact-file intent.
A changed tab's pending visible selection also rejects the old gesture. Ordinary
reading-line observations do not change this context. The existing file lane
replaces the active tab and retains its comparison, including an earlier pair;
sibling views are untouched. Ordinary selected-diff admission rejects late results
and reports targets absent from the retained comparison without newer fallback.

The [BPR-1416 closeout](../issues/30-in-progress/BPR-1416-composed-header-source-command.md)
and [verification ledger](../verification/RFC-BPR-0074.md) bind the composed command,
physical-key, render and indexed-work evidence to the implemented candidate.
