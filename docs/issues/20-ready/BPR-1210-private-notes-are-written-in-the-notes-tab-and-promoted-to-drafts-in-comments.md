---
schema: "implement-rfc.card.v1"
id: "BPR-1210"
title: "Private notes are written in the Notes tab, and notes and AI replies are promoted to drafts in Comments"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1133","BPR-1134"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner direction on BPR-1204's tab, 2026-09-25, in chat, relayed by the coordinator","criteria":["AC-09"],"related_rfcs":["RFC-BPR-0007","RFC-BPR-0012","RFC-BPR-0023","RFC-BPR-0048"],"related_cards":["BPR-1204","BPR-1133","BPR-1207","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1208--BPR-1210--a47a9558dfa6.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1210-promote","criterion":"AC7","fingerprint":"4e3aacca34215c53cb9d1479a2fff99c9015135d47d81639f7359f174f1ad44a","decision_id":"8386d8e9-09d6-4330-8560-f6f41a3fe6d1","outcome":"accept","note":"","saved_at":"2026-09-26T20:52:42.196584+00:00","actor":"human"}]}}
acceptance: [{"criterion":"AC1","outcome":"pending","revision":"fb73c462d028fd368d664231ba2705d0e8d0c592","evidence":{"kind":"observation","detail":"Engineering gap: historical file identity absent from current/previous accepted paths has no filename; current/renamed file cases pass. Owner acceptance cannot waive this."}},{"criterion":"AC2","outcome":"pass","revision":"fb73c462d028fd368d664231ba2705d0e8d0c592","evidence":{"kind":"command","detail":"Product5f7bf9d1: core/host3/3, initialQt5/5, finalNotesQt+composition100/125/2004/4, SQLite source persistence and explicit source-removal/publication negative controls, inspected renders, source pin and documentation pass. Logs /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1210-*. Main architecture repair 413eabf3 adopted; focused core-only architecture replay passes 2/2 in 109.26 seconds, recorded in 1210-repaired-architecture.log. Final Notes integration fb73c462 passes 9/9 in 99.26 seconds, including real Notes writing/promotion and continued chat; inspected final Notes renders. Historical-name AC1 and owner AC7 remain pending."}},{"criterion":"AC3","outcome":"pass","revision":"fb73c462d028fd368d664231ba2705d0e8d0c592","evidence":{"kind":"command","detail":"Product5f7bf9d1: core/host3/3, initialQt5/5, finalNotesQt+composition100/125/2004/4, SQLite source persistence and explicit source-removal/publication negative controls, inspected renders, source pin and documentation pass. Logs /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1210-*. Main architecture repair 413eabf3 adopted; focused core-only architecture replay passes 2/2 in 109.26 seconds, recorded in 1210-repaired-architecture.log. Final Notes integration fb73c462 passes 9/9 in 99.26 seconds, including real Notes writing/promotion and continued chat; inspected final Notes renders. Historical-name AC1 and owner AC7 remain pending."}},{"criterion":"AC4","outcome":"pass","revision":"fb73c462d028fd368d664231ba2705d0e8d0c592","evidence":{"kind":"command","detail":"Product5f7bf9d1: core/host3/3, initialQt5/5, finalNotesQt+composition100/125/2004/4, SQLite source persistence and explicit source-removal/publication negative controls, inspected renders, source pin and documentation pass. Logs /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1210-*. Main architecture repair 413eabf3 adopted; focused core-only architecture replay passes 2/2 in 109.26 seconds, recorded in 1210-repaired-architecture.log. Final Notes integration fb73c462 passes 9/9 in 99.26 seconds, including real Notes writing/promotion and continued chat; inspected final Notes renders. Historical-name AC1 and owner AC7 remain pending."}},{"criterion":"AC5","outcome":"pass","revision":"fb73c462d028fd368d664231ba2705d0e8d0c592","evidence":{"kind":"command","detail":"Product5f7bf9d1: core/host3/3, initialQt5/5, finalNotesQt+composition100/125/2004/4, SQLite source persistence and explicit source-removal/publication negative controls, inspected renders, source pin and documentation pass. Logs /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1210-*. Main architecture repair 413eabf3 adopted; focused core-only architecture replay passes 2/2 in 109.26 seconds, recorded in 1210-repaired-architecture.log. Final Notes integration fb73c462 passes 9/9 in 99.26 seconds, including real Notes writing/promotion and continued chat; inspected final Notes renders. Historical-name AC1 and owner AC7 remain pending."}},{"criterion":"AC6","outcome":"pass","revision":"fb73c462d028fd368d664231ba2705d0e8d0c592","evidence":{"kind":"command","detail":"Product5f7bf9d1: core/host3/3, initialQt5/5, finalNotesQt+composition100/125/2004/4, SQLite source persistence and explicit source-removal/publication negative controls, inspected renders, source pin and documentation pass. Logs /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1210-*. Main architecture repair 413eabf3 adopted; focused core-only architecture replay passes 2/2 in 109.26 seconds, recorded in 1210-repaired-architecture.log. Final Notes integration fb73c462 passes 9/9 in 99.26 seconds, including real Notes writing/promotion and continued chat; inspected final Notes renders. Historical-name AC1 and owner AC7 remain pending."}},{"criterion":"AC7","outcome":"pass","revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","evidence":{"kind":"observation","detail":"Owner hand-back 49, session d7cf21b16656c6ec19c8787255aa4006; BPR-1210-promote accept 8386d8e9-09d6-4330-8560-f6f41a3fe6d1. Exact fingerprints and notes retained in metadata. Verdict applies only to the frozen candidate."}}]
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
final deterministic core-only promotion test and composition pin pass. The initial architecture source gate reproduced the old-base MCP Windows
containment ownership failure in `mcp_terminal_diagnostic.hpp`. Main had already
repaired it in `ca23a9fe`; adoption `413eabf3` and focused core-only architecture
replay pass 2/2 in 109.26 seconds (`1210-repaired-architecture.log`). The final
non-Qt architecture/core/ledger/chat batch passes 4/4 in 7.08 seconds. The earlier
failure is superseded by that verified repair, not counted as a pass. Documentation and
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

## Owner hand-back 49 — 2026-09-26

The owner reviewed frozen source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
in session `d7cf21b16656c6ec19c8787255aa4006`. Full decision identities and
fingerprints are retained in the card metadata.

- **AC7 — Accept**, candidate `BPR-1210-promote`, decision `8386d8e9-09d6-4330-8560-f6f41a3fe6d1`. No note.

AC7 passes; AC1 historical file naming remains **engineering pending**. An
attachment retains only opaque file/comparison identities. The cached overview
replaces its previous catalogue, and restored tabs retain identities rather than
paths. None supplies a missing historical name after restart. A fabricated name
or an unbounded Git scan cannot satisfy AC1. Its storage boundary remains intact.

## Historical filename investigation after hand-back32

The 2026-09-26 continuation checked the retained source, not only the current UI.
`local_note_attachment` in `review_ledger_port.hpp` contains opaque file and
comparison identities and explicitly stores no path. The window's accepted
current/previous-path index is cleared at composition retirement. The persisted
GitHub overview is the latest catalogue for the PR, not a revision-indexed path
history, and restored document tabs retain identities. The Git cache's blob and
rename ports require an already known path; they cannot recover a missing name
from the note's opaque identities. No existing accepted observation supplies the
old path after a restart once both catalogues have dropped it.

AC1 therefore remains engineering pending, independently of the accepted AC7.
The concrete prerequisite is a bounded retained-path contract: preserve the
actual accepted attachment path when a file note is written, with a durable
read path, revision/identity binding and migration rules for existing nameless
notes. That requires an explicitly scoped ledger schema/contract amendment,
which this card's no-new-schema boundary excludes. Existing nameless notes must
remain honestly unnamed until trusted history is available; neither guessed
basenames nor hidden Git/filesystem scans are acceptable recovery.

Decision under the owner's delegated design authority: track that prerequisite
as `historical-note-path-contract` and leave the card Blocked. Rationale: the
retained bytes cannot reconstruct information that was never stored. This does
not reopen AC7, request another owner judgment, or mark AC1 satisfied. BPR-1204
keeps its terminal dependency on this card; native station debt stays BPR-1135.

## Owner decision — 2026-10-06

The integrator put one question to the owner in chat: *"A private note on a
file keeps only an internal ID, not the file's name. If a later push drops that
file from the pull request, the note can only say 'File comparison from an
earlier update'. Should file notes also remember the file's path when you write
them (a small local storage change; notes written before it stay unnamed), so
they keep their file name?"* The answer, verbatim: "Accept (Recommended)".
That lifts `historical-note-path-contract`. RFC-BPR-0007 records the
amendment.

## Linux implementation — 2026-10-06 (`bpr-main-linux`)

- **Port:** `local_note_record` gains `FilePath`, the file's path when the
  note was written. `valid()` allows it only with an attachment, non-empty,
  without NUL and at most `maximum_note_path_bytes` (4,096).
- **Ledger schema 7 → 8:** one new table, `private_note_path(scope_id,
  thread_id, path)`, with its primary key and a foreign key to
  `private_thread`. No row is the null path. It is a side table rather than a
  column on `private_thread`, so the migration creates one table instead of
  rebuilding a table that others reference by foreign key, inside the one
  migration transaction. The meaning is the same as a nullable column.
  - The migration creates it empty, so v7 notes stay unnamed.
  - Canonical schema validation adds it at v8, and preflight still validates
    v7.
  - A note is written with its path. An edit without one keeps the stored
    path.
  - Deleting a note's thread deletes its path row first.
  - The v6 legacy fixture drops the table, and a new `restore_v7_fixture`
    builds an exact v7 ledger.
- **App:** a new file note takes the selected comparison's
  `PresentationPath`. The note's label is its file's current or previous path
  from the accepted snapshot; failing that, the kept path; failing that,
  "File comparison from an earlier update".
- **Purge:** `review-ledger.sqlite3` and its sidecars are in the profile purge
  store list (BPR-1283's inventory), so the path data is removed with the rest
  of the ledger.

**Tests:**

- `test_note_paths_migrate_and_round_trip` (`sqlite_review_ledger_storage`):
  a v7 ledger with a file note and a whole-review note migrates to schema 8
  with an empty path table. Both notes keep text, revision and attachment,
  with no path. A new file note is written with its path, and an edit without
  one keeps it. A path without a file, or longer than its bound, is refused.
  The path survives restart, and deleting the note deletes its path.
- `application_review_submission_composition_public_path`: the real window's
  file note puts exactly one row in `private_note_path`, the path its label
  shows. The pull-request note has none.

**Failing-before controls:** with the store not writing the path,
`sqlite_review_ledger_storage` fails at "an edit keeps the path kept when the
note was written". With the app not recording it, the composed course fails at
its path check. Both pass again restored.

**Pin:** `application_review_round_trip_composition_contract` is resealed
(full application and production manifest digests; mutation pin unchanged).
No pinned-scope commit intervened since the BPR-1289 reseal.

**Evidence** (`build/dev`, GCC 15.2): every `sqlite_` row, the ledger,
private-review, Notes, agent-launch, review-submission, composition and
release-maintenance rows, and both architecture rows pass 72/72 in 817 s.
`coding-format-check` and `DOCUMENTATION_OK` pass.
