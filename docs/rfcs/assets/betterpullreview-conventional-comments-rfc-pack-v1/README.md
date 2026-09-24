# BetterPullReview — Conventional Comments RFC pack v1

Implement optional, complete Conventional Comments support with configurable
resolution-aware Overview counters, compact label visuals, native GitHub review
integration and convention-aware default skill hooks.

## Start here

Read `IMPLEMENTATION-PROMPT.md`, then `references/IMPORT-AND-NUMBERING.md` and
`MILESTONES.md`. `milestone-plan.json` is the authored execution seed, not a hint
to discard and rebuild. The archive is a specification and implementation plan;
no BPR implementation, GitHub mutation, live agent run or visual acceptance is
claimed by delivering it.

## RFC allocation

| Proposed ID | Scope |
| --- | --- |
| RFC-BPR-0047 | All twelve labels, decorations, lossless recognition and independent semantics |
| RFC-BPR-0048 | Optional authoring, native suggestions and review decisions, defects and tour/private-draft compatibility |
| RFC-BPR-0049 | Per-label Overview counters, catchall partition, resolution/coverage and settings |
| RFC-BPR-0050 | Dedicated UX/icon design decisions, implementation and rendered acceptance |
| RFC-BPR-0051 | Convention-aware default hooks and shared headless/CLI behavior |

Import after the original review-tour pack (proposed 0039–0043), then its CLI/skill
extension (proposed 0044–0046). This pack therefore proposes **0047–0051** and uses
`PACK-CC-*` issue aliases instead of colliding with live issue allocations.

**Evidence limit:** the two attached `.zip.sha256` files are retained verbatim.
The prior archive names were found in the Library, but their raw bytes could not
be materialized and their text reads contained no readable content. Predecessor
RFC ranges come from the earlier pack handoff, not manifests verified here.
Intake must check actual predecessor manifests/import maps and remap this pack
above any live reservations. A checksum file alone does not establish RFC IDs.
See `references/PREDECESSOR-PACKS.json` for exact expected archive digests.

## Product requirements retained

All twelve labels are first-class: praise, nitpick, suggestion, issue, todo,
question, thought, chore, note, typo, polish and quibble. Plain comments remain
valid. Settings choose an ordered subset of label columns; the existing catchall
counts everything not broken out within the same declared population. Count
logical conversations, not their replies. Unresolved-only is the initial primary
mode; an all/include-resolved mode retains the state distinction. Unknown and
non-applicable resolution must not be confused with resolved or a clean zero.

Icons are intended to be consistent across compact column headers and review
threads. Their actual glyphs, layout and controls are deliberately not preselected;
RFC-BPR-0050 owns a real design-and-delivery workstream. This archive contains no
approved artwork or mockup.

No AI is required. Relevant default hooks teach the conventions only when optional
AI assistance is invoked, preserving overrides and tour problem/solution/outline
purposes. GitHub native suggested changes and Comment / Approve / Request changes
remain first-class; labels neither replace them nor trigger them automatically.

## Contents and validation

The pack contains five additive RFC overlays, 27 importable issue-card templates,
eight authored milestones, a 48-requirement traceability matrix, source/integration
audit, grammar and catalog, parser/counter/native-interaction fixtures, proposed
hook content, a local validator and negative validator tests.

From the extracted pack root, using Python 3.10 or newer:

```sh
python3 -B scripts/verify_pack.py
python3 -B -m unittest discover -s tests -p 'test_*.py' -v
```

These commands read the pack only and run no network or BPR code. They validate
integrity, schema/graph consistency, requirement coverage and normalized fixture
accounting, including every one of the 4,096 label-column subsets. They do not
implement or execute the BPR parser, validate a live import, prove UI quality,
verify GitHub behavior or invoke a model. `verification/PACK-VALIDATION.md` records
the checks actually performed for this archive.

Keep this original archive immutable on import. Copy only the new RFCs into the
live RFC registry after collision resolution; allocate live issue IDs and adapt
the authored plan using the installed implement-rfc workflow. Merge index entries,
not whole old index/settings/skill files. Product implementation begins from the
live source and inherited tour contracts, not from a reconstructed snapshot.
