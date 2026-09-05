---
schema: "implement-rfc.card.v1"
id: "BPR-0265"
title: "Restore the diff-interaction public path after compact navigation"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0264"]
---

# Restore the diff-interaction public path after compact navigation

The complete macOS `dev` continuation after BPR-0264 exposed a shared public-
path regression. Diff evidence reaches the selected-ribbon frame but rejects
the following divider-resize invariant. The compact-navigation surface was
accepted through its focused and attended matrix, but this older unattended
course no longer provides passing evidence against the current composition.

## Required final state

- The diff-interaction course performs a real divider resize, observes the
  exact resulting geometry transition without relying on a stale splitter
  ratio or stale capture files, and completes its start/end/retention checks.
- Failures identify the exact violated splitter, geometry, selection, or
  capture predicate instead of reporting only the outer public-path stage.
- The appearance courses that consume this same diff/render evidence again
  complete once the shared interaction course is repaired.
- The affected tests pass repeatedly in isolation and inside the remaining
  macOS `dev` sequence; the repair does not reintroduce duplicate comparison
  controls or bypass the application/core decision path.

## Non-goals

- Do not revise the accepted compact-navigation product design or its attended
  visual verdict.
- Do not relax network isolation, execute a credentialed/live target, or
  manufacture replacement screenshots as proof.
- Do not claim Windows or Linux runtime evidence.

## Verification

- Rebuild the development application and affected Qt targets.
- Run the diff-workbench, appearance gesture, appearance customization, and
  appearance-store public-path selectors.
- Run their nearest Qt/application component tests, the architecture boundary
  pair, repository format/tidy, documentation, and diff gates.
- Review the final public-path and diagnostic changes independently.

## Closeout

The evidence course now derives a legal changed splitter target from the live
range, boundedly awaits one viewport-attributed rebuild, and preserves exact
selection, context, and semantic-index state through nineteen current frames.
Before each attempt it scans all twenty-one fixed output names, attempts every
stale removal, and reports one aggregate failure if any remains. Every caller-
side failure path emits exactly one fixed, bounded predicate marker; no path,
filename, provider value, source text, or credential enters the marker.

The real diff public path passed repeatedly. Theme customization, appearance
gesture, and obstructed-store courses passed after the separate epoch-refresh
repair. The 100/125/200 percent Qt workbench selectors, architecture pair,
repository format/tidy, documentation, diff, and production source-manifest
gates passed. Independent review first rejected incomplete diagnostic coverage;
after repair it returned `ACCEPT` and directly proved all fixed removals were
attempted while an unrelated sentinel remained.
