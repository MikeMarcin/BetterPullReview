---
schema: "implement-rfc.card.v1"
id: "BPR-0916"
title: "Implement native Markdown rendering for review content"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: []
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc0036-native-markdown","path":"docs/rfcs/plans/rfc0036-native-markdown/milestone-plan.json"}}
acceptance: [{"criterion":"M1","outcome":"pass","revision":"4812884e3670971b1b0bdc0b6e5d12fd81ddec48","evidence":{"kind":"artifact","detail":"Accepted shared reader, bounded resource policy and mandate-r2 empirical md4qt/native-Qt comparison; docs/verification/RFC-BPR-0036.md and docs/architecture/native-markdown.md. Changed reader interactions also pass M2 final component tests."}},{"criterion":"M2","outcome":"pass","revision":"a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f","evidence":{"kind":"observation","detail":"Composed overview and six complete public post/reply body kinds, exact per-window/core-host/browser link admission, source/payload preservation, uncapped long reading and native keyboard/accessibility/appearance/wide-block evidence. AC-01–AC-06 recorded on BPR-0917; 14/14 integration and final 8/8 affected plus 3/3 composed scale checks pass. Native Cocoa 50,742-byte layout 32 ms and pathological source 14 ms. Known broader pre-existing failures are explicitly retained."}}]
---

# Implement native Markdown rendering for review content

Owner feedback after the first loaded PR: “We need an RFC to support markdown
rendering.” Implementation is now authorized by the owner's request to implement
RFC-BPR-0036 in a worktree (mandate r1, 2026-09-20).

Follow [RFC-BPR-0036](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md).
Before source work, inventory overview and public-conversation body paths,
select the supported dialect and native renderer, define measured processing
bounds, and split bounded implementation slices as needed. Preserve original
Markdown for source/copy/publication and keep all resource/navigation effects
out of widgets. No embedded browser or automatic remote image loading.

## Acceptance

Implement AC-01 through AC-06 and record direct composed evidence on [[BPR-0917]].
Coordinate long-description geometry with [[BPR-0918]] without blocking that
investigation on renderer implementation. Use synthetic bodies covering long
text, nested structures, wide tables/code and hostile resource references.

- M1: Inventory the initial surfaces, document the selected dialect, resource
  policy and measured bounds, and implement the shared native component with
  source/copy, selection, keyboard links and synthetic-corpus verification.
- M2: Integrate overview and public conversations through the shipped
  composition, route admitted navigation through host policy, preserve original
  bodies and long-description geometry, and supply AC-01 through AC-06 evidence.

## Execution record

Both authored milestones are accepted. M1 source
`4812884e3670971b1b0bdc0b6e5d12fd81ddec48` supplies the shared component and
mandate-r2 md4qt/native-Qt comparison. M2 source
`a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f` supplies the composed overview and full
public post/reply reader, exact host navigation authority, appearance/focus and
complete native reading behavior. The [verification ledger](../../verification/RFC-BPR-0036.md)
records AC-01–AC-06, passing focused/public/native evidence and the explicitly
classified broader baseline failures. [[BPR-0917]] owns technical closeout;
formal RFC promotion still obeys canonical dependencies.
