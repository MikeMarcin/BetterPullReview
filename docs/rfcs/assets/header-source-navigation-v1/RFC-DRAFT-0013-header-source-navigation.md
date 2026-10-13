---
id: "RFC-DRAFT-0013"
title: "Jump between a review's header and source files"
status: "Proposed"
---

# Jump between a review's header and source files

## Problem and grounded current behavior

On 2026-10-12 the owner requested a best-effort jump between a C++ source
file and its header when both belong to the review, including layouts such
as `src/widget.cpp` and `inc/widget.h`. They suggested Alt/Option+O. The
request is bidirectional; it does not require an IDE or a project index.

The existing [workbench intents](../../../../src/core/include/bpr/core/workbench_interaction.hpp)
offer direct file selection and previous/next navigation, but no counterpart
command. The [accepted file observation](../../../../src/application/include/bpr/application/workbench_runtime_ports.hpp)
already carries stable file identities, repository paths, and a separate
`Withheld` collection for files excluded by private review receipts. Searching
only the visible Files rows would miss valid counterparts.

The [document-tab transition](../../../../src/core/src/document_tabs.cpp)
can replace the active tab's file while retaining its exact comparison, and
[composition](../../../../src/app/src/github_application.cpp) already connects
file gestures to selected-diff loading and tab observation. The
[workbench command map](../../../ux/workbench-specification.md)
and [window actions](../../../../src/adapters/qt/src/workbench_window.cpp)
contain no Alt+O binding. Existing commands protect native editor input and
require unique shortcuts in the assembled window.

## Proposed behavior and boundaries

### Match files already in the review

Add **Go to header/source** to pull-request review navigation. A source file
looks for headers and a header looks for source files, using these suffixes:

| Family | Extensions |
|---|---|
| Header | `.h`, `.hh`, `.hpp`, `.hxx` |
| Source | `.c`, `.cc`, `.cpp`, `.cxx` |

Compare extensions without ASCII case sensitivity. Match the nonempty basename
before the final extension exactly, with case-sensitive spelling on every
platform. Folder names and depth do not restrict membership: `src/widget.cpp`
can match `inc/widget.h`, `public/api/widget.hpp`, or another location in the
same review. Do not hardcode `src`, `inc`, or `include` substitutions.

Use the complete accepted changed-file catalogue for the current pull request,
including files hidden by a presentation filter or review receipt. Use each
entry's accepted current repository path, or its reported removed-file path;
a renamed entry's previous name is not a second matching alias. Deduplicate
by stable file identity. Files outside that catalogue are not candidates.
An unobserved or refused catalogue is unavailable, never evidence of no match.

This is a filename heuristic. It does not inspect includes, parse code, consult
an LSP, probe a developer checkout, search the full repository, or infer differently
named pairs. Local Review Cycles are deferred from this initial PR command.

### Resolve multiple candidates

**Decision Q1 — open.** The owner has been asked whether to prefer a same-folder
match and use a chooser for ambiguity, or cycle through matches. No answer has
been recorded. The recommendation below is proposed behavior; multiple-match
scope remains unready until this choice is answered.

Recommended behavior: if any candidates share the selected file's directory,
consider that group first. Otherwise consider all matching candidates. One
candidate in the preferred group jumps immediately. Multiple candidates open
a compact native chooser showing their full repository paths in deterministic
path order. Return or activation chooses one; Escape cancels without navigation
and restores focus. There is no arbitrary extension preference or hidden choice
based on the Files sort order. Repeating the command resolves afresh from the
newly selected file; it does not maintain an inferred pairing history.

### Navigate through the existing review path

The chosen file replaces the file in the active document tab, retaining that
tab's exact comparison. It neither creates nor activates another tab merely
because the counterpart is already open there. Use ordinary file-open selection
and initial reading-position behavior; matching names do not imply matching line
numbers. Both split and unified comparisons use the same command. Keep focus
in the source comparison after a successful jump.

Selecting a counterpart hidden by a filter or receipt must still load it.
The jump does not change those filters, mark a file reviewed, or alter the
previous/next actionable order. It uses the existing exact-path/blob resolver;
an unavailable target at the retained comparison reports its ordinary failure
instead of substituting a different file or a newer comparison.

Bind resolution and any chooser selection to the originating window, PR
authority, accepted catalogue generation, active tab, selected file and exact
comparison. A change to that context invalidates the pending choice. Revalidate
membership before opening; existing selected-diff stale-completion checks remain
in force.

### Command and keyboard behavior

Expose **View > Go to header/source** with **Alt+O** on Windows/Linux and
**Option+O** on macOS. Register one command per review window and mirror it
disabled in the resident macOS menu when no window is open. This secondary
navigation action adds no permanent toolbar control or new settings.

Offer the shortcut in Review while a supported file is selected, including
focus in its code, Files drawer or tab strip. Editable fields and composers
retain their native Option/Alt text input; the shortcut must not insert `ø`
into code or take it away from an editor. Home and other review modalities do
not navigate a retained hidden source. The menu shares the command's projected
availability and explains disabled states.

With a supported file and an accepted catalogue, a command with no candidate
leaves the view unchanged and gives a brief nonmodal, accessible status such
as “No matching header in this review” or “No matching source file in this
review.” An unavailable catalogue gets a distinct explanation. Unsupported
files and an absent selection disable the action with their reason.

### Ownership and bounded work

Follow [the workbench architecture](../../../architecture/system-architecture.md#rfc-bpr-0005-workbench-allocation):
core owns deterministic matching, preference and command legality; the host
owns the accepted catalogue, indexed lookup and exact context; Qt translates
the shortcut and presents the action, status and proposed chooser. Core may
inspect borrowed bounded path strings, but interaction state gains no retained
file list, filesystem objects, file content or effects.

Prepare lookup when the accepted catalogue changes, within its existing
admission limits. A command looks up its stem and matching candidates without
scanning the whole review or loading candidate blobs. Refresh only affected
projections. Navigation may perform the selected file's ordinary loading and
reading-state persistence; it grants no provider mutation authority and adds
no persistent pairing preference. Implementation updates the owning command
map and navigation architecture documentation.

## Acceptance

- **AC01:** In the composed PR review, the shortcut jumps from
  `src/widget.cpp` to the sole `inc/widget.h` counterpart and back, with both
  split and unified presentation. Supported suffix and case behavior follows
  “Match files already in the review”; an unrelated or outside-review file
  cannot become a target.
- **AC02:** The agreed multiple-candidate behavior distinguishes same-folder
  candidates from cross-folder candidates and handles ambiguity predictably.
  Cancelling any choice changes no reading state. This criterion is unready
  while Q1 remains open.
- **AC03:** A counterpart hidden by the Files filter or a private receipt
  remains reachable without changing the filter, receipt or actionable order.
  No-match, unsupported, no-selection and unavailable-catalogue states produce
  the distinct behavior specified under “Command and keyboard behavior.”
- **AC04:** A jump retains an active tab's older comparison and leaves sibling
  tabs unchanged. A source/tab/PR/catalogue/comparison change before commitment
  rejects the old choice, and a late diff cannot replace the latest selection.
  A missing target at that comparison is reported without fallback.
- **AC05:** The real QAction and physical key-event path reach the same
  selection operation, with a unique assembled shortcut and accessible menu,
  status and choice text. Editable Option/Alt input and inactive modalities
  retain their behavior; chooser completion/cancellation has the defined focus.
- **AC06:** A large accepted review performs indexed counterpart lookup without
  a full-catalogue scan per gesture, speculative candidate reads or model reset.
  Core matching is verifiable without QApplication, and the existing
  architecture boundaries continue to pass.
