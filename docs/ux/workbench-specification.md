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
