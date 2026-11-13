# Comments absorb notes: accepted design

Accepted by the owner on 2026-11-13 for
[RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md). One
comment is Local, Draft or Published; private notes become Local comments and
the Notes tab goes.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right; `png/` holds headless renders of its frames (not screen captures),
and `ref/` the four offscreen renders of today's build the page compares
against. Content is invented. The page's links to the draft and its inventory
point beside it; those files are kept as import provenance in
[docs/rfcs/assets/comments-absorb-notes-v1](../../rfcs/assets/comments-absorb-notes-v1.LIVE_IMPORT.md).

## The owner's decision, verbatim

> I like everything recommended except lock icon for local notes.. maybe
> QStyle::SP_DriveHDIcon works? Open to suggestions.

On the computer icon offered in reply:

> Computer icon is fine

So every recommended default (Q1 to Q14 on the RFC) is accepted, and the mark
of a Local comment is a **computer icon** (`QStyle::SP_ComputerIcon`, or an
OS-matched computer glyph) with the tooltip **"Only on this computer"**.
**Where these mockups draw a lock for Local, read the computer icon**; no
state wears a lock.
