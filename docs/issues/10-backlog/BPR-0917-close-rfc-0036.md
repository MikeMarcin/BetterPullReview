---
schema: "implement-rfc.card.v1"
id: "BPR-0917"
title: "Verify and close RFC-BPR-0036"
status: "Backlog"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-0916"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0036"]
metadata: {"milestone_plan":{"id":"rfc0036-native-markdown","path":"docs/rfcs/plans/rfc0036-native-markdown/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0036

RFC base: `7ad0e43eb92c217ac6cd01fc478bde2968d217a0`.

Planned cards: BPR-0916, BPR-0917;
append later implementation slices. Related layout investigation: [[BPR-0918]].

[RFC-BPR-0036](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md) is being
implemented under mandate r2. The shared component and md4qt comparison evidence
are recorded in the [verification ledger](../../verification/RFC-BPR-0036.md).
Full RFC acceptance still requires composed integration.

## Closeout checklist

| Criterion | Direct check | Result |
|---|---|---|
| AC-01: Overview and public conversations render the declared Markdown subset through the shipped composition; fixtures cover headings, nested lists, tasks, quotes, code, tables and links. | Composed overview and public conversation Markdown corpus | Not run |
| AC-02: Source, copy and preview preserve original body bytes and publication payloads; empty, malformed and unsupported input stays readable with explicit fallback. | Exact source/copy/publication preservation and readable fallback | Not run |
| AC-03: Hostile HTML, local-file URLs, executable schemes and image/resource references execute nothing and cause no automatic filesystem or network access; admitted link gestures traverse the host-owned path. | Hostile content/resource fixtures and admitted link dispatch | Not run |
| AC-04: Keyboard reading, selection, links and source switching work with appropriate accessibility, light/dark/high-contrast appearance and representative display scales; wide blocks remain usable. | Native keyboard/accessibility and theme/scale inspection | Not run |
| AC-05: Declared bounds and measured long/pathological-body behavior keep the UI responsive; unchanged refresh preserves position and stale completions cannot replace another body's content. | Measured long-body bounds, refresh preservation and stale results | Not run |
| AC-06: Composed long-description reading reaches the final content without a fixed six-line viewport constraint; documentation describes the supported subset, fallback, limits and image policy. | Full long-description reading and supported-feature documentation | Not run |

Record producing commits, commands and observed results for every criterion.
Keep unavailable platform evidence explicit. Require implementation cards done
and RFC dependency eligibility before changing status in canonical order.
Run both documentation gates and focused public-path checks after integration.

## Current continuation: milestone 1 accepted, mandate r2

Run `rfc0036-20260920`, segment `m1-comparison-r2`, owner
`/root/markdown_milestone_1` (`rfc-implementer`, `gpt-6-astra`, `max`). The owner's
original mandate authorizes the full RFC in a worktree; r2 explicitly adds the
md4qt versus native Qt GFM comparison. Milestone 1's component obligation and
five bound exits pass. BPR-0916 remains In Progress and this closeout card remains
Backlog; no composed AC or RFC closeout has been accepted yet.

The worktree is `/Users/jaeger/.codex/worktrees/14a8/BetterPullReview`, branch
`codex/rfc-bpr-0036-native-markdown`. Initial checkout was
`b7e2df46cc178bbcff88b49dd2170ecf27a8c764`. Plan preparation is `85a0184e`, claim
is `1322e913`, and the exact source candidate is
`4812884e3670971b1b0bdc0b6e5d12fd81ddec48`. The later handoff commit only records
evidence and status. The immutable RFC base above remains unchanged.

Durable plan: [rfc0036-native-markdown](../../rfcs/plans/rfc0036-native-markdown/milestone-plan.json).
Its protected r2 amendment preserves the prior exits and adds comparison
acceptance. Runtime continuation files, relative to this worktree:

- `build/rfc0036-run/execution-strategy.json` preserves the selected
  coordinator-implementer recipe, revision 2; host loading is not inferred.
- `build/rfc0036-run/m1-checkpoint-r2.json` binds the exact plan and M1 obligation.
- `build/rfc0036-run/m1-observation.json` and `m1-check-result.json` record the
  quiescent, current-HEAD completion and fresh-successor continuation check.
- `build/rfc0036-run/` retains synthetic comparison source/corpus/results,
  focused logs and native captures. The downloaded comparison repository and
  its compiled build are removed; the developer build remains for integration.

Changed product scope is the shared Qt reader, its private resource-denying
document, component tests, target registration and the narrowly guarded
architecture exception. Design, amended plan, RFC registration text and evidence
are the remaining changed paths. No core or application code is changed and the
reader is not yet mounted in the shipped overview/conversation composition.
The unrelated `.mask.lock.json` registration change belongs to the coordinator
and is excluded from this producer's commits.

The [ledger](../../verification/RFC-BPR-0036.md) records exact commands, retained
failures/fixes and results: component scales 3/3, native Cocoa component pass,
resource negative controls, source/copy/selection/keyboard/bounds/refresh tests,
architecture negative controls, format/tidy, documentation gates and board
validation. Native measured render/source times are 61/80 ms for 65,536/262,144
bytes respectively. Corrected native rendered and wide-content captures were
inspected. This does not certify VoiceOver, physical input or the composed app.

Renderer decision: retain native Qt Markdown with the bounded source-aware
repairs and complete-source fallback. md4qt 5.1.4 was compared at canonical
revision `34d059e066e0dac138f0bc8144fe1d3acbba3dc3`; its parser has useful source
positions and broader syntax, but its ordinary HTML/native bridge loses task
markers and its stream path probes filesystem existence while preparing links.
The [design comparison](../../architecture/native-markdown.md#renderer-comparison-requested-by-the-owner)
distinguishes AST fidelity from bridge behavior and records dependency/license,
maintenance, resource boundary, native accessibility, source and measured costs.

### Exact next milestone obligations

Start a fresh implementer for `m2-composed-integration`, accepting
`m1-shared-component` as its prerequisite, and bind the current plan under r2.
The component's raw link callback is explicitly untrusted and has no host effect.

1. Mount the reader in the shipped overview while preserving RFC0037's expanded
   reader, source anchors, Return/Escape behavior and return-to-files focus.
   Long content must reach its final paragraph without restoring a fixed
   six-line cap; source/rendered reading positions must remain usable.
2. Make complete public conversation and reply bodies reachable and selectable
   through the shipped conversation tree/delegate/panel. Compact navigation
   summaries must not become the only body view. Use stable body identities and
   verify final-content reachability for every public conversation path.
3. Admit raw link gestures through core/application current-authority and
   revision policy, then the existing host browser capability. Resolve the
   RFC's HTTP(S) policy explicitly against the opener's present HTTPS-only
   contract. Refuse local, executable and credential-bearing destinations;
   relative references require explicit current repository/PR context. Apply
   the same admission to image-placeholder links. Rendering itself performs
   no browser, filesystem or network effects.
4. Bind code font/appearance and current root context from the composition;
   preserve original source bytes, preview/publication payloads and receipts.
   Exercise unchanged refresh and authority/body replacement through the real
   composition, including clear, stale completion and capability-denied paths.
5. Extend and run relevant public-path tests. Existing concrete selectors are
   `qt_composed_github_scale_100`, `qt_composed_github_scale_125`,
   `qt_composed_github_scale_200`, `qt_github_conversation_panel`,
   `qt_conversation_cards_view`, `qt_pull_request_overview_page`, and
   `qt_workbench_window_scale_100`, `_125`, `_200`. Include the composed GitHub
   family explicitly; a narrower conversation selector misses that path.
   Select focused application/core link-admission and publication checks after
   the implementation establishes their changed boundary.
6. Supply native keyboard/selection/source/links/accessibility, light/dark/high
   contrast and representative-scale evidence at the composed candidate,
   including wide blocks and the final long-body content. Prepare the configured
   attended-review session and loopback handoff if a required human decision
   remains; this component-only return supplies no such decision.
7. Record AC-01 through AC-06 evidence, finish BPR-0916, then claim eligible
   BPR-0917 and close the RFC only after canonical predecessor/dependency checks.
   Reuse unchanged passing evidence and run the final documentation gate.

All producer-owned build, test and comparison commands are finished before
return; no repository writer, review server or protected attended candidate is
left active. There is no review URL/session/event cursor for milestone 1.
Ownership is released at return for the coordinator's fresh successor. Remaining
uncertainty belongs to milestone 2: complete composition and required native or
human acceptance. Unverifiable tab-bearing structures retain explicit full-source
fallback; do not expand the code-fidelity claim beyond the documented contract.
