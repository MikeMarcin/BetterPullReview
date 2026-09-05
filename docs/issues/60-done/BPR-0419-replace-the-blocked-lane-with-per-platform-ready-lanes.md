---
schema: "implement-rfc.card.v1"
id: "BPR-0419"
title: "Replace the blocked lane with per-platform ready lanes"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# Replace the blocked lane with per-platform ready lanes

Product owner, 2026-08-22, after being shown that 56 backlog cards had nothing
startable in them:

> if there is generic work and platform specific work that cannot be done here
> mixed in the same card that platform work should be split off into a blocked
> follow-up so we can work the generic part

and then, on where those follow-ups belong:

> or put them on ready-win ready-mac ready-lin instead of blocked

## What was wrong

[[BPR-0358]] settled that a card requiring Windows is not `50-blocked/`, on the
grounds that the project owns a Windows 11 x64 host, so the work is a queue
position rather than a hardware wait. That reading is now overruled, and the
owner is right that it was the wrong one: it is true about the project and
useless to a session. A macOS session reading this board could not tell which
cards it could actually pick up, and the answer — none of them — was buried in a
paragraph rather than visible in the lanes.

The opposite reading is no better. Calling the work *blocked* says something is
unresolved, and nothing is: the machine exists, the work is specified, and it
will run the day a session opens on that host. It is ready. It is ready
somewhere else.

## The lanes

| Folder | Meaning |
|---|---|
| `20-ready/` | ready, and portable — any session can start it |
| `21-ready-win/` | ready, and requires target-native Windows |
| `22-ready-mac/` | ready, and requires target-native macOS |
| `23-ready-lin/` | ready, and requires target-native Linux |

`50-blocked/` is **kept**, on the owner's follow-up direction — "50-blocked is
still reasonable to keep if there is a legit blocker" — with a narrower meaning:
something genuinely unresolved that is not a platform, not a person, and not
another card. Its two current residents are neither; both are `platform-linux`
and move to `23-ready-lin/`, leaving the lane empty and correctly so.

`blocked_by` survives with it, but as a free token rather than the old closed
set of platform values, because the blockers worth recording are the ones nobody
enumerated in advance. The verifier rejects a `platform-` value there: naming a
platform would hide the card from the session that could run it, which is the
failure these lanes exist to fix.

Rules carried over from the lane it replaces, unchanged:

- The per-platform lanes are **deferring** lanes: like `50-blocked/`, a card in
  one may still have an unmet dependency. The platform is the reason it is not
  in `20-ready/`; the dependency graph is a separate question and stays in
  `depends_on`.
- They are frontier lanes, so `scripts/verify_documentation.py` derives them
  into the "Frontier roots" table and fails when the table disagrees.
- Folder placement remains authoritative. All four ready lanes carry
  `status: Ready`, because the folder already names the platform and a second
  copy of that fact in front matter is a second thing to keep in sync.
- `blocked_by` no longer accepts a platform. `platform-linux-and-windows` had no
  residents and is gone entirely — under the owner's splitting rule a card
  needing two platforms is two cards.

## Required final state

- `scripts/verify_documentation.py` knows the new lanes everywhere it knew the
  old one: lane/status agreement, deferring-lane dependency rules, frontier
  derivation, and the retired `blocked_by` field.
- `tests/scripts/verify_documentation_tests.py` covers the new lanes, including
  a case that a per-platform lane may hold an unmet dependency and a case that
  the frontier table must list it.
- `docs/issues/README.md` documents the lanes and records why the [[BPR-0358]]
  reading was replaced rather than quietly dropped.
- No card is in `50-blocked/`, and the directory is gone.

## Non-goals

Splitting mixed cards is the owner's other direction and is separate work: this
card builds the lanes those splits will land in. It moves only cards whose
remaining work is *already* wholly platform-bound.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
ctest --test-dir build/core-only --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

## Done — 2026-08-22

The lanes exist and the board uses them.

| Lane | Cards |
|---|---|
| `20-ready/` | 2 — portable work any session can start |
| `21-ready-win/` | 10 — the Windows program, queued for its host |
| `23-ready-lin/` | 2 — both were `50-blocked/` residents |
| `50-blocked/` | 0, and that is the correct number today |

`22-ready-mac/` and `50-blocked/` have no residents, so their directories do not
exist. The verifier tolerates a missing lane directory and will accept the first
card placed in either.

### What moved, and what deliberately did not

Ten cards whose remaining work is wholly Windows-bound moved to `21-ready-win/`:
[[BPR-0036]], [[BPR-0053]], [[BPR-0311]], [[BPR-0315]], [[BPR-0347]],
[[BPR-0350]], [[BPR-0358]], [[BPR-0360]], [[BPR-0364]], [[BPR-0412]]. Three of
them were in `30-in-progress/`, which claimed a session was working them; none
was, and no macOS session could.

[[BPR-0240]] stayed in `30-in-progress/`. Its remaining work is Windows-only, but
it holds real in-flight state from an earlier session and its portable half was
already carved out into done [[BPR-0333]]/[[BPR-0334]] and open [[BPR-0332]].
Moving it would claim a tidiness the card does not have.

[[BPR-0362]] stayed in `40-parked/`. Its next action is a product decision, and
a decision needs a person whichever machine they are sitting at. The lane that
names a person outranks the lane that names a platform.

### Relative links were the one real hazard

Moving ten cards broke five closeout cards' procedure links, which resolve
relative to the card file — `verify_documentation.py` caught every one
(`closeout procedure link does not resolve to an issue`), and nine links across
BPR-0035, BPR-0054, BPR-0083, BPR-0101, and BPR-0242 were repaired by deriving
each target's new lane rather than by hand.

### Verification

| claim | command | result |
|---|---|---|
| the board satisfies every lane, status, dependency, and frontier rule | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |
| the contract suite covers the new lanes | `python3 tests/scripts/verify_documentation_tests.py` | 104 tests, OK — up from 98 |
| a platform lane may hold an unmet dependency | `test_accepts_platform_ready_card_with_open_dependency` | pass |
| a platform lane rejects a status that disagrees with it | `test_rejects_platform_ready_card_with_wrong_status` | pass |
| the frontier table derives platform lanes | `test_derives_a_platform_ready_lane`, `test_rejects_an_omitted_platform_root` | pass |
| a platform may not be recorded as a blocker | `test_rejects_a_platform_as_a_blocker` | pass |
| a real blocker reason is accepted | `test_accepts_a_real_blocker_reason` | pass |
| the shipped documentation gates agree | `ctest -R '^(documentation_structure\|documentation_issue_graph_contract)$'` | 2/2 pass |
