# Workbench Specification

## Accepted redesign awaiting implementation

The daily-workbench design is accepted and registered, but not implemented by
the registration. Its live contracts are
[Settings and direct sign-in](../rfcs/RFC-BPR-0018-robust-settings-center.md),
[BPR profiles and independent windows](../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md),
[review drawers and browser-style tabs](../rfcs/RFC-BPR-0025-review-workspace-navigation-browser-tabs.md),
and [personal Home and PR URL navigation](../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md).
Backlog entry/closeout cards are [[BPR-0592]] through [[BPR-0599]]. The sections
below retain their existing scope; this cross-reference does not claim that
profiles, multiple windows, tabs, or personal discovery are already shipped.

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

## Public preview

On enabled macOS, Public preview beside Connect activates the selected native
GitHub.com profile without sign-in. CLI selections still report unavailable. The title and session strip explicitly label the mode. The same file
drawer and exact selected-file comparison show public content. REST inline
comments appear as read-only root/reply groups, with no invented thread
resolution. Unobserved and observed-empty conversations have different wording.

Queue role, private review progress, review-thread resolution, and publishing
remain visible but disabled with explanations. A budget ring has no consumed
fraction before a budget was observed; after a read it shows the reported limit,
remaining units, and reset. Exhaustion and private-or-missing failures offer
Sign in. The Sign in gesture uses the selected profile's existing device path.
GHES refuses preview and Linux/Windows report unavailable rather than silently
trying a credentialed read. Switching mode clears stale identity-sensitive UI.

## Signing in with a device code

RFC-BPR-0016 adds a second way to enrol a profile beside the token field, and
the token field stays. `qt::device_authorization_panel` is the surface:

- A heading, two read-only fields — the user code and the address to enter it
  at — a remaining-validity label, and a browser-handoff label. The reviewer
  has to read the code aloud or type it elsewhere, so both fields are
  selectable text rather than a status line.
- Four actions: Get a code, Copy code, Open browser, and Cancel. Get a code
  becomes Get a new code once the flow has left `idle`, because offering
  "Get a code" after a denial reads as though nothing happened.
- Get a code and Cancel are enabled from the core's command projection rather
  than from a rule the panel keeps for itself. An absent command is a disabled
  command, so a projection that lost one fails closed. Copy and Open are
  enabled from what is on screen, because neither moves the flow
  ([[BPR-0283]], `qt_device_authorization_scale_100`).
- The handoff label reports what became of the last attempt to open a browser
  and never an authorization outcome. A refused, failed, or unavailable
  handoff leaves the flow exactly as live as an opened one, with the code and
  the address still presented ([[BPR-0280]], [[BPR-0516]]).
- Every terminal, including the successful one, clears the code and the
  address. A user code that outlives its flow is a code the reviewer can still
  be tricked into typing.
- Each published change produces at most one combined polite announcement
  carrying the stage, the code, and the handoff together, under the same rule
  as the rest of this window, and the panel is rendered at 1.0x, 1.25x, and
  2.0x in the Qt harness.

The session chrome offers **Sign out** beside Connect. Device grants are
remembered across orderly quits; manual tokens remain session-only. Sign out
removes both device-token slots and disables the stored binding. Startup
restores only an exactly validated session and reports unusable retained
material through the existing diagnostic surface. These effects belong to the
application host, not the button or panel ([[BPR-0541]]).

The panel receives a projection and never a token, a refresh token, a device
code, a client identifier, or an endpoint; those fields do not exist on the
type it is handed.

**Where it is mounted.** The workbench places the panel in the review header
column, directly under the session chrome that offers the host picker and the
Connect control, because a profile has two ways in and a reviewer looking for
either looks in one place. It is bound to whichever profile that picker names
and is rebound when the picker changes, so the token prompt and the device code
can never mean two different hosts. The header column scrolls, and the panel is
below the fold at the default height ([[BPR-0526]]).

The application constructs the device host on every platform and binds its
projection port to a fan-out: every publication reaches the recording sink the
public self-test reads, and then the mounted panel. The panel's own contracts
run at the adapter boundary (`qt_device_authorization_scale_100`, `_125`,
`_200`); its placement, profile binding and gestures are proven in the composed
window (`qt_composed_github_scale_100`, `_125`, `_200`); and that beginning a
flow from the mounted control reaches the device host is proven end to end
(`application_device_authorization_panel_course`, with
`application_device_authorization_panel_absent` as its construction cut).

## Primary actions

Previous/next file and previous/next change are always visible in one compact
top row. Files is the far-left action, then Previous file, Next file, Previous
change, and Next change appear in that semantic order. Bounded review-details
and private-progress controls follow, a flexible spacer absorbs remaining
width, and Conversations is the far-right action. The six drawer/navigation
presenters are icon-only, but each retains a stable visible tooltip, accessible
name and description, strong-focus stop, and keyboard shortcut. No text toolbar
appears at a narrower width.

The four navigation arrows use the native style's silhouettes with foreground
pixels derived from the row's live active and disabled `ButtonText` roles.
Palette, font, style, and theme changes rebuild those modes. This presentation
refresh changes neither command identity nor projected enablement: disabled
arrows remain readable and visually distinct, while the Files, Conversations,
Review details, and private-progress symbols retain their separate native
icons.

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
| Toggle source line numbers | `Alt+Shift+5` |

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
- Each pane numbers its own lines in a gutter at its left edge, on by default.
  The numbers are one-based on screen while every internal coordinate stays
  zero-based, and a pane is never renumbered to agree with the other side — so
  past a one-sided block the same screen row legitimately carries different
  numbers in each pane. The gutter is sized to the document's digit count
  rather than to what is on screen, because a column that grew while scrolling
  would shift the code sideways under the reader. Its colours are the
  `code_gutter_background` and `code_line_number_text` roles, which the
  appearance contrast rules already validate in every profile. The conversation
  anchor marker stays at the text viewport's inner edge, so it sits beside the
  code rather than behind the numbers.
- The numbering is a durable reader preference, reached from **View > Line
  numbers**. Turning it off returns the gutter's whole width to the code rather
  than leaving an empty margin, and turning it back on restores the exact
  geometry it had. The setting survives a restart. Its state belongs to the
  core and its action's checked mark comes from the published projection, so
  the menu entry cannot disagree with what the panes are drawing.
- One retained comparison-space position drives both pane origins and one
  shared vertical scrollbar. Wheel/trackpad input over either pane or the
  center divider, Page/Home/End, scrollbar drag, and programmatic reveal all
  route through that controller; horizontal scrolling remains pane-local.
- The center `DiffLinkMap` paints filled monotonic interval ribbons and
  insertion/deletion wedges attached to actual pane boundaries. Moved-code
  metadata uses markers or a temporary overlay, never permanent crossing
  ordinary ribbons. Its live-palette ordinary fill remains readable against
  the gutter in every built-in profile, while the selected highlight plus
  contrasting edge stays distinct. The composed capture verifies these as
  actual selected and unselected path pixels without changing endpoints.
- A one-sided block pins the empty side at the synchronization rail while the
  populated side advances. Unequal replacements advance proportionally and
  meet at one segment endpoint; reverse input retraces without accumulated
  drift or an input-receiver handoff jump.
- Selecting a line, hunk, ribbon, comment, or defect synchronizes all relevant
  surfaces through the one semantic alignment model.
- The lower pane is a full-width line-over-line detail view for the selected change group.
- Comments are shown beside code in the right drawer, not inserted into the source layout.

Conversation synchronization is exact rather than proximity-based. Selecting a
current root thread, or any reply below it, resolves the root's path, current
line, and GitHub diff side. When the selected comparison proves that endpoint's
revision identity, Qt converts the provider's one-based line to the
comparison's zero-based source index and uses the same semantic line-selection
transition as a source-pane gesture. The shared comparison controller reveals
that line in the correct natural-layout pane. A narrow four-pixel mark at the
pane's inner margin distinguishes the conversation anchor from the ordinary
selected-line wash; the comment body remains in the drawer and creates no
source row.

The reverse path uses the same identity. A reviewer-selected source line filters
the conversation drawer to roots on the exact current file, side, and line;
replies remain attached to their root. A group, connector, or file-level
selection clears the line filter and selected conversation with its marker
while retaining the current-file scope.
Automatic selection created while a diff is first published does not pretend
the reviewer selected that line.

Only a current anchor whose endpoint identity matches the selected comparison
is placeable. The composed GitHub path proves the right endpoint by matching
the selected right commit to the accepted `headRefOid`. It leaves `LEFT`
unplaced because GitHub's pull-request diff uses a merge-base coordinate and the
current provider snapshot does not carry that merge-base identity. Outdated,
file-level, endpoint-mismatched, and selected-comparison-out-of-bounds roots stay
visible at file scope with an explicit reason.
`OriginalLine` is retained as history but is never treated as a current
comparison coordinate, and Qt never guesses a side or revision. Missing or null
provider side and a line thread with neither current nor original line are
malformed provider responses rejected before this presentation boundary;
defensive Qt fixtures still prove their unplaced rendering.

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
meaning has a second carrier. Source-pane line-number visibility is split the
same way and for the same reason: the core owns it, the View menu emits an
intent, and the panes draw what the projection says. Drawer contents update
through incremental model signals; a bounded row or state change never resets
the model.

## Menu bar

The window carries a menu bar with one menu, **View**, holding the line-number
toggle. It was added by BPR-0457 because the owner asked for a "view menu
option" and, offered the choice between a real menu bar and a checkable entry
in the existing toolbar popup, chose the menu bar. On macOS it merges into the
system menu bar and costs the window no height; on Windows and Linux it takes a
row, which is the price of the same answer being true on every platform. The
existing toolbar popup keeps the private progress actions it already had.

The entry is disabled until a pull request is open, because line-number
visibility is workbench-surface state and an inactive workbench disables every
workbench command. A reader who opens **View** on a freshly launched window
sees the entry greyed.

## Conversation drawer

Replies read as a linear feed, not a staircase. A reply sits one step in from
the root it belongs to, and a reply to a reply sits in the same place — the
grouping by root stays visible, SmartBear Collaborator style, while depth stops
spending the width the comment text needs. Depth is still carried by the model
and is read on the thread map above, which is the product owner's own stated
reason for accepting a flat feed here.

Filters include New, Unresolved, Defects, Needs Verification, Pending Drafts,
Outdated, and All. The current comparison filters roots to the exact file. A
reviewer-selected source line narrows that forest to the exact diff side and
line; selecting a root or reply drives the reciprocal code reveal described
above. Unplaceable threads remain readable at file scope and state why no code
marker is drawn. Retained historical navigation requires a revision identity
and remains separate work; an outdated thread is never placed against the
current panes by reusing its original line.

### The thread map

Above the tree, the drawer draws the same forest as a map of its reply
structure. It is the Treeverse-style view the product owner chose on
2026-08-21, and it exists because a reply chain deep enough to matter stops
being followable as an indented list — which is the case the linear
presentation loses, not the ordinary one.

Nodes are boxes labelled with the author of a comment, or with the anchored
file and line for a thread's root, laid out one row per depth. Edges run
parent to child. Slots within a depth are assigned in pre-order, so within one
row every node of an earlier subtree precedes every node of a later one and no
two edges can cross. A box is sized to its own label, so no author name is
elided into an ambiguous prefix.

Selecting a node highlights **the whole branch it belongs to**, in two weights.
The root-to-selection path is drawn heaviest: that is the floor the owner set
— "it still needs to show the full branch that you clicked on, or at least up
to the node you selected". Everything downstream of the selection is drawn
lighter, so the chain's continuation is visible without competing with the
path; that lighter set is the whole subtree, sibling sub-branches included,
because all of it is downstream of the comment that was clicked. Anything
neither above nor below the selection is left plain. A caption below the map
states the path and how many replies follow.

Highlighting is not the same claim as showing. The drawer's band is short, so
selecting a node scrolls the map to it and keeps as much of the path above it
as fits; when the whole branch fits, all of it is shown. A band that changes
height re-reveals rather than leaving the selection scrolled away. Without
that, a reviewer gets a highlighted branch whose selected end is off-screen —
which is what the surface shipped as until an independent review measured it.

Selection has one owner. Pressing a node moves the drawer tree's current index,
and the code reveal, the anchor marker, and the defect surface keep reading
that selection from where they always did. The map holds no selection authority
of its own, performs no effect, and adds no projection: it is a second view of
the drawer's forest, never a second source of it.

There is deliberately **no read or unread state on the map**. The accepted
mockup legends one, and nothing in this product carries it — the review
ledger's unread notion is a per-conversation cursor rather than a per-comment
flag. Showing it would mean inventing the state or approximating it, so the
distinction is absent rather than faked.

An unexpected shape costs nesting and never a row. A parent naming a comment
that is not present, and a parent chain that never terminates, both promote the
node to a root — every node on the chain, decided in a pass that writes nothing
so the outcome cannot depend on visit order. That is the same answer the
drawer's tree gives, and the drawer shows both views at once, so a shape they
answered differently would be a disagreement the reviewer could see.

The map maps whichever threads the drawer is currently showing, so its summary
counts across them. A projection equal to the one already mapped is skipped
rather than relaid, because the drawer re-adopts on every anchor change.

## Command placement

Review submission actions remain visible in a persistent action region but do not displace file/change navigation. Private receipt progress stays available through the compact keyboard-operable popup beside Review details. Destructive or shared mutations require clear target and state presentation.

## Empty, loading, and failure states

Every pane has explicit loading, canceled, permission-denied,
unsupported-capability, unavailable-revision, binary, oversized,
authorization-required cache, and retry states. Cached private content never
opens for an unauthenticated or different account. No spinner may conceal a
permanent error.
