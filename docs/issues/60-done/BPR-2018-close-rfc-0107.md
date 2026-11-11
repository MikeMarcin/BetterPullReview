---
schema: "implement-rfc.card.v1"
id: "BPR-2018"
title: "Verify and close RFC-BPR-0107"
status: "Done"
rfcs: ["RFC-BPR-0107"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0003","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0030-close-the-independent-configuration-sharing-slice.md","draft_title":"Close the independent configuration-sharing slice"}
depends_on: ["BPR-2016", "BPR-2017"]
closeout_for: ["RFC-BPR-0107"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Two-profile storage course and composed sender/recipient courses; owner look accepted","command":"ctest --preset dev -R sqlite_agent_configuration_storage"}},{"criterion":"AC02","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Only selected portable material in the bytes; exact text shown","command":"ctest --preset dev -R 'core_agent_configuration|sqlite_agent_configuration_storage'"}},{"criterion":"AC03","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Whole-file refusals and per-item problems named","command":"ctest --preset dev -R core_agent_configuration"}},{"criterion":"AC04","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Preserve by default, replacement differences, changed_since, crash journal","command":"ctest --preset dev -R 'sqlite_agent_configuration_storage|qt_agent_configuration_dialogs'"}},{"criterion":"AC05","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"Only identity mints; imported agents Not checked","command":"ctest --preset dev -R sqlite_agent_configuration_storage"}},{"criterion":"AC06","outcome":"pass","revision":"9be81a9c2141b9bca9d20dab65a0528521268dbf","evidence":{"kind":"observation","detail":"macOS keyboard, accessibility, restart, restoration, skill references and owner look (629d64c72746157219d2473892973d7f) accepted. Windows and Linux not run."}},{"criterion":"AC07","outcome":"pass","revision":"7ae9b389c68f28921b101d9384d771dec6c6069a","evidence":{"kind":"command","detail":"RFC-BPR-0104 settings round trip; builds off unless accepted","command":"ctest --preset dev -R core_agent_configuration"}}]
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
| **AC01:** A user exports selected custom agents and existing prompt overrides, imports them into another profile, and obtains equivalent configured behavior after explicit local binding without any local-PR or enterprise feature installed. | `sqlite_agent_configuration_storage`; composed sender and recipient courses ([ledger](../../verification/RFC-BPR-0107.md#composed-public-path)) | Pass on fixture and composed paths; owner look accepted (session `629d64c7…`, no notes) | `7ae9b389c` |
| **AC02:** The exported bytes contain only selected portable material; local agent IDs and identities, executable/workspace paths, unselected settings and account/history stores do not leak into the format. Exact literal arguments and prompt text are inspectable before export. | `core_agent_configuration`, storage course, exact text in the export dialog | Pass | `7ae9b389c` |
| **AC03:** Missing programs/skills, incompatible protocols, duplicate identities, unrecognized prompt purposes and limit/version failures produce specific recoverable results, with no runnable partial definition or silent truncation. | `core_agent_configuration` | Pass | `7ae9b389c` |
| **AC04:** Importing and re-importing preserve existing settings by default. An explicit replacement shows the actual differences and whether the entry's retained conversations remain continuable; relaxed run checks apply only when accepted for that entry; concurrent edits or storage failure cannot leave a partly applied active selection. | Storage course (changed since, crash journal), `qt_agent_configuration_dialogs` | Pass | `7ae9b389c` |
| **AC05:** Selecting, inspecting and applying an import produces no process launch, network operation, credential access or harness-configuration change; an explicit subsequent agent check remains distinguishable. | Storage course (only identity mints), recipient course (agents Not checked) | Pass | `7ae9b389c` |
| **AC06:** Public Settings flows are usable with keyboard and accessibility, survive restart, and retain built-in restoration and prompt skill-reference behavior on supported native platforms. | `qt_agent_configuration_dialogs` (keyboard, accessible names), restart read-back, owner look | Pass on macOS, owner look accepted (`BPR-2017-export-selection`, `BPR-2017-import-preview-bind`, `RFC-BPR-0107-default-prompt-round-trip`); Windows and Linux not run | `7ae9b389c`, `9be81a9c2` |
| **AC07:** An exported agent definition carries both added settings; importing one that requests builds leaves builds off for that entry unless accepted in the preview, a requested scratch Off is applied, the entry's identity is unchanged by either, and no pull request's remembered scratch choice appears in the file. | `core_agent_configuration` | Pass | `7ae9b389c` |

## Closeout (2026-11-11)

Every criterion judged against the evidence in the
[ledger](../../verification/RFC-BPR-0107.md). The RFC's section "Fitted to
this pack's other decisions" holds: the share row sits where the BPR-2051
design record placed it, and the RFC-BPR-0104 settings travel as AC07 says.
AC06 is met on macOS; Windows and Linux were not run and stay recorded as not
run. Documentation gate run before this commit. RFC-BPR-0107 is Auto-Verified.
