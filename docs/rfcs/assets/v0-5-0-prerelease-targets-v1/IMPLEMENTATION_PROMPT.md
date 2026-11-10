# Implement this pack

Implement only subsequently authorized scope from the fifteen RFCs listed
in the [overview](README.md), using the
[authored milestone plan](milestone-plan.json). Its
[milestone view](MILESTONES.md) is generated; do not edit it independently.
The pack is a draft, not a current implementation mandate.

Read the consumer instructions, the [decision record](DECISIONS.md), the
[glossary](GLOSSARY.md), the owner's
[source notes](SOURCE_NOTES.md) and, before any work on RFC-DRAFT-0010 to
0015, the [reconciliation record](RECONCILIATION.md). Reconcile live source drift against the
inspected revision, import the provisional RFC and card IDs, and adopt the
authored grouping through implement-rfc before product work. The proposed
cards are in [cards/](cards/).

No choice of the owner's is unanswered. Home placement, the Overview faults,
the scratch folder's default, the tour stop design and builds in 0.5.0 are
settled, and the decision record quotes each answer. A recommendation in an
RFC that the record does not show as answered is still not an answer. The owner accepted the stop
mockup that RFC-DRAFT-0009's rules are drawn in; DRAFT-0026 still changes
what may be published and gets critical review, and the owner's look at the
running stop remains.

Start with `dependable-session`: its DRAFT-0022 begins with a diagnosis, and
the RFC's cause section is amended first if the found cause differs.
`agent-tab-asks`, `tour-publishing` and `home-next-move` are independent of
it and of each other. `agent-work-visible` follows `agent-tab-asks`;
`tour-stops` follows `tour-publishing`; `tour-conversation` follows
`agent-tab-asks`, `tour-publishing` and `tour-stops`;
`agent-scratch` follows `agent-work-visible`.

Three facts are to be established, not assumed: why Approve stalls, how the
installed agents report background work, and how Claude's reported cost
accrues on a resumed session. Verify agent flags and events against the
installed versions and their own documentation.

Load frontend-design before any Qt surface work and say so in the report;
the [design round](../../../design/v0-5-0-targets-v1/index.html) is the
starting point, not accepted design. Use offscreen verification and frozen
builds for the owner's looks; never drive the owner's desktop. Live GitHub
courses use MikeMarcin/BPR-Test only.

The amendments this pack makes to RFC-BPR-0011, 0012, 0034, 0041, 0042,
0043, 0066, 0070 and 0096 are stated inside each draft RFC and are narrow. Reconcile the live
text in the owning milestone; adoption is not implementation acceptance.

RFC-DRAFT-0010 to 0015 were absorbed from the local PR drafts pack. Where
they meet RFC-DRAFT-0001 to 0009, the first nine decide the shared surfaces
and each absorbed RFC's section "Fitted to this pack's other decisions" says
how; do not re-decide those surfaces while implementing a local draft. The
owner confirmed the six fitting rules that neither part had stated; they
bind as written. Their preserved decision record in
[local-drafts/](local-drafts/DECISIONS.md) holds the owner's answers D01 to
D14 and P04 to P06, which stand; its former hold on import no longer
applies. Keep agent and prompt sharing an independent first slice of that
part, preserve Review Cycles and existing pull-request flows, keep the
built-in author flow complete without enterprise tools, and do not look for
or execute the enterprise skill; its overview was left outside the
repository on purpose. Re-read
[current behaviour](local-drafts/CURRENT-BEHAVIOR.md) against the checkout
before relying on it, since the first nine RFCs move the code it cites.

All seven absorbed milestones are part of the 0.5.0 release, by the owner's
decision of 2026-11-10. That settles release scope, not authorization: work
starts on any milestone only under the implementation mandate, and in the
plan's order.

Import, draft validation and intermediate card completion are progress, not
acceptance. Preserve actual failures and installed-agent, platform and human
gaps, keep Debug timing nongating, and judge each RFC against real
public-path evidence. Report unresolved prerequisites without presenting
authoring checks as product passes.
