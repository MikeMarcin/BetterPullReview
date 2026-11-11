# 0.5.0 prerelease targets v1: live import

Mandate `v0-5-0-prerelease-targets-r1`, 2026-11-10: the owner asked to
implement this pack from its implementation entrypoint, starting with
`dependable-session` and following the plan's order. RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Imported on lane
`claude/v0-5-0-prerelease-targets-52265d`. The original pack bytes are kept
under [v0-5-0-prerelease-targets-v1](v0-5-0-prerelease-targets-v1/README.md)
with their provisional IDs; its [design round](../../design/v0-5-0-targets-v1/index.html)
moved with it, unchanged.

## Mapping

RFC-DRAFT-0001 to 0015 map to RFC-BPR-0098 to 0112 in order (draft number
plus 97). DRAFT-0001 to 0047 map to BPR-1989 to 2035 in order (draft number
plus 1988), allocated by `scripts/allocate_issue_id.py`; receipt
`docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json`. The full
map is [v0-5-0-prerelease-targets-v1.id-map.json](v0-5-0-prerelease-targets-v1.id-map.json)
and [the plan's source record](../plans/v0-5-0-prerelease-targets-v1/source.json).

The allocator refuses a non-`main` branch. Under the coordinator's instruction
it ran once against the main checkout, whose counter and receipt were moved to
this lane and the main checkout restored to its clean state; `main`'s counter
still reads BPR-1989 until this lane integrates. RFC numbers have no counter:
0098 to 0112 were free on every branch at import. The integrator renumbers on
a collision.

Closeout cards take the board's required name and title (`BPR-NNNN-close-rfc-NNNN`,
"Verify and close RFC-BPR-NNNN"); the draft title is kept in card metadata as
`draft_title`. Each closeout carries a pending checklist with every canonical
criterion. RFC acceptance sections are renamed "Acceptance criteria" for the
documentation gate; their text is unchanged. The absorbed RFCs' and cards'
`origin`/`absorbed_from` metadata keep the local PR drafts pack's own former IDs
verbatim, since those name that pack, not this one.

## Lanes and plan

BPR-2010 and BPR-2011 (`dependable-session`) are Ready; every other card is
Backlog. The authored sixteen-milestone grouping is adopted without scope or
exit changes under [plans/v0-5-0-prerelease-targets-v1](../plans/v0-5-0-prerelease-targets-v1/MILESTONES.md).

## Drift

The pack inspected `b12269ea9ad1ff07afe8ec31812875b89d870e5a`. Between it and
the baseline only agent guides (`AGENTS.md` files), `.mask.lock.json`, the
issue counter and one reservation changed; no source the RFCs cite moved.

## Exclusions

No credentials, logs, screenshots or build output were imported. The enterprise
overview the pack left outside the repository stays out. Amendments the drafts
make to RFC-BPR-0011, 0012, 0034, 0041, 0042, 0043, 0066, 0070 and 0096 are
not applied at import; each owning milestone reconciles its live text.
Adoption is not implementation acceptance.

## Reconciled with main, 2026-11-10

The owner made `main` the tracker's source of truth. Since the base, main
finished BPR-1988 and reserved and finished BPR-2036 and BPR-2037; after
seeing this lane's counter history it set its own counter to BPR-2036
(`ec70790bc`), then BPR-2038. None of main's IDs collides with this pack:
main holds no card, reservation or RFC in BPR-1989..2035 or
RFC-BPR-0098..0112, so nothing was renumbered. Main was merged into the lane
at `78ad5391b` (a clean merge; main's source arrives with its tracker), after
which both counters read BPR-2038.

Further IDs were allocated against main the same way as at import: the
allocator ran in the clean main checkout at `1513d4a85`, its counter and
receipt moved to this lane, and the main checkout restored. BPR-2038 and
BPR-2039 (receipt `docs/issues/_reservations/BPR-2038--BPR-2039--0af453354534.json`,
`481096d5b`) take the lane's counter to BPR-2040, which is what main's would
read after the same allocation. Main's own counter still reads BPR-2038 until
this lane integrates. Its allocator reads every branch's counter history, so
until main's counter is raised to BPR-2040 it refuses every allocation
("BPR-2038 rewinds historical high water BPR-2040", checked from the main
checkout), as it did before `ec70790bc`. Raising main's counter is the main
integrator's step; this lane does not write to main.

`home-next-move` (2026-11-10) allocated nothing. By then main had reserved
BPR-2040 (`26e8ca1b4`) and BPR-2041 (`8a3b9704f`) and reads BPR-2042, so the
lane's counter at BPR-2040 rewound the high water and the board helpers
refused to move a card. The lane's counter now reads BPR-2042, main's own;
no reservation is imported, and none collides with the lane's BPR-2038 and
BPR-2039.

The live-freeze correction (2026-11-10) allocated nothing. Main had since
reserved BPR-2042 (`1e82f86b2`) and reads BPR-2043; the lane's counter now
reads BPR-2043, main's own. No reservation is imported and none collides.

`agent-work-visible` (2026-11-10) allocated BPR-2043 and BPR-2044 against
main the same way: the allocator ran in the main checkout at `1e82f86b2`
(counter BPR-2043, history high water 2043), its counter and receipt
`docs/issues/_reservations/BPR-2043--BPR-2044--459846f19aa3.json` moved to
this lane and the main checkout restored. Both are Backlog product cards from
the live-freeze diagnosis, not scheduled in the pack. The lane's counter reads
BPR-2045; main's reads BPR-2043 until this lane integrates, and its allocator
refuses until then.
Main then recorded this lane's reservation as observed high water
(`555c15ccb`, counter BPR-2045) and reserved BPR-2045 and BPR-2046; it reads
BPR-2047. The lane's counter now reads BPR-2047, main's own; nothing is
imported and nothing collides. Main then reserved BPR-2047 and reads BPR-2048;
the lane's counter follows it.

The owner fix round (2026-11-10) allocated BPR-2048 and BPR-2049 against main
the same way: the allocator ran in the main checkout at `243dddcbb` (counter
BPR-2048), its counter and receipt
`docs/issues/_reservations/BPR-2048--BPR-2049--1f6f243721c8.json` moved to
this lane and the main checkout restored (`6bc29a5b3`). Main then reserved
BPR-2050 (`085cf45db`) without this lane's receipt and reads BPR-2051; the
lane's counter follows it. Nothing is imported and nothing collides.

The Enhance round (2026-11-10) allocated BPR-2051 against main the same way:
the allocator ran in the main checkout at `809d0296a` (counter BPR-2051), its
counter and receipt
`docs/issues/_reservations/BPR-2051--BPR-2051--efa66a8a01d3.json` moved to
this lane and the main checkout restored. The lane's counter reads BPR-2052;
main's reads BPR-2051 until this lane integrates. Nothing collides.
Main then reserved BPR-2052 to BPR-2055 (skipping this lane's BPR-2051) and
reads BPR-2056; the lane's counter follows it. Nothing collides.
Main then recorded BPR-2056 and reserved BPR-2057 (`86653eacd`) and reads
BPR-2058; the lane's counter follows it in the critical-review round. Nothing
is imported and nothing collides.

The owner feature round on suggested comments (2026-11-10) allocated BPR-2058
and BPR-2059 against main the same way: the allocator ran in the main checkout
at `86653eacd` (counter BPR-2058), its counter and receipt
`docs/issues/_reservations/BPR-2058--BPR-2059--bd1353aea511.json` moved to
this lane and the main checkout restored. The lane's counter reads BPR-2060;
main's reads BPR-2058 until this lane integrates. Nothing collides.
Collision, found while parking: between this lane's allocation and its
receipt commit (`b583bb919`), main reserved BPR-2058 for another card
(`b2a10cccd`, BPR-2058 failure-first repair tests, Done on main) and later
BPR-2060, skipping BPR-2059. The lane's Draft all card is renumbered to
BPR-2061, allocated against main at `f09384dc4` (counter BPR-2061), its
counter and receipt `docs/issues/_reservations/BPR-2061--BPR-2061--37ecd03b9111.json`
moved to this lane (`38ef8e5e0`) and the main checkout restored; every lane
reference to BPR-2058 now names BPR-2061, except the immutable receipt
`BPR-2058--BPR-2059--bd1353aea511.json`, earlier commit messages and the open
attended candidate `BPR-2058-draft-all`. BPR-2059 keeps its number (main has
no BPR-2059). The lane's counter reads BPR-2062, main's BPR-2061 until this
lane integrates.
