---
schema: "implement-rfc.card.v1"
id: "BPR-0982"
title: "Reconcile the CLI self-test courses and composition contract with discovered GitHub CLI accounts"
status: "In Progress"
rfcs: ["RFC-BPR-0015"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"RFC-BPR-0015 r3 closeout focused checks, 2026-09-22","found_at":"2218ef1f211e689ed41444a1832326e0c458c421","regressed_by":"1210eecc (BPR-0909)","milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# BPR-0982: Reconcile the CLI self-test courses and composition contract with discovered GitHub CLI accounts

## Observation

[[BPR-0909]] (1210eecc, 2026-09-18) made GitHub CLI a discovered way in: no
card carries a precomposed GitHub CLI profile any more, and the CLI is reached
only through Use GitHub CLI, which composition wires only where it offers the
read. The owner declined the heavy gate for that private patch build, so three
RFC-BPR-0015 guards written for the precomposed rows were not rerun. At the
unchanged `2218ef1f` baseline, before any RFC-0015 r3 change, all of these fail
in `build/dev`:

- Every non-fixture `--self-test` public path row stops at stage 0 with
  `CLI_UNAVAILABLE_FAILED` / `CLI_UNAVAILABLE_COUNT:0`, because its first act is
  to find and activate a `GitHub CLI (unavailable)` row that no longer exists:
  `application_public_path`, `application_public_path_ghes_cli_unavailable`,
  `application_device_authorization_public_path`,
  `application_device_authorization_release_contract`,
  `application_anonymous_public_preview_unavailable`,
  `application_anonymous_public_preview_release_contract` and
  `application_anonymous_public_preview_enabled_probe`.
- `application_composition_capabilities_inside_boundary` and
  `application_composition_capabilities_product_run` print the right browser
  and `COMPOSITION_CLI_READ` answers, then an empty `COMPOSITION_CLI_LABEL:`,
  because the probe reports the first card's CLI profile label.
- `application_composition_contract`: the `cli_read_offered_here` pattern no
  longer matches the as-built definition (BPR-0909 added the fixture-only
  `GithubCliDiscoveryAccounts.has_value() ||` in the tests-enabled macOS
  branch), its `boundary-answer-dropped` hostile control no longer changes the
  source, the tests-disabled Release binary no longer carries the UTF-8
  `gh sign-in` label, and the tests-disabled `--self-test` cannot find the CLI
  row whose read opt-in it used to decline.

The same method in that contract also fails for reasons outside RFC-0015; they
are recorded on [[BPR-0983]] and are not this card's.

## Acceptance

- AC1: The self-test CLI course asserts the discovered model directly: before
  any gesture no server card carries a GitHub CLI profile or offer, the
  account is untouched, and the Use GitHub CLI entry is offered exactly when
  the composed run offers the CLI read. It presses nothing, because pressing
  Use GitHub CLI launches `gh`. The retired row-activation helpers go.
- AC2: The capability probe reports whether the Use GitHub CLI entry is
  offered instead of a precomposed label; the inside-boundary row expects
  `absent`, the macOS product row `offered`, other platforms `absent`.
- AC3: The composition contract matches the as-built `cli_read_offered_here`
  and still refuses a dropped boundary answer, an offer opened inside the
  boundary, a retitled read warning and a read answer supplied for the
  reviewer. The tests-disabled Release self-test on macOS reports the entry
  offered and no CLI row; the obsolete `gh sign-in` byte marker is retired.
- AC4: The seven public path rows, both capability rows and the RFC-0015 parts
  of `application_composition_contract` pass in `build/dev`; the round-trip
  composition pins are resealed with this review recorded.

## Boundaries

No product behavior changes: the course, probe and contract follow the owner
accepted BPR-0909 model. No `gh` process is launched by any course. The consent
itself is exercised by the tests-enabled discovery courses in
`application_setup_destination_public_path` and by native tests-disabled
observation, not here.
