# BetterPullReview Issue Board

Issue IDs use the `BPR-NNNN` sequence allocated by `NEXT_ID`.

## Lanes

| Folder | Status | Meaning |
|---|---|---|
| `10-backlog/` | Backlog | Planned but waiting on another card |
| `20-ready/` | Ready | Unblocked, and portable — any session can start it |
| `21-ready-win/` | Ready | Unblocked, and requires target-native Windows |
| `22-ready-mac/` | Ready | Unblocked, and requires target-native macOS |
| `23-ready-lin/` | Ready | Unblocked, and requires target-native Linux |
| `30-in-progress/` | In Progress | The current implementation slice |
| `40-parked/` | Parked | Nothing is in the way but a person or a live service |
| `50-blocked/` | Blocked | Something is genuinely unresolved, and it is not a platform, a person, or another card |
| `60-done/` | Done | Verified terminal state |

Folder placement is authoritative. Front-matter `status` must agree with the
folder. Move a card rather than copying it, and update dependencies when new
work is discovered. The two-digit prefix exists so that a directory listing
sorts the lanes in workflow order rather than alphabetically.

### Ready, and ready where

A lane has to tell a session what it can actually start. That is the whole
reason the ready lanes split by platform, and it took two wrong answers to get
here.

[[BPR-0319]] created `50-blocked/` for work needing an operating system this
checkout did not have. [[BPR-0358]] then argued the opposite: the project owns a
Windows 11 x64 host, so a Windows card is a queue position rather than a
hardware wait, and it moved nine cards back into active lanes. Both readings are
defensible and both were useless in practice. The first told a macOS session
that work was blocked when the machine existed; the second told it the work was
active when the session could not touch it. Either way, the only way to learn
that nothing here was startable was to read the cards.

So `2X-ready-<platform>/` says the true thing: the work is *ready*. It is
specified, its blockers are gone, and it will run the day a session opens on
that host. It is ready somewhere else.

- `20-ready/` is ready **and portable** — the lane a session picks from when it
  does not care which machine it is on.
- `21-ready-win/`, `22-ready-mac/`, `23-ready-lin/` are ready **on that
  platform**. A session that is not on it skips the lane; a session that is
  reads it as its queue.
- All four carry `status: Ready`. The folder names the platform, and a second
  copy of that fact in front matter is a second thing to keep in sync.
- The platform lanes **defer**, exactly as `50-blocked/` did: a card in one may
  still have an unmet dependency. The platform is why it is not in `20-ready/`;
  the dependency graph is a separate question and stays in `depends_on`.

`50-blocked/` survives that change with a narrower and more honest meaning:
something is genuinely unresolved. Not a platform, which is a lane; not a
person, which is `40-parked/`; not another card, which is `depends_on`. Its
`blocked_by` field is a free token rather than a closed set, because the
blockers worth recording are the ones nobody enumerated in advance — and
`scripts/verify_documentation.py` rejects a `platform-` value there, since
naming a platform would hide the card from the session that could run it.

When a card mixes portable work with work that needs another platform, split the
platform part into its own card so the generic part stays workable. That is
product-owner direction, given 2026-08-22. It has one condition: the portable
half must be *verifiable* here. A slice whose only proof is a target-native run
is not separable, and splitting it manufactures a card that cannot close —
[[BPR-0360]] and [[BPR-0364]] both considered exactly that split and correctly
declined it.

`40-parked/` holds a card whose every dependency is already done, and whose own
remaining work needs a person or a live service. Each one carries a `parked_on`
field: `awaiting-owner-verdict` for a person who must look, click, or return an
exported verdict; `awaiting-live-github` for a real GitHub call against a
designated target; `awaiting-product-decision` for a decision nobody has made
yet. A card returns to a ready lane when the verdict, the session, or the
decision arrives.

A parked card is a *frontier root* — nothing is queued in front of it, so it is
the only kind of open card someone could act on today. That is enforced rather
than asserted: `40-parked/` is not a deferring lane, so the dependency check
fails a parked card that still has an unmet dependency.

Each card links one RFC, names its dependencies, defines completion evidence,
and is committed with the implementation slice it tracks. RFC closeout is
blocked while any required card remains outside `60-done/`.

Verification scope follows the transition. Moving a card to `60-done/` needs
the focused selectors that card names and `clang-format` on the files it
touched.

Two selector traps are recorded because both have cost a day. A selector chosen
by *subsystem* name misses the suites named after the *composition* that holds
it — [[BPR-0418]] ran `github_conversation|workbench|thread_map` green and broke
`qt_composed_github`, which builds the same window through the same drawer. And
a diff that adds or changes a `LOOPBACK_PORT_COUNT` must run
`test_network_isolation_manifest`, because no ordinary selector matches it and
[[BPR-0452]] sat red for a day between the registration and the next full
matrix.
The full `dev` and `core-only` matrices and the strict `coding-tidy` target
run for the better part of an hour, and spending that on every slice buys
nothing a focused selector did not already prove. Run them only on an explicit
request or when a change plausibly reaches beyond its slice; they are not a
closeout gate. A closeout records, per acceptance criterion, the focused check
that would fail if the criterion were false, its result, and the commit, and
reuses a passing result whose inputs did not change.

## Allocating IDs

```text
python3 scripts/allocate_issue_id.py [--count N] [--json]
```

The allocator prints the claimed IDs and advances `NEXT_ID` as one step under
an exclusive lock, refusing when the counter is not already ahead of every
card in the tree or is below the highest value the file has held in Git
history — the case a concurrent branch creates. Editing `NEXT_ID` by hand
still works on `main`; the allocator is a convenience and a guard.

On any branch other than `main` — a station branch such as `windows-lane` —
block reservation is mandatory: the branch never edits `NEXT_ID` and consumes
IDs only from a block the integrator reserved on `main` with

```text
python3 scripts/allocate_issue_id.py --count 8 \
    --reserve-for bpr-win-jaegaw --purpose "windows-lane block"
```

`--reserve-for` advances the counter and writes one immutable receipt per
reservation under `docs/issues/_reservations/`. Commit the advanced counter
and the receipt together on `main`; that commit is the `reservation_commit` a
dispatch envelope quotes, and the consuming branch verifies its block with
`git show <reservation_commit>:<receipt-path>` after a fetch, never by
merging `main`. Receipts are never edited or deleted; an abandoned block
keeps its receipt and its unused IDs are burned, not recycled. The station
contract that consumes these reservations lives in
`.agents/skills/implement-rfc/references/remote-routing.md`.

## Programs and history

The RFC index (`../rfcs/README.md`) is the only order authority; each card's
`depends_on` carries sequencing. The lanes carry the frontier: a card in a
ready or in-progress lane whose dependencies are all done is where work can
start, and no table restates that. The board's narrative record of how each
RFC got where it is — the dev-binary hold of August 2026 and the per-RFC
frontier history — lives in [HISTORY.md](HISTORY.md) and is not validated.

RFC closeout runs `python3 scripts/verify_documentation.py --root .` (lane and
status agreement, dependencies, links, the light closeout-card contract) and
the focused checks the closeout card names; see
`.agents/skills/implement-rfc/references/project.md`.
