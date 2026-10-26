---
schema: "implement-rfc.card.v1"
id: "BPR-1769"
title: "Keep corporate Claude wrapper settings through setup and reset, and expose diagnostics"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner report 2026-10-26: process wrapper fails first-run setup and Show details is missing; owner confirmed Amazon Bedrock with corporate SSO, possibly Okta/VPN (unconfirmed)","reservation":"docs/issues/_reservations/BPR-1769--BPR-1769--8e61db489223.json","related_cards":["BPR-1301","BPR-1749","BPR-1753","BPR-1738"],"frontend_design":"frontend-design:frontend-design applied; preserve native font, palette and existing layout","milestone_plan":{"id":"bpr-1769-wrapper-setup","path":"docs/rfcs/plans/bpr-1769-wrapper-setup/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"69d7447c4579c89414fa41f7f84bb9b5066f539d","evidence":{"kind":"command","command":"application_agent_setup_host; qt_claude_process_wrapper","detail":"Saved effective program/fixed arguments for auto-chosen migrated preset; sanitized pretty Bedrock status through actual supervisor; unsuppressed banner unreadable; mutation negative control fails before correction."}},{"criterion":"AC2","outcome":"pass","revision":"69d7447c4579c89414fa41f7f84bb9b5066f539d","evidence":{"kind":"command","command":"qt_agent_setup_view; application_agent_invocation_composition_contract","detail":"Keyboard-operated separate bounded literal read-only details and workspace log; clearing and retained expansion; custom availability/Ready text; contextual rescan controls; settled public window and light/dark/busy render inspection."}},{"criterion":"AC3","outcome":"pass","revision":"69d7447c4579c89414fa41f7f84bb9b5066f539d","evidence":{"kind":"command","command":"8 focused CTest rows across final targeted runs; clang-format22; 4-TU clang-tidy; documentation gate and 67 script tests","detail":"All scoped gates pass; explicit pre-existing comment-only architecture fix; exact source/manifest pins and unchanged mutation branch; final settled composed pass31.44s."}},{"criterion":"AC4","outcome":"pass","revision":"69d7447c4579c89414fa41f7f84bb9b5066f539d","evidence":{"kind":"command","command":"application_agent_setup_host; application_agent_invocation_composition_contract","detail":"Reset invalidates stale discovery and loaded reset settings defeat legacy wrapper; restart regression passes. Real Reset confirmation clears SQLite canonical/legacy wrapper/model while retaining MarkRead; prior notes/conversations reset logic unchanged."}},{"criterion":"AC5","outcome":"pass","revision":"69d7447c4579c89414fa41f7f84bb9b5066f539d","evidence":{"kind":"command","command":"application_agent_invocation_host; qt_agent_setup_view; application_agent_invocation_composition_contract","detail":"Independent held agent/Git completions retain progress/disable retries; failed-check private-copy action starts check directly; Stop token settles workspace before retry; reset generation prevents stale completion."}}]
---

# BPR-1769: Wrapper setup and check details

## Mandate

Repair the first-run regression reported on 2026-10-26. The owner's Claude
wrapper uses Amazon Bedrock and corporate SSO. Okta and VPN requirements are
uncertain. Do not require normal Claude account sign-in for this wrapper or
claim its external credentials were validated. Preserve the prior acceptance
of RFC-BPR-0092 and BPR-1301; this is a new follow-up.

## Acceptance criteria

- AC1: Setup checks the effective saved wrapper program and fixed arguments,
  including an auto-chosen migrated preset with no preferred entry saved yet.
  The owner's sanitized pretty JSON (`loggedIn: true`, third-party Bedrock)
  traverses the real supervisor and strict decoder. Unreadable output remains
  a failure; custom agents retain their own check behavior. A successful check
  saves the exact checked entry for subsequent requests.
- AC2: Each failed setup row exposes a keyboard-accessible, literal, bounded
  diagnostic disclosure, with separate agent and workspace output. Rerunning
  a check clears old details. No output is automatically logged or copied.
  Custom program checks and their Ready text say availability, not sign-in.
  Rescans belong to Agent and Repository, with specific scope and no generic
  discovery footer on Check it works.
- AC4: Reset agents forgets the canonical entries and legacy wrapper/model
  settings, so the old wrapper cannot return without its cleared arguments,
  now or after restart. Unrelated preferences, notes and conversations stay.
- AC5: Check it works stays visibly busy until both independent agent and
  workspace checks settle, prevents duplicate retries, and Stop reaches the
  workspace token. A private-copy fallback starts its retry directly after
  that choice is accepted; old/cancelled results cannot overwrite reset.
- AC3: Focused host, real-process wrapper, Qt and composed public-path checks
  demonstrate the repair, including a failing-before regression and offscreen
  rendered diagnostics. Formatting and documentation gates pass. Record the
  absence of live Bedrock/SSO and other-platform validation.

## Design

Keep the existing native palette and font. The setup retains its sequence and
primary Check again action; a quiet Show details / Show the log disclosure
sits immediately under the corresponding failure. The disclosure opens a
read-only plain-text area. A wrapper row says its availability was checked,
with sign-in reported by that wrapper for its configured provider. No new
sign-in switch, model request or environment widening.

## Diagnosis amendment (owner-manual-status-r3)

The owner ran their wrapper with its `--no-extra-output` fixed argument and
`auth status --json`: exit 0, `loggedIn: true`, `authMethod: third_party`,
`apiProvider: bedrock`, with pretty JSON. That output is supported. The
initial version-only availability proposal is withdrawn: retain the RFC's
strict status check. Regression tests exposed an auto-chosen migrated preset
with `PreferredAgent=0` losing its saved arguments. The owner then reported
Reset agents retaining their wrapper: reset cleared the new list/arguments,
but detection and the fallback launcher still consulted the legacy wrapper.
The canonical migrated list must be authoritative; reset clears the legacy
wrapper/model fields too. This new evidence refines the original repair,
without changing historical Done acceptance or asserting the owner's exact
runtime cause before their repaired build is observed.


## Owner follow-ups (owner-setup-regressions-r4)

The owner reports that Custom agent passes its first row but still fails the
workspace; that row only checks program availability unless given a check of
its own, so it must not say signed in. The owner also reports that the private
copy fallback requires another Check again and still fails, that progress is
unclear, and that the pinned Look again button has no clear step context.
The fallback now retries directly; visible progress covers both operations;
rescans are step-specific. A private copy can still require the same download.
The corporate workspace error remains unknown until its current log is read;
this slice repairs diagnostics and control behavior, not that unseen failure.


## Verification and execution record

Implemented on macOS, 2026-10-26. The exact source candidate and commands are
recorded in acceptance metadata. The source-only milestone is complete; no
package was installed, released or pushed.

- AC1: `application_agent_setup_host` checks a migrated auto-chosen preset
  with no preferred entry, the exact program and `--no-extra-output` before
  both status/version commands, the sanitized pretty third-party Bedrock
  response, exact saved selection and an updated program. A source mutation
  disabling the effective-entry fallback fails that argument assertion; the
  corrected source passes. `qt_claude_process_wrapper` runs the pretty JSON
  through the real supervisor/plain-line capture and strict decoder. An
  unsuppressed fixture banner remains unreadable. This demonstrates the
  failure mode without claiming the owner's actual wrapper emitted a banner.
- AC2: `qt_agent_setup_view` opens each disclosure through keyboard/button
  input and checks literal read-only text, separate logs, expansion retention
  and clearing. It checks custom availability and Ready wording, Agent-only
  and Repository-only rescans, and their absence on Check. The full composed
  window checks the two actual diagnostic outputs and settled minimum editor
  and wrapped-label geometry. Offscreen light/dark and full-window renders
  were inspected; no native-style or owner-desktop claim is made.
- AC3: All eight scoped tests pass across focused final runs:
  `architecture_boundaries`, `architecture_boundary_contract`,
  `application_agent_invocation_host`, `application_agent_setup_host`,
  `qt_agent_setup_view`, `qt_claude_process_wrapper`,
  `application_agent_invocation_composition_contract`, and
  `application_review_round_trip_composition_contract`. The composition
  course enters Review and opens the real Agent drawer before discovering
  agents, uses the real SQLite settings and supervised local fixtures, then
  reaches Ready and exercises independent failures. Earlier harness failures
  (hidden drawer, hostile fixture basename and unsettled screenshot) are
  retained in the external checkpoint, corrected, and narrowly rechecked.
  clang-format 22.1.8, focused clang-tidy on all four changed production
  translation units, documentation and 67 documentation-script tests pass.
  The roundtrip full/manifest pins are resealed; mutation bytes are unchanged.
  One pre-existing CMake comment used a forbidden gate word; replacing it
  with “measurements” is the only incidental edit and makes that gate pass.
- AC4: Reset invalidates found rows and pending discovery, the canonical list
  overrides the legacy source, and startup refreshes an early detection from
  loaded settings. A new host over persisted reset settings with the old
  legacy source cannot resurrect it. The composed Reset confirmation clears
  canonical entries and the durable legacy wrapper/model fields while keeping
  an unrelated MarkRead preference. Existing notes/conversations reset code
  is unchanged.
- AC5: Manual-lane probe tests prove cancellation and cleanup. Qt tests cover
  each independent busy order. The composed course holds the agent and Git
  fixtures separately: both busy, workspace finished/agent still busy, then
  direct private-copy retry with agent finished/workspace still busy. Stop
  settles that probe before another retry is offered. A later check reaches
  Ready, and two new failures expose separate current diagnostics. No model
  request or GitHub publication occurs.

Limits: the owner's corporate workspace failure has not been diagnosed or
reproduced against the private repository. Bedrock SSO, possible Okta/VPN and
an actual agent question were not exercised. Windows/Linux native runs and
installed-package behavior are unclaimed. The new log is how the underlying
workspace failure can be identified on that machine; it is not presumed fixed.
