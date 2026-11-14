# Ask v1: live import

Imported 2026-11-13 on lane `claude/v0-5-0-prerelease-targets-52265d`
(mandate `v0-5-0-prerelease-targets-r1`), mirrored to `main`. Written at
import as a new RFC from the owner's design decisions; there was no draft.
No product code was written; every card is Backlog. Baseline `f85b216ec`.

## Sources

| File | SHA-256 | Kept at |
|---|---|---|
| The Ask mockups `index.html` and its 38 renders in `png/` | `index.html`: `49a75b4772b3671f5ebcbb31625230b1b698a2fc914233661afd92ff72cda02a` | [docs/design/ask-v1](../../design/ask-v1/README.md) |
| The owner's decisions of 2026-11-13 (section "Ask model") | `4e65920c294a866fc8a78a41a93a90f9d0fadec14bdd6d95f57945cfbe74225f` | Quoted on the RFC and the design README |

Excluded: the mockups' generators (`gen.py`, `base_gen.py`) and stylesheets
(the page inlines its styles).

## Why a new RFC

The decisions change RFC-BPR-0116, 0117, 0118, 0119 and 0125 together and
add a report kind to the tab model; spreading them over five RFCs would leave
no single place to build and verify them. Each of those RFCs records the
amendment and points here.

## Cards and plan

[RFC-BPR-0128](../RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md);
[plan](../plans/ask-v1/MILESTONES.md), three milestones.

| Card | What | Criteria |
|---|---|---|
| [BPR-2163](../../issues/60-done/BPR-2163-quick-asks-and-their-scopes.md) | Quick asks: flag, scopes, availability | AC03, AC04 |
| [BPR-2164](../../issues/20-ready/BPR-2164-ask-control-at-three-anchors.md) | One Ask control at three anchors | AC01, AC02, AC08 |
| [BPR-2165](../../issues/10-backlog/BPR-2165-was-this-addressed-on-any-thread.md) | Was this addressed? anywhere, claim chip | AC05 |
| [BPR-2166](../../issues/10-backlog/BPR-2166-report-opens-as-a-file-tab.md) | The report as a file tab | AC06 |
| [BPR-2167](../../issues/10-backlog/BPR-2167-merge-of-main-is-one-line.md) | A merge of main is one line | AC07 |
| [BPR-2168](../../issues/10-backlog/BPR-2168-close-rfc-0128.md) | Closeout | all |

IDs allocated on `main` (`eaf625a0f`, receipt
`docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json`).
