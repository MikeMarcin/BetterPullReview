# Same-machine handoff protocol

This is a narrow extension to profile ownership, not a general BPR RPC platform. The runtime continues to own policy and storage; local transport only carries finite commands and observations.

## Ownership cases

| Situation | Required behavior |
|---|---|
| No profile owner | CLI acquires existing native lease, constructs headless services, performs/commits the bounded command, then closes/drains/releases. |
| Desktop owns the profile | CLI authenticates to the owner's advertised tour command interface and forwards without opening the store. |
| External editor or agent is thinking | No standalone CLI runtime lease is held for the waiting interval; a captured revision/ticket can later re-enter the current owner. |
| Desktop launches during a short command | Bounded wait or explicit transient busy; no competing runtime. The draft remains durable for next open. |
| Older owner has activation only | `owner_protocol_unsupported` with an actionable update/reopen message. Never misinterpret activation frames or bypass the lease. |
| Owner disappears during request | Uncertain delivery; reacquire only through the existing admission path and inspect durable operation identity before replay. |

Free movement does not mean conflict-free simultaneous coauthoring. Normal desktop-open ↔ CLI/skill use is supported without closing the desktop. Two concurrent writers to one field still need revision conflict handling.

## Request/response

The owner channel uses a versioned local frame with a bounded payload, OS peer authentication, profile/owner-instance binding, request identity, finite tour command kind, exact draft identity, required record revisions and digest. Ordinary user-readable paths are not authorities. Use existing canonical profile-root validation and platform channel primitives, with independent rejection tests for symlink/endpoint replacement, wrong users/profiles, unsupported versions, oversized frames and command confusion.

The supplied activation channel accepts a fixed small activation message. Implement an explicit separate protocol/endpoint or versioned negotiated message family; do not expand a fixed array blindly or route arbitrary caller JSON into the provider. Owner responses use the CLI command result semantics, tagged with the actual owner instance, revision and completeness. The CLI controls its local editor/pager; the desktop owner does not launch arbitrary caller-selected executables.

Only tour commands are admitted. Mutations to local draft content and remote GitHub writes are different command classes. Parent authorization cannot be represented by a bare `approved=true` field. For remote publication, the receiving host resolves the exact immutable plan and obtains supported live authorization in the invoking presentation; otherwise return interaction_required. IPC peer identity proves only same-user transport access. It cannot attest that an AI model is an approved corporate provider or that a human read a stop.

## Durable deduplication

Before committing a local mutation, atomically record its request ID, canonical digest and outcome with the draft mutation. Same ID/same digest returns that result; same ID/different digest refuses. Record explicit unavailable/compacted outcomes, never invent a successful deduplication after losing history. Bound ordinary completed local records separately from unresolved remote operation journals. A durable “request accepted” receipt is not an external publication receipt.

Reads may be retried only under their normal safe read policy. A lost mutation acknowledgement requires inspection by exact request ID; if ambiguous, return uncertainty rather than manufacture a new operation ID and repeat. Repeated Undo or Add is not harmless. A publication command passes through the original remote journal; never give it another idempotency story just because the transport is IPC.

## Draft revisions and buffers

For a field edit, compare field revision and captured relevant source context. For reorder, compare structure revision and stable stop IDs. A body edit on stop A need not invalidate an unrelated field B unless shared context requires it. A changed outline can invalidate a draft-wide publication preview without forcing an editor-text reset.

Before a forwarded read requiring freshest local content, the desktop commits ordinary pending text deltas. IME preedit is not committed text: return editor_busy until the user completes/cancels composition; do not flush partial characters or hang indefinitely. The owner returns only committed revisions. If a user edits after a response was captured, a subsequent edit command is stale and keeps its candidate. Interleaved display updates do not restore earlier bytes over a dirty editor.

Undo/redo operates on the shared semantic journal. A native editor may accumulate a tentative coalesced transaction, but on flush/handoff there is exactly one owner of committed history. Externally applied deltas update the editor view without replaying the same edit into another independent stack. Preserve parent selection behavior as a presentation hint, not as an excuse to move other clients' cursors. A cross-interface undo of a labelled structural edit restores order/IDs through reversible state, not deletion/recreation of published comments.

## Crash and lifecycle tests

Kill a standalone CLI before local commit, after commit/before response, during owner forwarding, after a remote send, and after reconciliation. Reopen from another interface and assert exact text/IDs, request outcome and independent remote-send counts. End the desktop during a CLI external-editor session and start it again before save; the save must route to the current owner with its original baseline. No PID-age heuristic, lock stealing, broad process kill or queue-drop cleanup is permitted.

No boot/login service is added. Idle waiting in a skill is just its normal agent session. A short-lived CLI invocation drains workers and releases resources; desktop owner services exist only while the existing application runtime owns the profile. Compatibility/permission errors must not automatically start the GUI or ask the model to work directly on SQLite.
