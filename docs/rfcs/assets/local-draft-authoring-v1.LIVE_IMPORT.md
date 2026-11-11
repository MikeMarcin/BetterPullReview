# Local draft authoring v1: live import

Imported 2026-11-10 at the owner's request ("so /author-rfc our final
suggestions as a pack and import it"). This is an import, not a mandate to
implement: every card is Backlog and no milestone is started. RFC_BASE_SHA:
`b0d54697a6db6b17b459a751515de60aff14d65d`. Imported on lane
`claude/v0-5-0-prerelease-targets-52265d`, the only branch that holds the
RFCs this pack extends (RFC-BPR-0108 to 0112). The original pack bytes are
kept under [local-draft-authoring-v1](local-draft-authoring-v1/README.md)
with their provisional IDs.

## Mapping

RFC-DRAFT-0001 to 0003 map to RFC-BPR-0113 to 0115 in order. DRAFT-0001 to
0010 map to BPR-2062 to 2071 in order, allocated by
`scripts/allocate_issue_id.py`; receipt
`docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json`. The full
map is [local-draft-authoring-v1.id-map.json](local-draft-authoring-v1.id-map.json)
and [the plan's source record](../plans/local-draft-authoring-v1/source.json).

The allocator refuses a non-`main` branch, and in the main checkout it
refused as well: main's counter read BPR-2061 against this lane's high water
of BPR-2062 ("BPR-2061 rewinds historical high water BPR-2062"). The
allocation therefore ran in the main checkout at `f09384dc4` with its
working counter set to BPR-2062, the lane's own value; the counter and
receipt were moved to this lane (`b0d54697a`) and the main checkout was
restored, with nothing committed there. Main then saw this lane's counter of BPR-2072 as high water and reserved
BPR-2072 for a card of its own (`b044c5fb8`); it reads BPR-2073, and the
lane's counter follows it. Nothing collides: main holds no card or
reservation in BPR-2062..2071. RFC numbers have no counter: 0113
to 0115 were free on every local and remote branch at import. The integrator
renumbers on a collision.

Closeout cards take the board's required name and title
(`BPR-NNNN-close-rfc-NNNN`, "Verify and close RFC-BPR-NNNN"); the draft title
is kept in card metadata as `draft_title`. Each closeout carries a pending
checklist with every canonical criterion. RFC acceptance sections are
renamed "Acceptance criteria" for the documentation gate; their text is
unchanged.

## Lanes and plan

All ten cards are Backlog. The authored four-milestone grouping is adopted
without scope or exit changes under
[plans/local-draft-authoring-v1](../plans/local-draft-authoring-v1/MILESTONES.md).
Every milestone waits on adopted work of the 0.5.0 pack that is not yet
built: BPR-2020 and BPR-2021 (local draft candidates and shell), BPR-2029
(pull request creation), BPR-2007 (the agent grant line), BPR-2026
(local-draft sessions) and BPR-2016 (the prompt sharing format).

## Amendments to adopted RFCs

The pack's [proposed amendments](local-draft-authoring-v1/AMENDMENTS.md)
change requirements in RFC-BPR-0104, 0108 and 0111 and clarify RFC-BPR-0110
and 0112. They are not applied at import; each owning milestone reconciles
its live text, and the changed exit of the adopted plan's
`publish-prepared-draft` milestone is an explicit plan amendment. One thing
was written into live text: a note at the head of RFC-BPR-0111's "Native
branch decision" saying that the owner reopened PUBLISH-01 and pointing
here, so that the section is not implemented as the final word. The
section's own text, its acceptance criteria and its cards are unchanged.

## Decisions carried

D01, D02, D03 and D06 of the pack's
[decision record](local-draft-authoring-v1/DECISIONS.md) are the owner's
answers. P01 to P11 are the pack author's proposals and are unconfirmed.
The pack was not reviewed before import.

## Drift

The pack inspected `5380330fb1e1f15f5a3da476e0e8260ba4a730a3`. Between it
and the baseline only the issue counter and this import's reservation
changed.

## Exclusions

No credentials, logs, screenshots or build output were imported. Adoption is
not implementation acceptance.
