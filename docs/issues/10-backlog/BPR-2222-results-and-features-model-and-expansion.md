---
schema: "implement-rfc.card.v1"
id: "BPR-2222"
title: "Results and Features: the catalog, family contracts, /bpr: expansion and trust, and migration"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2219"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2222--BPR-2226--052c4bca31ba.json","milestone":"M1","milestone_acceptance":"AC01-AC06, AC08, AC15-AC17"}
---

# Results and Features: the catalog, family contracts, /bpr: expansion and trust, and migration

RFC-BPR-0129 milestone M1, the model half ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)). It starts after
[[BPR-2219]], whose read-only review format contract becomes the Findings
report Result.

## Scope

- **Core catalog.** The fourteen Results, including the six that are
  added by the owner (O1), and the three Features. Each has its contract,
  family and revision, along with:
  - which placements each Result allows;
  - which Features each Result carries (none for the JSON Results and the
    tour chats).
- **Family contracts.** The Agent tab contract is split from
  `bpr-pr-review` (owner, O2): the boundary, the `brief.*` legend, the
  conversation rule, "always answer in words" and no publishing. The
  Writing contract is reworded per Result.
- **`/bpr:` references.** `bpr:` is a reserved qualifier in
  `core/skill_references`, with the trust rule: no Feature comes from a
  substituted value or from material, and a block is honoured only if its
  request declared it. A repeated reference appends its contract once.
- **Request assembly.** The nine byte classes and the routes for each
  agent kind. The assembled instructions are one value, which the editor
  card renders. The size rules of the RFC's Sizes section apply.
- **Actions.** Parsing each declared block and deciding its action. The
  host performs Draft comment (the block's text at the block's place),
  Keep local, and Apply (disabled with its reason, Q4). The surfaces that
  show these actions are [[BPR-2227]]'s.
- **Migration.** Stored macros, repository layers and the queued encoding,
  with a new queued version (RFC-BPR-0126). Coerced placements are kept.
- **Amendments.** Record the RFC-BPR-0125 and RFC-BPR-0126 amendments. If
  [[BPR-2150]] has already closed RFC-BPR-0125, record that amendment as
  post-closure.
- **Fixture agent.** For every Result, it answers with no format guidance;
  with and without each Feature's block; with a malformed block; and with
  an undeclared block.

## Acceptance

RFC-BPR-0129 AC01 (the model's names), AC02 (core decision), AC03, AC04,
AC05 (the typed question's contract), AC06, AC08, AC15, AC16, AC17.

## Intended verification

- Core tests of the catalog, placement, references and trust, assembly
  classes, block parsing and migration.
- Captured requests for each family, agent kind and grant.
- A composed Fixture-agent course for AC03, AC15 and AC16.
