# Workbench Specification

## Architectural interaction contract

- Navigation, epoch selection, current file/change, command legality, and review semantics are functional-core state and policy.
- GitHub/Git/ledger/diff observations and background work are supplied by the application host.
- The main window, drawers, toolbars, actions, Qt models, focus, splitters, scrolling, painting, and accessibility are Qt-presentation-adapter responsibilities.
- Every user gesture becomes a semantic intent, and every bounded state change reaches Qt as an incremental projection or delta. No workbench widget directly calls an effectful service.

## Primary layout

```text
┌ Header: product, PR, status counters, account/host ┐
├ Primary toolbar: Previous File | Next File | Previous Change | Next Change | view controls ┤
├──────── file drawer ────────┬──────── comparison workbench ────────┬──── conversation drawer ────┤
│ filters and review map       │ independent left/right epoch pickers  │ threads, defects, AI tabs   │
│                              │ upper side-by-side panes               │                              │
│                              │ clean center connector band            │                              │
│                              ├ full-width selected-change detail pane ┤                              │
└──────────────────────────────┴────────────────────────────────────────┴──────────────────────────────┘
```

The file and conversation regions are true collapsible drawers. Their open widths are independently resizable and persisted. Collapsing one returns its width to the central review surface.

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
- Repository and pull-request input, command legality, capability tri-state,
  rate-limit observations, normalized metadata, conversations, status, and
  bounded diagnostic text remain distinct accessible fields.
- Remote title, body, author, path, and comment values render as literal plain
  text. Provider records, requests, URLs, transports, vaults, credentials, and
  authentication material never enter the Qt adapter.
- Selecting another identity or opening another target immediately clears
  stale identity-sensitive content. Refresh keeps the last accepted content
  visible while a replacement is loading or if that replacement fails,
  cancels, or becomes stale.
- Conversation rows have stable semantic identities and update by insert,
  remove, minimal move, or data change without resetting the complete model.

The window remains usable through a minimum-size scroll surface, exposes
keyboard shortcuts and accessible names/descriptions, and is rendered at
1.0x, 1.25x, and 2.0x scale in the BPR-0028 harness. BPR-0031 retains
native-style and complete public-application UX certification.

## Primary actions

Previous/next file and previous/next change are always visible in the top toolbar. They operate on the current filtered/actionable set and expose keyboard shortcuts. Disabled states explain why navigation is unavailable.

`Next actionable` remains available as a secondary command and uses the local actionability projection.

## Epoch selection

Each upper pane has its own selector. The default comparison is `Branch base` on the left and `Latest` on the right. Selecting a named shortcut updates both selectors but never hides the exact OIDs.

## Comparison surface

- Upper panes show side-by-side full-file context with synchronized scrolling where line mapping permits.
- The center connector band groups changed blocks without visually confusing crossovers.
- Selecting a line, hunk, connector, comment, or defect synchronizes all relevant surfaces.
- The lower pane is a full-width line-over-line detail view for the selected change group.
- Comments are shown beside code in the right drawer, not inserted into the source layout.

## File drawer

Filters include All, Unread, Changed Since Review, Changed Since Accepted, Defects, Needs Verification, and Excluded. Status is represented by label/icon as well as color.

## Conversation drawer

Filters include New, Unresolved, Defects, Needs Verification, Pending Drafts, Outdated, and All. Selecting a conversation navigates to current or retained historical code context.

## Command placement

Review submission actions remain visible in a persistent action region but do not displace file/change navigation. Destructive or shared mutations require clear target and state presentation.

## Empty, loading, and failure states

Every pane has explicit loading, canceled, permission-denied, unsupported-capability, unavailable-revision, binary, oversized, offline-cache, and retry states. No spinner may conceal a permanent error.
