# Agent assistance v1: live import

Imported 2026-11-10 at the owner's request ("The other agent ideas from the
initial ask are part of our final suggestions", after "author our final
suggestions as a pack and import it"). This is an import, not a mandate to
implement: every card is Backlog and no milestone is started. RFC_BASE_SHA:
`a9663a706fd72061f60b615bd8e749ee0ddb66b5`. The original pack bytes are kept
under [agent-assistance-v1](agent-assistance-v1/README.md) with their
provisional IDs.

## Where it was imported

On branch `claude/agent-assistance-v1-import`, cut from lane
`claude/v0-5-0-prerelease-targets-52265d` at `90785ae34`, in its own
worktree. The lane is where the RFCs this pack builds on live (RFC-BPR-0096
to 0115), but another session was merging `main` into the lane's worktree
when this import ran, so nothing was committed there. This branch is to be
merged into the lane; it touches only this pack's files, the RFC index and
the issue counter.

## Mapping

RFC-DRAFT-0001 to 0006 map to RFC-BPR-0116 to 0121 in order. DRAFT-0001 to
0013 map to BPR-2073 to 2085 in order, allocated by
`scripts/allocate_issue_id.py`; receipt
`docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json`. The full
map is [agent-assistance-v1.id-map.json](agent-assistance-v1.id-map.json)
and [the plan's source record](../plans/agent-assistance-v1/source.json).

The allocator refuses a non-`main` branch. It ran in the clean main
checkout at `e5bb712ab` (counter BPR-2073, which the lane's counter also
read); its counter and receipt were moved to this branch (`a9663a706`) and
the main checkout was restored, with nothing committed there. This branch's
counter reads BPR-2086. Main's and the lane's read BPR-2073 until they
follow it, and their allocator and documentation gate refuse until then
("rewinds historical high water BPR-2086"). RFC numbers have no counter:
0116 to 0121 were free on every local and remote branch at import. The
integrator renumbers on a collision.

Closeout cards take the board's required name and title
(`BPR-NNNN-close-rfc-NNNN`, "Verify and close RFC-BPR-NNNN"); the draft title
is kept in card metadata as `draft_title`. Each closeout carries a pending
checklist with every canonical criterion. RFC acceptance sections are
renamed "Acceptance criteria" for the documentation gate; their text is
unchanged.

## Lanes and plan

All thirteen cards are Backlog. The authored six-milestone grouping is
adopted without scope or exit changes under
[plans/agent-assistance-v1](../plans/agent-assistance-v1/MILESTONES.md). The
milestones are independent of one another. `agent-run-checks` waits on
BPR-2024 (the evidence record) and BPR-2068 (change runs); the other five
build on shipped code.

## Amendments to adopted RFCs

The pack's [proposed amendments](agent-assistance-v1/AMENDMENTS.md) change
the subject rule of `evaluate_proposed_fix` and `analyze_current_file` and
add to RFC-BPR-0068, 0086, 0090, 0102, 0107 and 0109 without changing what
they require. None is applied at import; each owning milestone reconciles
its live text.

## Decisions carried

D01 of the pack's [decision record](agent-assistance-v1/DECISIONS.md) is the
owner's answer. P01 to P08 are the pack author's proposals and are
unconfirmed. The pack was not reviewed before import.

## Drift

The pack inspected `90785ae34aeccf63724b9e5e7a70446f89341ec5`. Between it
and the baseline only the issue counter and this import's reservation
changed.

## Exclusions

No credentials, logs, screenshots or build output were imported. Adoption is
not implementation acceptance.
