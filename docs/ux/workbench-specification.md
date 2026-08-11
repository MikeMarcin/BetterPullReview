# Workbench Specification

## Architectural interaction contract

- Navigation, epoch selection, current file/change, command legality, and review semantics are functional-core state and policy.
- GitHub/Git/ledger/diff observations and background work are supplied by the application host.
- The main window, drawers, toolbars, actions, Qt models, focus, splitters, scrolling, painting, and accessibility are Qt-presentation-adapter responsibilities.
- Every user gesture becomes a semantic intent, and every bounded state change reaches Qt as an incremental projection or delta. No workbench widget directly calls an effectful service.
- Epoch rows and comparison endpoints use stable authority, catalog-incarnation,
  and anchor IDs. Qt never derives an epoch cause, resolves an alias, checks
  object residency, opens a cache, or infers a fallback.

## Primary layout

```text
┌ Header: product, PR, status counters, account/host ┐
├ Files | Previous File | Next File | Previous Change | Next Change | review controls | spacer | Conversations ┤
├──────── file drawer ────────┬──────── comparison workbench ────────┬──── conversation drawer ────┤
│ filters and review map       │ independent left/right epoch pickers  │ threads, defects, AI tabs   │
│                              │ natural-layout panes + DiffLinkMap      │                              │
│                              │ one shared vertical scrollbar          │                              │
│                              ├ full-width selected-change detail pane ┤                              │
└──────────────────────────────┴────────────────────────────────────────┴──────────────────────────────┘
```

The file and conversation regions are true collapsible drawers. Their open widths are independently resizable and persisted. Collapsing one returns its width to the central review surface. Their toggles remain at the opposite edges of one compact icon-only row, so both surfaces stay reopenable without a second drawer toolbar.

## GitHub host and pull-request presentation

Before the complete workbench is publicly composed, the GitHub presentation
surface exposes the same architectural contract in a bounded read-only
window:

- The user selects a configured profile, never an account identity. The
  active hostname, transport, and authenticated account are visible,
  read-only outputs of the accepted session.
- Connect, Open, and Refresh emit semantic application intents. Show Review
  Threads is Qt-local presentation behavior that selects, expands, scrolls
  to, and focuses the first available review thread.
- Connection and pull-request locator controls form compact responsive rows.
  The active host, transport, and account remain visible, while capability
  and rate-limit facts are behind a Connection Details disclosure. At narrow
  widths locator actions move below their inputs, and the description and
  conversation regions change from a horizontal to a vertical split.
- Repository and pull-request input, command legality, capability tri-state,
  rate-limit observations, normalized metadata, conversations, status, and
  bounded diagnostic text remain distinct accessible fields.
- The pull-request title, author, branch, state, and counts establish the
  primary hierarchy. Exact commit identities remain available behind a
  Technical Details disclosure rather than competing with the review
  content.
- The viewport has explicit `disconnected`, `connecting`, `empty`, `loading`,
  `error`, `cancelled`, `loaded`, `refreshing`, `loaded-error`, and
  `loaded-cancelled` presentation states. The last three nonterminal/failure
  variants retain accepted content. A failed or canceled refresh cannot
  replace the last accepted content with an indeterminate spinner.
- Remote title, body, author, path, and comment values render as literal plain
  text. Provider records, requests, URLs, transports, vaults, credentials, and
  authentication material never enter the Qt adapter.
- Selecting another identity or opening another target immediately clears
  stale identity-sensitive content. Refresh keeps the last accepted content
  visible while a replacement is loading or if that replacement fails,
  cancels, or becomes stale.
- Conversation rows have stable semantic identities and update by insert,
  remove, minimal move, or data change without resetting the complete model.
- Conversation rows use a wrapped, non-eliding delegate that preserves native
  selection and focus painting. The first accepted projection expands the
  initial hierarchy; later refreshes preserve local expansion and restore the
  current selection when its identity remains.
- Each published status/diagnostic update produces at most one combined,
  polite accessible announcement, with repeated identical announcements
  suppressed.

The window remains usable through a minimum-size scroll surface, exposes
keyboard shortcuts and accessible names/descriptions, and is rendered at
1.0x, 1.25x, and 2.0x scale in the Qt harness. It inherits the active native
palette, style, and font without a forced palette, style, or QSS, and responds
to application palette, font, style, and theme changes. Offscreen
forced-style renders verify layout only. The reusable RFC-BPR-0003 comparison
control follows this same native-System inheritance. User-selectable System,
Light, Dark, and high-contrast profiles remain RFC-BPR-0006 work, with a fresh
installation defaulting to System.

## Primary actions

Previous/next file and previous/next change are always visible in one compact
top row. Files is the far-left action, then Previous file, Next file, Previous
change, and Next change appear in that semantic order. Bounded review-details
and private-progress controls follow, a flexible spacer absorbs remaining
width, and Conversations is the far-right action. The six drawer/navigation
presenters are icon-only, but each retains a stable visible tooltip, accessible
name and description, strong-focus stop, and keyboard shortcut. No text toolbar
appears at a narrower width.

`Next actionable` remains available as a secondary command and uses the local actionability projection.

RFC-BPR-0005 implements this shell. The four primary actions are `QAction`s
owned by the window itself, not by the toolbar, which is what keeps their
shortcuts alive when the toolbar reorganises. Their enablement and disabled
reasons come from the core's command projections; a widget never decides
navigation legality locally, and a disabled action carries its exact reason in
both tooltip and status tip.

File navigation resolves through the retained accepted Files order and the
existing workbench host selection path. The application supplies the core only
the bounded cursor facts of the exact accepted diff and semantic selection;
the core projects change-command legality. A toolbar change gesture then calls
the comparison surface's narrow `navigate_change(direction, policy)` bridge,
which uses its existing semantic index and the exact projected stop-versus-wrap
policy. The comparison surface has no second Previous/Next presenter, and Qt
never invents enabled state.

| Command | Shortcut |
|---|---|
| Previous file | `Alt+Up` |
| Next file | `Alt+Down` |
| Previous review change | `Alt+PgUp` |
| Next review change | `Alt+PgDown` |
| Toggle files drawer | `Alt+Shift+1` |
| Toggle conversations drawer | `Alt+Shift+2` |
| Toggle review details | `Alt+Shift+3` |
| Open private review progress | `Alt+Shift+4` |

The drawer toggles use `Alt+Shift` rather than plain `Alt+1`/`Alt+2` because
the composed epoch control already holds `Alt+1` through `Alt+3` with
widget-with-children context. Both bindings match whenever that control has
focus, and Qt resolves an ambiguous shortcut by triggering neither, so the
plain-`Alt` form produced two silently dead toggles. No key sequence may be
duplicated anywhere in the assembled window.

The row contains one presenter for each semantic action at every supported
width. Review details is a compact disclosure. Seen, Reviewed, Accepted, and
Excluded remain the same receipt `QAction` instances and appear in one
keyboard-operable private-progress popup; the popup adds no duplicate policy,
shortcut, or persistent text width. Closing the popup leaves the actions owned
by the window, so their projected state and receipt semantics remain intact.

## Epoch selection

Each upper pane has its own selector. The default semantic comparison is
`Whole PR`: the latest live epoch's exact `Branch base` anchor on the left and
exact head (`Latest`) anchor on the right. Full OIDs remain non-elided and
copyable.

Named comparisons resolve as one operation:

- `Whole PR` selects the latest live epoch's exact base and head;
- `Previous upload -> latest` selects the nearest earlier live epoch with a
  different head and the latest live head;
- `My last reviewed -> latest` selects the exact newest chronologically
  ordered, non-pending current-viewer review commit and the latest live head.

Alias resolution chooses that semantic candidate before checking object
availability. If the exact previous upload or newest review is pruned,
cache-deleted, corrupt, or never observed, the alias remains visibly
unavailable with its reason; it never falls back to an older resident
candidate. A named selection re-resolves only when accepted review facts or
the catalog revision changes.

An explicit pair remains on its exact anchor IDs and OIDs. Any two resident
anchors in the same PR authority and catalog incarnation are legal, including
reverse and identical pairs; identical OIDs represent an empty comparison.
Changing one side of a resolved alias converts the selection to an exact pair.
Cross-authority, cross-incarnation, mixed-object-format, or unavailable pairs
are rejected with a visible reason rather than remapped.

Epoch rows show ordinal, availability, and both the primary label and
independent base/head transition facts. A simultaneous base/head change keeps
both facts, and unknown lineage is shown as `Head change; lineage unavailable`
rather than guessed as an upload or force-push. Append, availability, and
selection changes update stable rows incrementally without a routine model
reset.

The selectors retain one bounded page at a time: at most 100 epochs and 200
base/head anchor rows, independent of the durable catalog size. `Load earlier
epochs` follows only the exact continuation authorized by the core, while
`Return to newest epochs` requests the newest page without a cursor. Replacing
a page uses stable-ID row signals, not a model reset. If the exact selected pair
leaves the resident page, both selector indices become nonresident while the
complete selected OIDs, availability, comparison identity, and accessible
explanation remain visible. No pinned or synthetic epoch row is created.

A live catalog revision does not mix a newly accepted epoch into an older
resident page. The visible page is retained as read-only and identified as
stale; earlier navigation and selector gestures are disabled until the
core-authorized newest request succeeds against the new revision.

The interim loaded pull-request surface now hosts this reusable control. Each
selector lists the exact base and head anchor supplied for a stable epoch, and
read-only text fields expose the complete selected SHA-1 or SHA-256 OIDs for
keyboard or pointer copying. A distinct review anchor selected by `My last
reviewed` remains visible through its exact OID without being presented as a
fabricated epoch. Loading, retained refresh, empty, failed, canceled, pruned,
cache-deleted, corrupt, unavailable, and never-observed states use text as well
as enabled state; color is not their only distinction.

A single secondary `Local history maintenance` menu contains prune selected
epochs, delete repository cache, and cancel maintenance. The core controls
enablement, and prune stays disabled unless the accepted pair holds an epoch
anchor that can actually enter a prune operation, so an already pruned,
cache-deleted, or never-observed selection is never offered a destructive
confirmation it would only fail. A disabled command carries its exact core
reason in the tooltip and status tip rather than only losing its enabled
state. Destructive commands open keyboard-accessible, window-modal
warnings with Cancel as the default and Escape action; the exact stable target
is captured before the dialog opens and revalidated after confirmation. The
dialogs state the retained-history or repository-cache consequence and that
neither GitHub nor a developer worktree is modified.

This RFC-BPR-0003 control is the reusable semantic and accessibility surface,
not final visual acceptance. RFC-BPR-0005 still owns the integrated workbench
presentation-polish and native visual-acceptance slice. RFC-BPR-0006 owns
selectable themes, fonts, syntax presentation, and high-contrast profiles.

## Comparison surface

- Upper panes show both source documents in natural layout with no synthetic
  alignment blanks or invented source rows.
- One retained comparison-space position drives both pane origins and one
  shared vertical scrollbar. Wheel/trackpad input over either pane or the
  center divider, Page/Home/End, scrollbar drag, and programmatic reveal all
  route through that controller; horizontal scrolling remains pane-local.
- The center `DiffLinkMap` paints filled monotonic interval ribbons and
  insertion/deletion wedges attached to actual pane boundaries. Moved-code
  metadata uses markers or a temporary overlay, never permanent crossing
  ordinary ribbons.
- A one-sided block pins the empty side at the synchronization rail while the
  populated side advances. Unequal replacements advance proportionally and
  meet at one segment endpoint; reverse input retraces without accumulated
  drift or an input-receiver handoff jump.
- Selecting a line, hunk, ribbon, comment, or defect synchronizes all relevant
  surfaces through the one semantic alignment model.
- The lower pane is a full-width line-over-line detail view for the selected change group.
- Comments are shown beside code in the right drawer, not inserted into the source layout.

The live GitHub window now composes this selected-file comparison surface with
the production natural-layout panes, link map, shared controller/scrollbar,
semantic previous/next selection reached through the single top-row presenter,
source-line highlight, and lower detail.
Loading, failure, file/epoch/options replacement, authority invalidation, and
whole-cache deletion clear stale geometry by exact request context. Font,
style, palette, screen, DPI, viewport, and divider changes remeasure Qt-local
geometry while retaining valid semantic selection.

A metadata-only accepted pull-request refresh preserves an exact selected diff
and semantic selection when every authority and anchor-driving revision fact
is unchanged. A changed head, base, branch, origin, identity generation,
current-review candidate, file, options, or comparison clears it before late
geometry can appear. Loading, ready, failure, invalidation, and semantic
selection changes expose one combined polite accessible announcement per
presentation change; consecutive identical statuses are suppressed.

This defines functional and rendered-composition acceptance for
RFC-BPR-0004, not the final visual hierarchy. BPR-0083 retains the final-source
public-composition replay and target-native System-theme transition.
RFC-BPR-0005 retains the planned integrated presentation-polish and native
visual-acceptance pass. Target-native System-theme evidence must use the real
platform style and a full selected composition; forced offscreen palettes
remain layout evidence only.

The complete normative interaction is in
[Diff Link Map and Alignment-Space Scrolling](diff-link-map-and-alignment-space-scrolling.md).

## File drawer

Filters include All, Unread, Changed Since Review, Changed Since Accepted, Defects, Needs Verification, and Excluded. Status is represented by label/icon as well as color.

Both drawers are independently collapsible and resizable. Collapsing hides the
panel rather than resizing it to zero, because a splitter distributes a hidden
child's space among its remaining children; that is what actually returns the
width to the centre. Reopening restores the persisted width, and a drawer
refuses to be dragged below the width floor the host is willing to persist.

Ownership is split by meaning. Collapsed and expanded state is semantic, so the
core owns it and a toggle emits a core intent. Pixel widths carry no semantics,
so the host owns them and a drag emits the host's persistence path. Neither
meaning has a second carrier. Drawer contents update through incremental model
signals; a bounded row or state change never resets the model.

## Conversation drawer

Filters include New, Unresolved, Defects, Needs Verification, Pending Drafts, Outdated, and All. Selecting a conversation navigates to current or retained historical code context.

## Command placement

Review submission actions remain visible in a persistent action region but do not displace file/change navigation. Private receipt progress stays available through the compact keyboard-operable popup beside Review details. Destructive or shared mutations require clear target and state presentation.

## Empty, loading, and failure states

Every pane has explicit loading, canceled, permission-denied,
unsupported-capability, unavailable-revision, binary, oversized,
authorization-required cache, and retry states. Cached private content never
opens for an unauthenticated or different account. No spinner may conceal a
permanent error.
