# RFC-BPR-0109 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `author-evidence`
([[BPR-2023]], [[BPR-2024]], closeout [[BPR-2025]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Product candidate: `8f5ca195f`.
Frontend-design was loaded before the Qt surface work. Owner amendments in
force: "If you need it you need it go build it."; "main should be the source
of truth for the tracker"; "Continue working unattended, you can file reviews
but dont block on their results". Debug timings are nongating; nothing below
is a timing claim.

The cards have not moved: [[BPR-2023]] depends on [[BPR-2022]] (RFC-BPR-0108's
closeout), which waits on [[BPR-2021]]'s owner look. The engineering below
needed only the built local-draft code; the claims and finishes follow
BPR-2022.

## What was built

| Layer | What |
|---|---|
| Core (`core/draft_evidence.hpp`, `core/local_draft_evidence.hpp`) | Claims (kind, text, earlier wordings), checks (method, command as text, linked claims, formal scope), results (outcome, `reported` only, producer, material, bound candidate, inputs, environment, expected, actual, limitations, artifacts), questions, imports; accept or reject; selection; the `bpr.evidence-import` v1 parser that refuses a document whole; `bind_material`; `currency` (current, carried, needs recheck, unbound, later); `reach` per method; `evidence_for_claim` (counts, no verdict); `selected_evidence`; the stored form inside the draft's record |
| Application | `import_local_draft_evidence` (the document's folder opened as a read-only workspace root; the document and each artifact read beneath it within bounds, without following links; applied in one store write, re-read once on a lost race); workbench evidence edits, import with cancellation on close, artifact reading |
| SQLite | Artifact objects pinned to the draft, removed with it |
| Qt | `local_draft_evidence_area`: Changes and Evidence tabs (each draft opens on Changes); the evidence tree with a Use column, a detail pane stating what each method's result speaks for, artifacts, one fixed row of equal-width commands, one status line |
| App | The Evidence area's dispatch (Import… chooser; a link goes to `system_browser_opener` only as a validated address); `bpr draft open --evidence FILE`, here or forwarded over the owner channel; the attended look `--attended-local-draft-evidence FILE`; the page held by `QPointer` (a window torn down first no longer crashes shutdown) |

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 | `application_local_draft_evidence_desktop_public_path` (real window and composition, real worktree, no agent, account or network): Add Claim…, Add Check…, Record Result… and Add Question… through the Evidence area's buttons; a revision keeps the earlier wording and shows it; Import… applies a document chosen in the window; the kept TLC output is read back in the artifact reader; after a restart the draft opens on Changes and the claims, results, import, earlier wording and selection are all there. `local_draft` (real files): the import's log is copied and read back from the store; a new store over the same profile resumes the evidence. `bpr draft open --evidence` from a terminal: created the draft and imported 2 claims, 2 checks, 2 reported results (2 bound), 2 questions | Pass |
| AC02 | `core_local_draft_evidence`: an import whose result says `observed` is refused (`observed`), one without a class is refused, a stored record with `observed` does not decode, and no encoded draft contains the word; a claim has no verified state and a reported pass of a proposed check does not count for it. Desktop course: the observed import is refused whole with its reason; no tree row says verified or observed; results read "Pass, reported by …"; failed, inconclusive, not-run and interrupted outcomes are distinct values (`core_local_draft_evidence`) | Pass |
| AC03 | `core_local_draft_evidence`: a scoped result is carried to a text-only candidate, an unscoped one needs a recheck, a changed input needs a recheck, a new base needs a recheck, the earlier candidate still shows it current; a commit alone does not bind a candidate with working files, the exact commit and working objects do. `local_draft` (real Git): a report naming HEAD and the `git hash-object` of the included working file binds the dirty candidate 2; HEAD alone binds only the clean candidate 1 and needs a recheck for candidate 2. Desktop course: after a README-only candidate the scoped test is "Carried from candidate 1", the model check "Recheck", and choosing candidate 1 shows it "This candidate". Negative control: `candidate_material` without working files fails `core_local_draft_evidence` at "a commit alone does not bind a candidate with working files" | Pass |
| AC04 | `core_local_draft_evidence`: `reach` gives model checks the model, proofs the property, benchmarks the environment, tests the material; a formal check keeps property, subject, tool, assumptions and bounds; a test drops formal fields. Desktop course: the imported model check's detail says it "Concerns the model it analyzed, within its bounds; not the code." Nothing aggregates results into a whole-change or pull-request label | Pass |
| AC05 | `core_local_draft_evidence`: oversized (unread), malformed, other format, other version, unknown field, invalid values, references to nothing, empty, duplicate and stale drafts are refused whole. `local_draft` (real files): an oversized, a malformed, a missing and an observed document, and a cancelled import, leave the stored draft byte-equal; a missing artifact is `missing`, an artifact through a symbolic link to a file outside the folder is `refused` and not read, a link is kept and not fetched. No path reads a secret store or runs a command: the import reads only beneath the chosen folder through the workspace port | Pass |
| AC06 | Desktop course: imported claim, check and questions arrive Proposed; Accept and Reject in the window accept the claim and model check, reject one question (kept, dimmed, "Rejected") and accept the other; the Use column selects a claim, a result and a question, and `selected_evidence` offers exactly those with their check, without earlier wordings, proposed or rejected records; a rejected record has no Use box. No agent is started anywhere in this RFC's code | Pass for the flow; the look is the owner's ([Owner look](#owner-look)) |
| AC07 | `core_local_draft_evidence` and `local_draft`: an import from producer kind `agent` with the pull request checkout's commit arrives reported, origin agent, material that commit, bound to no candidate and `unbound` for the draft's candidates; it cannot say observed | Pass |

## Runs

At `8f5ca195f` (dev preset, Debug): `core_local_draft`,
`core_local_draft_evidence`, `local_draft`, `core_profile_data_reset`,
`sqlite_profile_data_reset_files`, `application_local_draft_desktop_public_path`,
`application_local_draft_evidence_desktop_public_path`,
`application_review_round_trip_composition_contract` (8/8; the contract had
failed since local-draft-entry on its app source inventory, then on a
shutdown crash, both fixed here). `bpr draft open --evidence` and the
`--attended-local-draft-evidence` look offscreen (`ATTENDED_LOCAL_DRAFT_SHOWN`).
`sqlite_tour_owner_channel` and `architecture_boundary_contract` pass;
`architecture_boundaries` failed on `QRandomGenerator::system()` in
`local_draft_handoff.cpp` (from local-draft-entry; the gate bans the token as a
process call) and passes after it became `global()` in the commit that adds
this ledger. clang-format 22 on every changed C++ file; clang-tidy (warnings
as errors) on the changed translation units.

## Owner look

Filed, not waited on: session `7fc09e758ebf8ac0d028da38a5d374f7` (state
`/Users/jaeger/.attended-review-state/local-draft-evidence-v001-2026-11-11/state`,
cursor 6), candidates `BPR-2024-evidence-area` and `BPR-2023-result-wording`
on the frozen build of `8f5ca195f370` (root
`/Users/jaeger/local-draft-evidence-v001.pdc9rq1b`, executable SHA-256
`055e387e9e0e…`). AC06's flow passes on the course; its look is pending.

## Limits

- Conversation-proposed claims and checks arrive with RFC-BPR-0110; until
  then proposals come only by import.
- Description and tour use of the selection arrive with RFC-BPR-0110; this RFC
  provides `selected_evidence` and the Use column.
- `inputs` are the producer's statement; nothing derives dependencies.

## Owner verdicts (attended session 7fc09e758ebf8ac0d028da38a5d374f7, handed back 2026-11-12T06:24:42Z)

- `BPR-2023-result-wording`: **Reject** (decision `214fd32c-fb1c-4442-95a0-0fcd9f640187`, 2026-11-12T06:23:51Z, actor human), the owner: "I have no idea what I'm looking at here. This doesn't seem useful, unless you can explain how it's valuable to me."
- `BPR-2024-evidence-area`: **Reject** (decision `40abb603-d9d5-4b6c-bd3b-3ef8576ac037`, 2026-11-12T06:24:41Z, actor human), the owner: "Same as other / I have no idea what I'm looking at here. This doesn't seem useful, unless you can explain how it's valuable to me."

Root `/Users/jaeger/local-draft-evidence-v001.pdc9rq1b` removed with `freeze_attended_build.py remove` and the host
stopped (2026-11-12). The coordinator: a redesign from mockups comes first;
no new look is filed until then.

## Owner amendment: deferred and removed (2026-11-12)

The owner, asked whether to cut Evidence from 0.5.0: "Mockups; cut
Evidence". This RFC is Deferred, out of 0.5.0, kept as a possible workflow
extension; [[BPR-2023]], [[BPR-2024]] and [[BPR-2025]] are Blocked on
`owner-deferred`. The results above are history: the code they exercised was
removed from the product on the lane (the Evidence area, `bpr draft open
--evidence`, the stored record and its artifacts, agent-proposed claims and
checks, citations, selected evidence in previews and the publication plan),
with a forward migration for stored drafts. `core_local_draft_evidence` and
`application_local_draft_evidence_desktop_public_path` no longer exist. The
removal's own checks are recorded in the
[RFC-BPR-0110 ledger](RFC-BPR-0110.md#owner-amendment-evidence-removed-2026-11-12).

## Owner decision: Rejected (2026-11-15)

The deferral above is superseded. The owner, on 2026-11-15: "it shouldnt be
deferred it should be rejected or something stronger so it doesnt come back".
RFC-BPR-0109 is Rejected, a terminal status the documentation gate keeps
closed: no open card or milestone plan may name it. [[BPR-2023]],
[[BPR-2024]] and [[BPR-2025]] are closed in Done with
`resolution: "rejected"` and count as not delivered. Nothing above is
reopened; the results stay as history.
