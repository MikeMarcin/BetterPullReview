---
schema: "implement-rfc.card.v1"
id: "BPR-0180"
title: "Reconcile RFC-BPR-0006's ledger with what the application composes"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0117"]
---

# Reconcile RFC-BPR-0006's ledger with what the application composes

RFC-BPR-0006's ledger records ten claims, eight of them `Verified`. Every one
of those eight is true about the component it names. None of them is true about
BetterPullReview, because **nothing RFC-BPR-0006 built is connected to the
application**.

This card does not fix that. It records it accurately, corrects the rows that
overstate their surface, and allocates the cards that close it. Fixing it
inside the same commit would bury the finding in a diff.

## How this was found

Scoping BPR-0114 — the owner-attended check that System mode follows an
operating-system scheme change while an explicit mode does not. The card was
parked as *attended*, so the assumption was that the behaviour existed and only
a person was missing. Looking for the surface to select an explicit mode on
found that there is none, and pulling that thread found the rest.

BPR-0168 was parked the same way and is wrong for the same reason. It is
addressed on its own card.

## What was measured

Five commands, each answerable by anyone, run against the `dev` build at this
card's parent commit.

| # | Command | Result |
|---|---|---|
| A | `nm build/dev/src/app/better_pull_review \| c++filt \| grep -c appearance_application_host` | `0` |
| B | `nm build/dev/src/app/better_pull_review \| c++filt \| grep -c packaged_syntax_classifier` | `0` |
| C | `grep -rln appearance_profile src/` | the Qt adapter's own header, own translation unit, own `CMakeLists.txt`, and one stale include |
| D | `grep -rln appearance src/app/` | `src/app/main.cpp` only, and only its test probe |
| E | `grep -n Adapter src/app/CMakeLists.txt` | `AdapterGitHub`, `AdapterGit`, `AdapterSQLite`, `AdapterAI`, `AdapterQt` |

A and B are the load-bearing ones. A symbol count of zero is not weak evidence
that a component is unused — it is the linker reporting that nothing referenced
it, which is stronger than any grep. `appearance_application_host` and
`packaged_syntax_classifier` are not merely unreachable in the shipped binary;
they are not in it.

C is the complement for `appearance_profile`, which *is* in the binary. It is
in the binary because it lives in a static archive alongside code that is used,
not because anything calls it. The only non-test reference is an include in
`diff_workbench.hpp`; `diff_workbench.cpp` uses nothing from it.

E is the direct cause for syntax. `BetterPullReview::AdapterSyntax` is a
correctly declared alias at `src/adapters/syntax/CMakeLists.txt:13`. The
composition target never links it. Beside it in the link list sits
`AdapterAI`, which `src/adapters/CMakeLists.txt:6` declares as an empty
`INTERFACE` library — the composition links a placeholder and omits the real
adapter.

## What this means for the product

There is no appearance mode. There is no font setting. There is no theme. There
is no syntax highlighting in the diff panes — `diff_workbench.cpp`'s only
`Highlight` is `QPalette::Highlight`, which colours a selection.

What the application does have is Qt's native palette propagating through the
composed window, which is macOS and `QMacStyle` doing their own work. That is
worth having and it is real. It is not `core::appearance_mode::system`
resolving, because the application never asks.

## Why the ledger did not catch it

Each row names a *direct surface*, and every row's evidence exercises exactly
the surface it names. The rows that say `Fixture files`, `Instrumented
highlighter`, `Import validator`, and `Settings default` are honest and remain
`Verified` — a component claim proven against that component.

Two rows name a surface they were not proven against:

- `Font change preserves semantics` names **Composed comparison**. It is proven
  against an `appearance_profile` and a `diff_workbench` constructed together
  by a test. That is a composed *pair*, not the composed application, and the
  application composes neither.
- `Settings persist and preview immediately` names **Real application**. It is
  proven against `appearance_application_host` driven by fake ports — a host
  the real application does not construct and does not link.

The lesson is narrow and worth stating, because it will recur. Every one of
these components was proven non-vacuous: assertions were broken, failures
observed, behaviour restored. Non-vacuity proves a test constrains its subject.
It says nothing about whether the subject is reachable from the product. No
break test on `appearance_profile` could have failed differently if the
application never constructed one.

The check that would have caught it is the one used above, and it is cheap: ask
the linker whether the shipped binary contains the thing.

## Ledger corrections made here

- `Font change preserves semantics` — surface restated as the adapter pair it
  is proven against, result annotated that the application composes neither.
- `Settings persist and preview immediately` — surface restated from `Real
  application`, and the result records that the host is absent from the binary.
- `Fresh install defaults to System` — remains true of
  `appearance_settings`, annotated that no application path reads it.
- `Definitions are selected correctly`, `Hunk classification equals whole-blob
  classification`, `Theme change does not re-parse` — remain `Verified` as
  component claims, annotated that the classifier is not linked.
- `System mode follows the OS; explicit does not` — moves from `Deferred,
  owner-attended` to blocked behind the composition cards. Attendance was never
  what was missing.
- A `Composition` section is added stating the gap in the ledger itself, so it
  cannot be read as complete again.

RFC-BPR-0006's own verification matrix asks for `Highlighting integration` and
`Real application`. Neither exists. The RFC stays `Proposed`, which it already
was, and BPR-0115's closeout is now blocked by the new cards as well.

## Cards allocated

| Card | Slice |
|---|---|
| BPR-0181 | Adapter: durable appearance settings behind `appearance_settings_port` |
| BPR-0182 | Compose `appearance_application_host` into the application |
| BPR-0183 | Qt: the appearance profile drives the composed window |
| BPR-0184 | Qt: the appearance settings gesture |
| BPR-0185 | Link the syntax adapter and highlight the diff panes |
| BPR-0186 | Attended interactive launch, so a person can inspect the composition |
| BPR-0187 | Record `microsoft/DirectXMath#336` as a replayable fixture |

BPR-0114 is re-pointed at BPR-0183 and BPR-0184. BPR-0100 is re-pointed at
BPR-0186 and BPR-0187.

## Required direct evidence

- The five measurements above are reproduced in the card, each with the command
  that produces it.
- The corrected ledger names, for every row it still calls `Verified`, the
  component that row was proven against.
- No row claims a surface the application composes until a card lands that
  composes it.
- `verify_documentation.py` and the documentation issue-graph contract pass
  with the new cards and re-pointed dependencies.

## Evidence

Documentation only; this card changes no code and needs no build.

- `python3 scripts/verify_documentation.py --root .` → `DOCUMENTATION_OK`.
- `ctest --preset dev -R documentation` → 2/2, `documentation_structure` and
  `documentation_issue_graph_contract`.
- `git diff --check` clean.

The five measurements are recorded above with the commands that produce them,
so a reader can re-run them rather than trust the table. They are expected to
change: A and B are exactly what BPR-0182 and BPR-0185 must make non-zero, and
E is what BPR-0185 must extend.

## What was corrected elsewhere

- `docs/verification/RFC-BPR-0006.md` — status restated, a `Composition`
  section added ahead of the claim matrix, every row's `Direct surface` restated
  to name what it was proven against, the two overstated rows called out
  explicitly, BPR-0114 moved from deferred to blocked, and the closeout
  blockers extended.
- `docs/verification/RFC-BPR-0005.md` — the native-acceptance row and the
  deferred-gates section record that attendance was not the binding constraint.
- `BPR-0114` — dependencies re-pointed at BPR-0183 and BPR-0184, with the
  reason it was misfiled and the instruction not to accept a native-palette
  demonstration in place of the claim.
- `BPR-0100` — dependencies re-pointed at BPR-0186 and BPR-0187.
- `BPR-0115` — dependencies extended by the six new cards, with the instruction
  to replay against the composed application and to ask the linker.
