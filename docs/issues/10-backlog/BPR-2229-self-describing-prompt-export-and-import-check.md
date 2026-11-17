---
schema: "implement-rfc.card.v1"
id: "BPR-2229"
title: "A self-describing prompt export, and an import that checks before it writes"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2224", "BPR-2149"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2229--BPR-2230--95323aae2ed0.json","milestone":"M2","milestone_acceptance":"AC18-AC20"}
---

# A self-describing prompt export, and an import that checks before it writes

RFC-BPR-0129 milestone M2 ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md), "A self-describing export, and an
import check"; owner decision 2026-11-16). This card is separate from
[[BPR-2224]] because it needs a person's own prompts in the file, which
RFC-BPR-0125's [[BPR-2149]] adds.

## Scope

- **The guide.** Core generates it from the catalog. It covers every
  Result and Feature with a one-line description, the placements, the
  Features each Result carries, the rules, and one worked example. It is
  written into every export as the Prompts item `guide.prompts`, using
  only existing item fields. The decoder is strict, so a new top-level
  section would make older builds refuse the whole file.
- **Import.** The new build never imports the guide item. The import check
  decides in core whether each prompt is Add, Change or Refused, with the
  RFC's reasons. The import preview shows those decisions and writes
  nothing until the person confirms; Cancel writes nothing.
- **Amendment.** Record the second RFC-BPR-0107 amendment row.
- **Design.** The preview's per-prompt reasons are a surface the owner will
  look at. Load `frontend-design:frontend-design` and file an owner look
  (Fixture agent build, fresh short session, one Accept/Reject for each
  criterion).

## Acceptance

RFC-BPR-0129 AC18, AC19, AC20.

## Intended verification

- A core test comparing the guide with the catalog.
- Fixture files with the worked example and with each mistake, through the
  import preview.
- Importing into a build from before this RFC: the guide and the `flow.*`
  items are listed as not importable, and the rest imports.
