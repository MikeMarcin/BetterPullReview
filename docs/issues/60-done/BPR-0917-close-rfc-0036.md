---
schema: "implement-rfc.card.v1"
id: "BPR-0917"
title: "Verify and close RFC-BPR-0036"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-0916"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0036"]
metadata: {"milestone_plan":{"id":"rfc0036-native-markdown","path":"docs/rfcs/plans/rfc0036-native-markdown/milestone-plan.json"}}
acceptance: [{"criterion":"AC-01","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"Actual Home Open gesture through real read host and Qt workbench; six full public body/reply kinds; native headings/lists/tasks/quotes/code/tables/links at scales 1.0, 1.25 and 2.0. Full evidence: docs/verification/RFC-BPR-0036.md."}},{"criterion":"AC-02","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"Exact CRLF/tab/Unicode copy MIME bytes, accepted snapshot source and original body identity retained. Source/malformed/empty/explained fallback pass; application_setup_destination_public_path retains draft/source with zero publication. Full evidence: docs/verification/RFC-BPR-0036.md."}},{"criterion":"AC-03","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"Resource-negative-control component corpus passes. Core/host link policy admits explicit HTTP(S), exact-head repository paths and identical image gestures; rejects local/executable/userinfo/credential-query destinations, absent capability/body and stale window/revision. Rendering makes no browser effect. Full evidence: docs/verification/RFC-BPR-0036.md."}},{"criterion":"AC-04","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"Native Cocoa keyboard document end proves final cursor and visibility, source selection and post-to-reader Tab. Four appearance palettes, accessible text interfaces and three scale selectors pass. Final wide-code/table captures inspected; local horizontal reading reaches END OF CODE and END OF TABLE. Full evidence: docs/verification/RFC-BPR-0036.md."}},{"criterion":"AC-05","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"Native 50,742-byte/650-paragraph body completes in 32 ms; 1,027-byte pathological source fallback in 14 ms. Identical refresh retains document/selection/scroll; body/context replacement clears before new data; stale completion/window/publication and sibling-preview isolation checks pass. Full evidence: docs/verification/RFC-BPR-0036.md."}},{"criterion":"AC-06","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"Uncapped expanded overview reaches its exact final paragraph at narrow and wider allocations; Return/Escape, file focus, directed selection and source anchors preserved. docs/architecture/native-markdown.md records subset, bounds, source fallback and image/link policy. Full evidence: docs/verification/RFC-BPR-0036.md."}},{"criterion":"Technical closeout and canonical status","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"BPR-0916 is Done. Focused integration 14/14, final affected 8/8, final composed 3/3, native Cocoa and core navigation/boundary checks pass. Both documentation gates and scoped board validation pass. Broader established failures remain tracked by BPR-0940; RFC/index stay Proposed because RFC-BPR-0008 and canonical predecessor eligibility are pending. Full evidence: docs/verification/RFC-BPR-0036.md."}}]
---

# Verify and close RFC-BPR-0036

RFC base: `7ad0e43eb92c217ac6cd01fc478bde2968d217a0`.
Planned cards: BPR-0916 and BPR-0917. Related delivered geometry: [[BPR-0918]].

Technical implementation and AC-01–AC-06 are accepted at source
`a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f`. The RFC remains **Proposed** because
RFC-BPR-0008 and canonical predecessors are not all Auto-Verified. This card
records technical reconciliation; it does not bypass the RFC sequencing gate.

## Closeout checklist

| Criterion | Direct check and producing candidate | Result |
|---|---|---|
| AC-01: Overview and public conversations render the declared Markdown subset through the shipped composition; fixtures cover headings, nested lists, tasks, quotes, code, tables and links. | Actual Open/read-host/Qt course, six full public body/reply types and native document structures at `a3ab46ee` | Pass |
| AC-02: Source, copy and preview preserve original body bytes and publication payloads; empty, malformed and unsupported input stays readable with explicit fallback. | Exact CRLF/tab/Unicode MIME bytes, accepted snapshot source, malformed/empty/fallback and zero-publication retained-draft course at `a3ab46ee` | Pass |
| AC-03: Hostile HTML, local-file URLs, executable schemes and image/resource references execute nothing and cause no automatic filesystem or network access; admitted link gestures traverse the host-owned path. | Resource negative control, direct/image admission, credential/local/executable refusals and exact-head host/browser path at `a3ab46ee` | Pass |
| AC-04: Keyboard reading, selection, links and source switching work with appropriate accessibility, light/dark/high-contrast appearance and representative display scales; wide blocks remain usable. | Native Cocoa input/text accessibility, four palettes, three scales, post-to-reader Tab and readable local wide overflow at `a3ab46ee` | Pass on available macOS |
| AC-05: Declared bounds and measured long/pathological-body behavior keep the UI responsive; unchanged refresh preserves position and stale completions cannot replace another body's content. | 50,742-byte layout 32 ms; pathological source fallback 14 ms; unchanged selection/document/scroll, stale window/body/publication and shutdown checks at `a3ab46ee` | Pass |
| AC-06: Composed long-description reading reaches the final content without a fixed six-line viewport constraint; documentation describes the supported subset, fallback, limits and image policy. | Exact final cursor/visible paragraph at two reader widths, preserved source anchors/Return/Escape/file focus, complete reading contract at `a3ab46ee` | Pass |

The [verification ledger](../../verification/RFC-BPR-0036.md) gives exact commands,
retained failures/corrections, platform limits, inspected captures and binary
hashes. The [reading contract](../../architecture/native-markdown.md) includes the
owner-requested md4qt/native-Qt comparison and the resulting native-Qt decision.
The final source received a focused simplification/self-review and changed-line
tidy; independent review or human attendance was not required by this scope.

## Durable milestone checkpoint

- Assignment: sole non-delegating engineering/bookkeeping owner
  `/root/markdown_milestone_2`, `rfc-implementer`, `gpt-6-astra`, `max`.
- Mandate r1: implement RFC-BPR-0036 in a worktree. Mandate r2 explicitly adds
  the md4qt-versus-native-Qt GFM comparison. MASK readiness was handled by the
  coordinator and does not change product acceptance.
- Run `rfc0036-20260920`, segment `m2-composed-r2`, milestone
  `m2-composed-integration`; prerequisite `m1-shared-component` accepted.
- Worktree `/Users/jaeger/.codex/worktrees/14a8/BetterPullReview`, branch
  `codex/rfc-bpr-0036-native-markdown`; adopted M1 handoff
  `7509d70dad08a55128290a3b3d11e1a37190fb6b`; M1 source
  `4812884e3670971b1b0bdc0b6e5d12fd81ddec48`; M2 source above.
- Authored plan: [rfc0036-native-markdown](../../rfcs/plans/rfc0036-native-markdown/milestone-plan.json).
  Runtime `build/rfc0036-run/execution-strategy.json` preserves the recipe;
  `m2-checkpoint.json`, `m2-observation.json` and `m2-check-result.json` bind
  current cards, exact candidate and quiescent completion. Binding renewal
  accounts for accepted card moves; the authored r2 plan is unchanged.
- Changed source responsibility: core Markdown admission, application projection
  and per-window authority, host/browser composition, shared native reader,
  overview and public conversation presentation, and their focused tests.
  Design/verification/RFC/index/card records are the owned documentation scope.
- Verification: focused integration 14/14; final table/read correction 8/8;
  final composed scales 3/3; native Cocoa pass with inspected ten captures;
  core-only policy/boundary/negative-control checks 4/4; final navigation rerun
  passes. Documentation verifier, its 64 tests and scoped board validation pass.
- Known broader failures: `qt_composed_github_scale_100/125/200` has the
  existing CLI-account-card assertion; `qt_workbench_window_scale_100/125/200`
  has the epoch-row assertion, reproduced on a clean `7509d70d` checkout.
  [[BPR-0940]] already owns both. A separate anonymous-preview account-activation
  assertion is recorded without claiming a passing full family; the changed
  per-window conversation selector passes. Unfiltered tidy has retained
  unrelated warnings; changed/new-line tidy passes without global suppression.
- Review handoff: no required human decision, no attended candidate/protection,
  no loopback server, URL, session or event cursor. Native evidence does not
  claim VoiceOver certification, physical input, Windows/Linux or live services.
- Resource state: every producer-owned build/test/helper command is finished at
  handoff. The disposable baseline checkout/build is removed; synthetic logs,
  captures and manifest remain under `build/rfc0036-run/`. No provider mutation,
  publication, branch push or private-content artifact was produced.
- Unrelated `.mask.lock.json` remains uncommitted and untouched. `NEXT_ID` was
  synchronized under the board lock to the existing BPR-0966 floor from
  `d3d707ac0785b7f0c55187b2d58f91caa4511d16`; no new ID/reservation or external
  source/card was created or imported. Its exact provenance and fresh validation
  are retained in `m2-counter-reconciliation.json`.

## Continuation

All seven engineering successor obligations from the M1 handoff are discharged:
overview, complete public posts/replies, admitted current-context links,
appearance/source identity, explicit broader selectors, native composed evidence
and per-criterion reconciliation. No implementation or required human evidence
remains for this available-platform milestone. Ownership is released to the
coordinator after the guarded finish and quiescent checkpoint.

The next authorized status-only action is to reconcile RFC-BPR-0036 in canonical
order after RFC-BPR-0008 and all required predecessors become Auto-Verified.
Do not change status now or repeat unchanged implementation evidence merely
because a predecessor later closes. Separate live/provider or unavailable-platform
qualification would need its own applicable target and evidence.
