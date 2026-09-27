# RFC-BPR-0023 M9 rendered UX evidence

Scope: BPR-0589, 2026-09-27. All GitHub, agent, thread, finding, draft, and
publication data in this capture set came from deterministic local fixtures.
The PNGs are intentional acceptance artifacts under
[`rfc-bpr-0023-m9-rendered/`](rfc-bpr-0023-m9-rendered/); their hashes are in
[`SHA256SUMS`](rfc-bpr-0023-m9-rendered/SHA256SUMS). No live account was used.

## Executed paths

The `private_review_public_path.py` course launched the shipped app in three
fresh processes per run (seed, compose, publish), drove the real workbench,
and inspected the private SQLite ledger between phases. Each completed run
reported `PRIVATE_REVIEW_PUBLIC_PATH_OK native`:

| Qt platform | Scale | App appearance | Result |
|---|---:|---|---|
| offscreen | 100% | light | Pass; history and composition captured |
| offscreen | 100% | high-contrast-light | Pass; composition captured |
| offscreen | 125% | system | Pass |
| offscreen | 125% | dark | Pass; discovered and then fixed light dialog palette |
| offscreen | 200% | high-contrast-dark | Pass; Tab from preceding action focused Accept and scrolled it into view |
| macOS Cocoa | 125% | dark | Pass; native font and control metrics captured |
| macOS Cocoa | 200% | high-contrast-dark | Pass; native controls and scrolled action captured |

The Cocoa 200% course also had one intermittent fixture-modality timeout at
stage 2 (Review controls were temporarily not offered). A fresh three-process
rerun passed. This is not counted as evidence of focus or product acceptance.
The background Cocoa process has no key window and reports no focus widget;
its course verifies that scrolling exposes Accept. The offscreen course
performs the Tab focus assertion. Native keyboard focus with an active key
window remains a manual/human surface at final acceptance.

The Qt `qt_private_review_panel` test passed at 100% and 200% offscreen scale.
It renders exact actor, completion, availability and lineage variants at a
420-logical-pixel panel width; exercises Tab transitions in History, Sources,
and Write, three composition modes, two distinct exact targets, separate
Split and Accept intents, a stale/unplaceable refusal, palette changes,
accessible names, and unchanged selected indexes without a
full model reset. The provenance projection keeps requested, configured,
protocol-reported and content-self-reported facts separate; the conflicting
case says no winning model or effort is inferred. Test PNGs are synthetic Qt
projections, while Cocoa and offscreen app PNGs are from the real composition
root.

Representative captures:

| Evidence | Capture |
|---|---|
| Private history at 100% light | [history](rfc-bpr-0023-m9-rendered/light-100-history.png) |
| Native provenance at 125% dark | [provenance](rfc-bpr-0023-m9-rendered/cocoa-dark-125-provenance.png) |
| Native candidate at 125% dark | [candidate](rfc-bpr-0023-m9-rendered/cocoa-dark-125-candidate.png) |
| High contrast light candidate | [candidate](rfc-bpr-0023-m9-rendered/high-contrast-light-100-candidate.png) |
| Native candidate at 200% high contrast dark | [candidate](rfc-bpr-0023-m9-rendered/cocoa-high-contrast-dark-200-candidate.png) |
| Native scrolled action at 200% | [Accept and Cancel](rfc-bpr-0023-m9-rendered/cocoa-high-contrast-dark-200-action.png) |
| Native post-publication workbench | [published](rfc-bpr-0023-m9-rendered/cocoa-high-contrast-dark-200-published.png) |
| Actor and state text without color dependence | [legacy](rfc-bpr-0023-m9-rendered/legacy-unknown-100.png), [incomplete](rfc-bpr-0023-m9-rendered/incomplete-200.png), [stale](rfc-bpr-0023-m9-rendered/stale-200.png) |
| Separate factual bases and local/shared lineage | [conflicting provenance](rfc-bpr-0023-m9-rendered/provenance-conflicting-200.png), [composition lineage](rfc-bpr-0023-m9-rendered/composed-published-200.png) |
| Multiple exact targets require choice or Split | [multi-target](rfc-bpr-0023-m9-rendered/composition-multi-target-200.png) |

## Findings and limits

At 125% dark, the separate composition window initially retained a light
palette. It now mirrors the workbench palette at creation and when the parent
palette changes; the Qt test and dark Cocoa capture cover both paths. Cocoa
combo boxes at 200% initially clipped the workspace and target labels. Their
minimum widths were increased and the visible head label shortened to 12 hex
characters; the full head remains in the target tooltip and the intent still
carries the exact target identity. The final native capture shows the target
side, line and short head without clipping. At 200%, the form scrolls, and
Accept and Cancel are visible after scrolling.

These fixtures prove rendered local behavior, not external GitHub publication
acceptance. BPR-0584's designated or signed recording and independent public
body/actor read remain pending. BPR-0588 owns M9 crash, security, quota and
performance evidence. Final independent review remains at M10 as directed by
the owner.
