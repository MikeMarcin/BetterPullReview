---
schema: "implement-rfc.card.v1"
id: "BPR-0188"
title: "Plan RFC-BPR-0005's composition and reconcile the two windows"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0097","BPR-0180"]
---

# Plan RFC-BPR-0005's composition and reconcile the two windows

BPR-0180 found that RFC-BPR-0006 composes nothing. Asking the same question of
every other RFC found that the gap starts one RFC earlier and never closes.

## The pack-wide measurement

`nm build/dev/src/app/better_pull_review | c++filt | grep -cF <name>`, against
the `dev` build at this card's parent commit:

| Application host | RFC | Symbols |
|---|---|---|
| `github_application_host` | 0002 | 4158 |
| `epoch_application_host` | 0003 | 10633 |
| `diff_application_host` | 0004 | 954 |
| `workbench_application_host` | 0005 | **0** |
| `appearance_application_host` | 0006 | **0** |
| `review_ledger_application_host` | 0007 | **0** |
| `review_mutation_coordinator` | 0008 | **0** |
| `review_synchronization` | 0008 | **0** |
| `defect_correlation` | 0009 | **0** |
| `review_queue_refresh` | 0010 | **0** |
| `review_queue_enrollment` | 0010 | **0** |
| `review_queue_notification` | 0010 | **0** |

Three of twelve. **RFC-BPR-0002, 0003, and 0004 reach a user. Nothing from
RFC-BPR-0005 onward does.**

The Qt side agrees. `github_window` 226, `diff_workbench` 596,
`epoch_comparison_control` 212 — the virtualized review *surface* is real.
`workbench_window` **0**, `workbench_drawer` **0**, `review_queue_model` **0**,
`review_landing_model` **0**, `defect_card_view` **0**, `review_draft_editor`
**0**.

## Why it stayed invisible

Every one of those components is tested, and every assertion was proven
non-vacuous by breaking the behaviour and observing the failure.

Non-vacuity establishes that a test constrains its subject. It cannot establish
that the subject is reachable from the product, and no break test on an
uncomposed component ever could — breaking `workbench_window` fails
`qt_workbench_window` exactly as designed whether or not any application shows
that window.

Two habits turned that blind spot into six RFCs of undelivered work:

- **A port satisfied by a fake never asks for an adapter.** Each host takes its
  dependencies as ports. Tests supply fakes. Nothing anywhere demanded a real
  one, so for several RFCs no adapter was ever written — not deferred, not
  carded, simply never required by anything.
- **Ledger rows named surfaces they were not proven against.** Corrected below.

## Ledger correction for RFC-BPR-0005

Five rows name **Real application**, **Instrumented application**, or
**Rendered composition**. Every one is proven by `qt_workbench_window`, an
adapter test that constructs `qt::workbench_window` directly — a window the
shipped application does not contain.

That includes the virtualization figure. "60/6,000/60,000 lines produce
68/68/68 window widgets" is a true measurement of a window no user can open.

The rows stay `Verified` **as component claims**, restated to name the harness
that proves them, exactly as BPR-0180 did for RFC-BPR-0006. None may claim the
application until BPR-0191 lands.

## The decision: which window is the application's window

This is the real design question, and it is not a wiring detail.

`github_window` and `workbench_window` **both** compose `diff_workbench` and
`epoch_comparison_control`. They are two windows that each believe they own the
review surface, and the application shows `github_window` — the one that
predates RFC-BPR-0005.

What each holds that the other does not:

| `github_window` (RFC-BPR-0002) | `workbench_window` (RFC-BPR-0005) |
|---|---|
| host profile selector, owner/repository/pull-request locator | primary navigation toolbar and compact overflow |
| connect, open, refresh actions; status banner | files and conversations drawers |
| threaded conversation view (`QTreeView`) | full-width line-over-line detail pane |
| | declared deterministic focus order, accessible names |

**Decision: `workbench_window` becomes the application's window.**

The alternative — `github_window` composing `workbench_window` as its central
widget — is less invasive and is wrong. RFC-BPR-0005 is titled *Native
Workbench Shell*. If RFC-BPR-0002's window stays the shell and merely contains
the workbench, then the shell RFC-BPR-0005 specified never shipped, and its
toolbar, overflow behaviour, drawer layout, focus order, and accessible-name
work stay unreachable while the ledger reports them verified. That is the state
this card exists to end, not to formalise.

The two surfaces are complementary rather than duplicated, which is what makes
this tractable. `workbench_drawer_model` holds identity-and-detail rows — a
navigation list. `github_conversation_model` holds the threaded content. The
drawer lists the conversations; the thread shows one. They were built to fit
and were never put together.

So RFC-BPR-0002's session chrome — profile, locator, connect/open/refresh,
status — becomes a composed region inside the shell rather than a window of its
own, and its conversation view moves into the conversations drawer region.
RFC-BPR-0002 keeps sole ownership of session identity, credentials, and
conversation content; it stops owning the window.

## What must not happen

Do not let both windows survive with the review surface composed twice. Two
owners of one surface is how this started.

Do not weaken `github_window`'s existing evidence to make the move easier. Its
accessible names, unique key sequences, and command-projection behaviour are
load-bearing and were each forced by a real defect — BPR-0097 found three live
accessible-name collisions and an ambiguous shortcut that silently disabled
both bindings. Those tests move with the code.

## Cards allocated

| Card | Slice |
|---|---|
| BPR-0189 | Adapters for `workbench_settings_port` and `workbench_observation_port`, neither of which exists |
| BPR-0190 | Compose `workbench_application_host` into the application |
| BPR-0191 | The shell becomes the application's window; the session chrome and conversation view move into it |

BPR-0186 and BPR-0187, allocated by BPR-0180, also belong to RFC-BPR-0005 and
follow BPR-0191: an attended launch is only worth holding open once the window
it holds open is the one the RFC specifies.

## Required direct evidence

- The measurement table is reproduced with the command that produces it.
- RFC-BPR-0005's ledger names, for every row it still calls `Verified`, the
  harness that proves it, and no row claims the application.
- The window decision is recorded with its reasoning, including why the
  cheaper alternative was refused.
- `verify_documentation.py` and the documentation issue-graph contract pass
  with the new cards.

## Evidence

Documentation only; this card changes no code and needs no build.

- `python3 scripts/verify_documentation.py --root .` → `DOCUMENTATION_OK`.
- `ctest --preset dev -R documentation` → 2/2.
- `git diff --check` clean.

The measurements are recorded with the commands that produce them, and they are
expected to change: the zeros for `workbench_application_host` and
`workbench_window` are exactly what BPR-0190 and BPR-0191 must make non-zero.

## What was corrected elsewhere

- `docs/verification/RFC-BPR-0005.md` — status restated, a `Composition`
  section added ahead of the claim matrix, all five overstated `Direct surface`
  entries restated to name the `qt_workbench_window` harness, and the
  virtualization figure annotated as measured on a window the application does
  not contain.
- The same file's architecture allocation is now read back against itself. It
  names a fourth region, **Composition root**, responsible for "wiring the
  workbench into the real application" with "selected public-path evidence
  through the real executable" as its planned evidence. The claim matrix has
  rows for the core, the host, and the adapter, and **no row for the
  composition root**. The plan named this gap before the work started; the
  verification table had no line for it, so nothing ever asked whether it had
  been done.
- `BPR-0101` — dependencies extended by the three composition cards, with the
  instruction to replay against the composed application and to ask the linker.
- `BPR-0186` — now follows BPR-0191, because holding a window open for a person
  is only worth doing once it is the window the RFC specifies.

That missing matrix row is the most reusable finding here. A verification table
built by walking the architecture allocation region by region would have had an
empty composition-root row from the first day, and an empty row is visible in a
way that an absent one is not.
