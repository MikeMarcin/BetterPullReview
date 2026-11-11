---
schema: "implement-rfc.card.v1"
id: "BPR-2018"
title: "Verify and close RFC-BPR-0107"
status: "Backlog"
rfcs: ["RFC-BPR-0107"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0003","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0030-close-the-independent-configuration-sharing-slice.md","draft_title":"Close the independent configuration-sharing slice"}
depends_on: ["BPR-2016", "BPR-2017"]
closeout_for: ["RFC-BPR-0107"]
---

# BPR-2018: Verify and close RFC-BPR-0107

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0107](../../rfcs/RFC-BPR-0107-agent-and-prompt-sharing.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for the complete independent first RFC.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Judge every owning RFC criterion against direct evidence, preserve native/human gaps and update only authorized live records after import.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** A user exports selected custom agents and existing prompt overrides, imports them into another profile, and obtains equivalent configured behavior after explicit local binding without any local-PR or enterprise feature installed. | Pending | Pending | Not implemented |
| **AC02:** The exported bytes contain only selected portable material; local agent IDs and identities, executable/workspace paths, unselected settings and account/history stores do not leak into the format. Exact literal arguments and prompt text are inspectable before export. | Pending | Pending | Not implemented |
| **AC03:** Missing programs/skills, incompatible protocols, duplicate identities, unrecognized prompt purposes and limit/version failures produce specific recoverable results, with no runnable partial definition or silent truncation. | Pending | Pending | Not implemented |
| **AC04:** Importing and re-importing preserve existing settings by default. An explicit replacement shows the actual differences and whether the entry's retained conversations remain continuable; relaxed run checks apply only when accepted for that entry; concurrent edits or storage failure cannot leave a partly applied active selection. | Pending | Pending | Not implemented |
| **AC05:** Selecting, inspecting and applying an import produces no process launch, network operation, credential access or harness-configuration change; an explicit subsequent agent check remains distinguishable. | Pending | Pending | Not implemented |
| **AC06:** Public Settings flows are usable with keyboard and accessibility, survive restart, and retain built-in restoration and prompt skill-reference behavior on supported native platforms. | Pending | Pending | Not implemented |
| **AC07:** An exported agent definition carries both added settings; importing one that requests builds leaves builds off for that entry unless accepted in the preview, a requested scratch Off is applied, the entry's identity is unchanged by either, and no pull request's remembered scratch choice appears in the file. | Pending | Pending | Not implemented |
