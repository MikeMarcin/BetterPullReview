---
schema: "implement-rfc.card.v1"
id: "BPR-1210"
title: "Private notes are written in the Notes tab, and notes and AI replies are promoted to drafts in Comments"
status: "In Progress"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1133", "BPR-1134"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner direction on BPR-1204's tab, 2026-09-25, in chat, relayed by the coordinator", "criteria": ["AC-09"], "related_rfcs": ["RFC-BPR-0007", "RFC-BPR-0012", "RFC-BPR-0023", "RFC-BPR-0048"], "related_cards": ["BPR-1204", "BPR-1133", "BPR-1207", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1208--BPR-1210--a47a9558dfa6.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1210: Private notes are written in the Notes tab, and notes and AI replies are promoted to drafts in Comments

## Owner direction

From the owner's direction of 2026-09-25 on the tray's second tab, recorded
verbatim on [[BPR-1204]]:

> The "Findings" tab becomes Local or Notes or Personal. It's the place for your non-public notes and AI conversations/review feedback. You can promote things from there to draft in the Comments / Public tab to publish them.

## What the existing flow already covers

- **AI review feedback is covered.** RFC-BPR-0012's finding queue already
  offers "Convert to local draft comment", "Convert to local draft defect"
  and "Add to private notes". A converted finding stays a local draft until
  the explicit Publish ([RFC-BPR-0012](../../rfcs/RFC-BPR-0012-ai-review-chat-findings-mcp.md),
  "Converting a finding creates only a local draft until explicit
  publication"). This card reuses that flow unchanged.
- **Notes exist only in storage.** RFC-BPR-0007's private ledger stores notes
  (`local_note_record`), each attached to the whole review or to one file
  comparison, with revision-checked `store_note` and `erase_note`
  ([RFC-BPR-0007](../../rfcs/RFC-BPR-0007-local-review-ledger-actionability.md)).
  In the product, a note is created only by a finding's "Add to private
  notes". The agent panel lists notes only as "Private note #N", and the
  Overview shows a file's note in its row. No surface lets the reviewer write
  or edit a note or see the pull request's notes together, and nothing
  promotes one to a draft.
- **AI conversations are not covered.** The side chat has no path from a
  reply to a draft.
- **RFC-BPR-0023 stays Deferred.** Its durable private threads, provenance,
  and multi-item Squash to Draft are a later RFC
  ([RFC-BPR-0023](../../rfcs/RFC-BPR-0023-private-review-threads-provenance-comment-composition.md)).
  RFC-BPR-0048 ("Defects and private notes") expects promotion at a
  private-note seam without implementing RFC-BPR-0023 as a hidden
  prerequisite
  ([RFC-BPR-0048](../../rfcs/RFC-BPR-0048-feedback-authoring-native-github-integration.md)).

So promotion is partly uncovered, and this card adds the missing half: writing
notes in the tab, and promoting one note or one AI reply to a draft.

## Decisions made under the owner's delegation (2026-09-25)

The coordinator relayed that the owner delegates every open choice in this
batch. Each decision below is the implementer's. Changing one later takes a
new owner decision.

- **Notes use the existing ledger.** A note is attached to the pull request or
  to the current file's comparison, as `local_note_record` already allows. No
  new storage capability, schema or line anchor is introduced. Rationale: the
  storage is built and Auto-Verified, and a line anchor would be a storage
  change the milestone's boundary excludes.
- **Promotion takes Comment's target.** Promote to draft starts a draft
  comment at the target Comment names: the current line, or a reply to the
  selected thread ([[BPR-1133]], with [[BPR-1207]]'s rule when there is no
  target). The item's text becomes the draft's body. Rationale: one targeting
  rule for every way a comment starts, and a whole-review or whole-file note
  has no line of its own.
- **One item at a time, and the source stays.** Promoting copies one note, or
  one side-chat reply the reviewer selects, into a draft. The note or reply
  is kept. Rationale: multi-item composition with lineage is RFC-BPR-0023's
  Squash to Draft, which stays Deferred.

## Acceptance

- AC1 The tray's AI tab (Notes, [[BPR-1204]]) lists this pull request's
  private notes from the existing ledger, newest first. The list includes the
  notes added from findings. Each note says whether it is on the pull request
  or on a file, and names the file when it is on one.
- AC2 The reviewer writes a note on the pull request or on the current file,
  edits it and deletes it. Each change goes through the existing ledger port
  with its revision rules. A refused write keeps the reviewer's text and says
  why. No new storage capability, schema or network path is added.
- AC3 "Promote to draft" on a note, or on a side-chat reply the reviewer
  selects, creates a local draft comment at Comment's target, with the item's
  text as its body. It opens that draft in Comments ready to edit, and the
  note or reply stays where it was. The draft is published only through the
  existing explicit Publish, and there is no GitHub mutation before it. Where
  Comment has no target, Promote to draft is unavailable and says why.
- AC4 A finding promotes through RFC-BPR-0012's existing actions, unchanged,
  and those actions' courses pass.
- AC5 Notes stay private. A note is included in an agent request only when
  the reviewer selects it (RFC-BPR-0012). Promotion copies no private metadata
  into the draft's body.
- AC6 The core decides the draft command from the item and the target, the
  host persists notes, and Qt presents. Core and host tests cover the
  promotion and the note writes. A composed course writes a note, promotes it,
  and shows the draft in Comments with zero GitHub mutation. A negative
  control, promotion that publishes or that removes the note, fails.
- AC7 On the fixture, the owner writes a note and promotes it to a draft
  without publishing, then judges it on macOS with one question: does
  promoting from Notes to a draft in Comments work the way you expect?

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-09 as the owner amended it on 2026-09-25. It does not activate
RFC-BPR-0023. The core decides, the host performs, Qt presents. No
publication, authority, provider or Git capability changes. Notes are written
only through RFC-BPR-0007's existing private ledger port. Windows and Linux
station runs follow the owed commands on [[BPR-1135]].

## Engineering decisions (2026-09-26)

- Reuse `review_ledger_host` persistence, erase, private observation and rejected-note
  dismissal. The existing bounded synchronous ledger work on the decision lane is
  inherited debt tracked by [[BPR-1211]], not a new nonblocking guarantee.
- Share the existing private-context observation and retain one path index per
  accepted snapshot. File notes name the current or previous snapshot path; when
  that identity is absent from both, say "File comparison from an earlier update".
  Rationale: the existing ledger stores identity, not historical path text, and this
  card must not introduce a schema or hidden filesystem lookup. This historical
  naming limit remains explicit for AC1.
- Sort by descending creation identity because the ledger has no timestamp; editing
  does not turn an old note into a new note. Scope generations and exact revisions
  guard gestures. Refreshes retain unsaved text; explicit Discard reloads the
  accepted record and clears the host's rejected candidate.
- One validated chat delivery becomes one selectable reply containing its finding
  claims separated by blank lines, with no private metadata. Keep at most sixteen
  replies with session-scoped, nonrecycled identities; clearing or replacing the
  session clears them. Rationale: this enables one-item promotion without inventing
  RFC-BPR-0023's durable transcript or provenance model. Replies beyond the existing
  draft body bound remain readable with promotion disabled.
- Own the copied source text before invoking Comment, since that gesture may refresh
  the notes projection. The core constructs the exact revisioned draft edit; the
  existing host persists it. Neither source deletion nor publication is part of
  promotion. Finding conversion and explicit agent private-context opt-in remain
  unchanged.

### Composition source review and pin reseal

Reviewed every `github_application.cpp` change since `44bd1266` (the last
matching pin); there is no intervening commit in the pinned scope. The new
projection shares the existing private observation and caches accepted paths;
retirement clears its scope and retained rows. CRUD uses only the owned ledger
host. Promotion owns its text before Comment can refresh the vector, rechecks
scope and anchor, uses the core's revisioned edit and retains both source and
explicit Publish. Reply selection checks the host session and reply identity.
The mutation branch hash stays
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
The full application and manifest pins are resealed in this same product slice;
the composed course rejects a removed source row and publication evidence.

### Focused verification and remaining acceptance

The initial composed batch passes 5/5 (40.47 seconds): private Notes, side chat,
real chat reply promotion, unchanged finding conversions and review composition.
The final Notes batch passes 4/4 (40.15 seconds): panel plus composed application
at 100/125/200 percent. It writes PR/file notes, rejects and retains an oversized
candidate, explicitly discards/reloads, edits/deletes at the accepted revision,
promotes one note, checks zero provider mutation, rejects removed-source and
implicit-publication negative controls, and reopens SQLite to recover the source.
All three Notes renders were inspected. Core/host focused tests pass 3/3; the
final deterministic core-only promotion test and composition pin pass. The
core-only boundary contract and architecture boundary contract pass; the
architecture source gate reproduces the preexisting MCP Windows containment
ownership failure in `mcp_terminal_diagnostic.hpp` (also seen on the base).
This is not recorded as a successful architecture gate. Documentation and
changed-path formatting pass. Logs are under
`/Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1210-*`.

**AC1 remains pending for one engineering case:** a file note whose identity is
absent from both current and previous accepted paths cannot name that historical
file with the existing retained observation. Current and renamed file cases pass;
"File comparison from an earlier update" does not satisfy the remaining naming
requirement. A follow-up must expose an existing accepted historical path without
hidden effects, or explicitly authorize the required storage design. Owner AC7
cannot waive that engineering gap. AC7 itself and native station evidence remain
pending. RFC-BPR-0023 remains Deferred.
