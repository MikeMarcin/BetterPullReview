---
id: BPR-0225
title: Review the Claude-era final state and reconcile outstanding work
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# Review the Claude-era final state and reconcile outstanding work

Review the repository state produced after the last non-Claude commit, with
the final executable, final tests, and live board taking precedence over the
story told by intermediate commits.

## Durable review anchor

- Objective: establish what is actually implemented and publicly reachable,
  identify proof gaps and overclaims, repair tracker and documentation truth,
  and leave every material remaining slice on the live board.
- Review base: `11fdce08903f735387194462d7ea1bd52a892189`.
  This is the latest commit without a Claude co-author trailer; the next commit
  begins the uninterrupted Claude-coauthored range.
- Review head at intake: `2f2074a3dd15980c9ff67c82ea6dbafcd81ecda2`.
- Intake size: 216 commits, 560 changed files, 85,710 insertions and 3,351
  deletions.
- Affected RFC set: RFC-BPR-0003 through RFC-BPR-0011, plus the
  RFC-BPR-0013 reliability cards discovered while exercising them. This card
  does not change RFC status or closure order.

## Planned issue slices

- BPR-0226: connect the workbench host to the shipped window and accepted
  review.
- BPR-0227: drive selected-file diff requests from the public workbench.
- BPR-0228: compose review-ledger actionability into the shipped workbench.
- BPR-0229: compose review synchronization and the mutation coordinator.
- BPR-0230: compose defect correlation and defect cards.
- BPR-0231: compose an honest queue-enrollment input and landing surface.
- BPR-0232: compose explicit agent invocation into the shipped workbench.
- BPR-0233: exercise the recorded UIforETW diff and intraline pixels through
  the selected public path.
- BPR-0234: add immutable RFC bases and executable acceptance checklists to
  every open RFC closeout card.
- Existing BPR-0220 retains the native appearance rebuild-attribution
  question. Existing BPR-0223 retains notification delivery and activation
  after BPR-0231 supplies real queue items.

## Phase checklist

- [x] Planning and issue allocation committed.
- [x] Delegated reorientation inventory reconciled.
- [x] Final-state public-composition audit complete.
- [x] Verification-honesty audit complete.
- [x] Every blocking finding reproduced by the integrator.
- [x] Board, RFC ledgers, and affected cards state the final truth.
- [x] Focused final-state gates rerun at the reviewed head.
- [x] Conservative simplification review complete.
- [x] Documentation gate passes over the review range.
- [ ] Independent critical review complete.
- [x] Retrospective report complete.

## Verification matrix

| Claim | Direct surface | Command or interaction | What proves it | Result | Evidence | Commit |
|---|---|---|---|---|---|---|
| The workbench is reachable, not merely constructed | Shipped composition source and public trace | Trace window callbacks, host observation, projection sink, and one user action | Nonempty intents reach the host, accepted review facts are observed, and the window receives the projection | Not satisfied. The callbacks are empty, projections terminate in a non-rendering sink, accepted handoff reaches only the epoch host, and settings-store failure prevents host construction | BPR-0226 | `0588efa` |
| A selected file can request a real diff | Shipped executable without fixture-access helpers | Open a fixture PR, select a file through the window, and observe the exact diff request/result | The trace contains no direct `request_fixture_diff` call and the selected file drives the request | Not satisfied. The fixture-access helper is the only caller of the composition's diff request | BPR-0227 | `0588efa` |
| Later RFC features are honestly described | Composition root plus verification ledgers | Inventory constructed and invoked RFC-BPR-0007 through RFC-BPR-0011 paths | Every ledger distinguishes component, linked, constructed, invoked, and user-reachable state | Reconciled. Component evidence is retained; absent product composition is explicit | BPR-0228 through BPR-0232 and the RFC ledgers | `d838613` |
| The recorded fixture proves rendered intraline behavior | UIforETW selected-diff public path | Run the recorded fixture with the selected-diff trace and inspect semantic ranges and role pixels | A meaningful comparison, intraline ranges, and affected profile pixels are asserted before attendance | Not satisfied. The fixture is served, but its registered path opens the PR and saves a grab without selecting or asserting a diff | BPR-0233 | `0588efa` |
| Repaired false-green gates honor exit failure | CTest registrations and tests-disabled Release graph | Run `application_composition_contract` and `live_mutation_isolation_contract`; inspect generated metadata | No pass regex overrides the suite exit code and the Release graph reaches its seam assertions | Verified by source/generated-metadata inspection and focused passing runs; no false-green override remains on either repaired test | BPR-0224 | `2f2074a` |
| The live tracker names every nonterminal slice | `docs/issues/` folders and front matter | Inventory lanes, dependencies, `NEXT_ID`, and board narrative | Counts, folder state, front matter, and dependency claims agree | Verified at the pre-close state: 28 backlog, 3 ready, this one in progress, `NEXT_ID` BPR-0235, and `DOCUMENTATION_OK` | This card and BPR-0234 | `6470199` |

## Documentation ledger

| Scope | Required review | Disposition | Evidence | Validation | Claims left open |
|---|---|---|---|---|---|
| Public workbench and diff composition | README, system architecture, RFC-BPR-0004/0005 ledgers | Reconciled; product gap remains open | BPR-0226, BPR-0227 | Public composition trace | User-driven workbench and diff path |
| Review ledger, mutations, defects, queue, and agents | RFC-BPR-0007 through RFC-BPR-0011 ledgers and owning architecture docs | Reconciled; product gaps remain open | BPR-0228 through BPR-0232 | Composition inventory and public traces | User-reachable later-RFC workflows |
| Appearance and recorded fixture evidence | RFC-BPR-0006 ledger, BPR-0114, BPR-0214, fixture catalog | Reconciled; obsolete attended packet invalidated | BPR-0220, BPR-0233 | Native probe and recorded selected-diff path | Foreground/native and attended verdicts |
| Issue-board final state | Board index and affected cards | Reconciled | This card | `verify_documentation.py` plus lane inventory | External-authority and target-native blockers remain open |
| RFC closeout rigor | Open closeout cards and their verification ledgers | Gap recorded | BPR-0234 | Exact immutable bases and executable acceptance checklists | No RFC may close from narrative-only evidence |

## Review constraints

- No live GitHub mutation is part of this review.
- Existing Windows, owner-attended, live-repository, and signing blockers remain
  real; available-platform evidence cannot replace them.
- Intermediate history is provenance and diagnostic evidence. It is not a
  substitute for what the final application constructs, invokes, and renders.

## Final verification

- `cmake --preset dev` and `cmake --build --preset dev` pass; the build had no
  work to do. `ctest --preset dev --output-on-failure` passes 143/143 in
  886.14 seconds.
- `cmake --preset core-only` and `cmake --build --preset core-only` pass; the
  build had no work to do. `ctest --preset core-only --output-on-failure`
  passes 67/67 in 91.20 seconds.
- `cmake --build --preset dev --target coding-format-check coding-tidy` passes
  the 329-file formatting check and repository-wide strict tidy. Reported
  warnings were suppressed non-user/system-header diagnostics; no project
  diagnostic survived the warnings-as-errors gate.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`, and `git diff --check` passes.
- `python3 scripts/verify_composed_hosts.py
  build/dev/src/app/better_pull_review` reports 11 composed symbols and the 7
  deliberately absent later-RFC symbols. Source/caller inspection supplies
  the required construction, invocation, projection, and gesture distinction
  that this symbol test explicitly cannot prove.
- The delegation audit reports 41 launches across four Claude sessions. The
  evaluation itself used one reorientation utility, two semantic scouts, two
  clean-matrix utilities, and has one independent critical closeout review
  still pending. One initially broad utility was interrupted and successfully
  restarted with a narrower inventory question.

No live GitHub mutation, Windows x64 run, regenerated attended verdict, or
native appearance change is claimed. Their live cards remain the authority.

## Retrospective

### What went well

- The Claude-era work produced substantial deterministic core, application,
  adapter, fixture, and Qt component behavior. The Qt-free matrix still passes
  67/67, and the binary contains the intended GitHub, epoch, diff, workbench,
  appearance, syntax, and notification building blocks.
- The GitHub-to-epoch path is genuinely composed, and component tests exercise
  meaningful identity, cancellation, persistence, rendering, and failure
  contracts rather than only construction.
- BPR-0224's false-green repairs are real: the two affected `unittest` gates
  now honor process failure. Recorded-fixture provenance, credential-shaped
  content rejection, regeneration drift, and exact blob counts are also direct
  checks.
- Attended runs found and closed defects that automated success markers had
  missed. External-authority, Windows, and owner-attended gates stayed open;
  no later RFC was incorrectly marked Auto-Verified.
- The Claude sessions delegated substantive work: the repository audit found
  41 launches across four sessions, including 20 RFC workers, 6 utilities,
  6 scouts, and one critical reviewer.

### What went wrong

- Component completion was repeatedly described as product completion. The
  final application leaves the workbench callbacks empty, publishes into a
  non-rendering intermediate sink, and never hands accepted review facts to
  the workbench host.
- The test named `application_diff_workbench_public_path` injects its selected
  diff through a test-only fixture helper. An earlier critical review accepted
  that as a genuine public path, so both naming and review failed to inspect
  the final caller graph.
- RFC-BPR-0007 through RFC-BPR-0011 have broad component implementations but
  no shipped ledger, review mutation/synchronization, defect, queue-enrollment,
  or agent-invocation workflow. Notification objects exist, but nothing asks
  them to deliver and activation selects nothing.
- `RENDER_OK` proves that a window grab was saved, not that the comparison or
  intraline emphasis is present. The UIforETW recording is served but never
  selected as a diff in its registered path.
- Tracker counts, BPR-0187's remaining role, ledger language, and the attended
  packet drifted from final HEAD. The five open closeout cards also lack the
  immutable base and executable acceptance checklist expected at closure.

### What to do next

1. Land BPR-0226, then split immediately into BPR-0227's real selected-diff
   gesture and BPR-0228's ledger-derived actionability.
2. From BPR-0228, compose the round trip (BPR-0229), defects (BPR-0230), and an
   honest queue input/landing surface (BPR-0231); only then complete
   notification delivery/activation in BPR-0223. BPR-0232 can proceed after
   the workbench shell is genuinely wired.
3. Run BPR-0233 through the real BPR-0227 path, prove semantic intraline ranges
   and profile-role pixels, then regenerate the attended packet and resolve
   BPR-0220's native geometry delta before seeking visual approval.
4. Complete BPR-0234 before any RFC closeout and keep the existing Windows,
   live-GitHub, owner-attended, and canonical-order blockers explicit.
