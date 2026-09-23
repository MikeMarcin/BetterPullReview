# BetterPullReview — Review tour implementation pack

**Version 1 · 5 new RFCs · 26 implementation cards · 8 authored milestones · 43 acceptance requirements**

Build an author-prepared guided review that respects reviewer attention. Preparation is native to BPR; the published tour is **normal GitHub comments**, with an ordered Start here index, exact source destinations and working Previous/Next/overview links for people reviewing entirely on the website.

## Start here

Read [IMPLEMENTATION-PROMPT.md](IMPLEMENTATION-PROMPT.md), [decisions and precedence](supporting/DECISIONS-AND-PRECEDENCE.md), and the [source audit](supporting/SOURCE-AUDIT.md). Import [milestone-plan.json](milestone-plan.json) as the default execution seed; [MILESTONES.md](MILESTONES.md) explains the sequence. The complete manual lifecycle precedes optional AI.

| RFC | Responsibility |
|---|---|
| [RFC-BPR-0039](repository-overlay/docs/rfcs/RFC-BPR-0039-manual-first-review-tour-authoring.md) | Manual-First Review Tour Authoring |
| [RFC-BPR-0040](repository-overlay/docs/rfcs/RFC-BPR-0040-github-native-tour-format-import-and-navigation.md) | GitHub-Native Tour Format, Import, and Navigation |
| [RFC-BPR-0041](repository-overlay/docs/rfcs/RFC-BPR-0041-staged-tour-publication-and-recovery.md) | Staged Tour Publication and Recovery |
| [RFC-BPR-0042](repository-overlay/docs/rfcs/RFC-BPR-0042-published-tour-maintenance-and-reconciliation.md) | Published Tour Maintenance and Reconciliation |
| [RFC-BPR-0043](repository-overlay/docs/rfcs/RFC-BPR-0043-scoped-review-preparation-ai-assistance.md) | Scoped Review Preparation AI Assistance |


## Locked behavior

The author writes **Problem → Solution → Tour stops**, revisits fields freely, inserts/reorders stops and previews the actual GitHub output. Existing PR material can be mapped/reloaded without AI. Drafts persist locally; publication and subsequent updates are explicit. Published reorder preserves comment identities and discussions, and later stops can join the route from another batch.

Magic buttons are optional editing commands. A nonempty selection limits the replacement exactly; otherwise enhance the current field, or recover/draft from PR context if empty. Successful output replaces text in place as **one ordinary undoable edit**. There is no Use/Apply/Dismiss panel. Redo reuses the received text, and editing while a request runs invalidates its response.

Four independently customizable hook defaults are included. Optional outline generation makes only a skeleton: titles, destinations and private prompts. Each retained generated stop requires a visible author visit and completion of its current comment. There is no full-tour generation, bulk Ready or autonomous publication. Jira/issue retrieval is deferred.

## Publication and maintenance

Prefer one native pending review: stage comments, read actual IDs/URLs, wire links, verify the exact membership and submit COMMENT. Extend BPR's existing API/session/journal code, including the admitted `gh api` route. File-level pending staging can use a qualified fixed GraphQL operation; unbatchable compatibility comments require explicit acknowledgment of the different visibility boundary. PR-description edits and later multi-comment updates are not part of an atomic transaction.

Unknown network outcomes are reconciled rather than blindly retried. Website edits are remote truth, with preserved local conflicts. The cold-import path uses normal GitHub Markdown and identities, not hidden metadata or a BPR-only shared manifest.

## Source grounding and integration

The supplied root snapshot was generated September 22, 2026 and contains RFCs through **0038**. Proposed **0039–0043** and pack-local card aliases are not reserved live IDs. Intake checks/remaps collisions and records actual HEAD. [SOURCE-BASELINE.json](references/SOURCE-BASELINE.json) records source hashes/locations; the private 177 MiB source dump itself is **not** included.

[INTEGRATION-MAP.md](supporting/INTEGRATION-MAP.md) identifies current core/application/Qt/provider/store/agent seams and narrow documentation amendments. [AUTHORITY-AND-STATE.md](supporting/AUTHORITY-AND-STATE.md) defines bounded publication plans, one-use child operations, storage and exact editor transactions. Existing non-tour workshop behavior and unrelated PR-management non-goals remain intact.

## Pack contents

- `repository-overlay/docs/rfcs/`: five additive RFCs, not replacements for settled RFCs.
- `cards/`, `milestone-plan.json`, `MILESTONES.md`: actionable implementation slices and the offline-authored execution plan.
- `skill-defaults/`, `specs/`, `examples/`: four writing hooks, fixed application contract, structured result schema, synthetic normal-comment/API examples and Unicode/raw-byte edit cases.
- `supporting/`, `references/`, `verification/`: source/integration audit, design decisions, official API references, complete acceptance mapping, adversarial test plan and designated native/website course.
- `scripts/verify_pack.py`, `tests/test_pack.py`, `SHA256SUMS`: offline artifact validation, negative tests and integrity hashes.

## Validate this archive

Python **3.10+**, standard library only. Commands below operate on the pack, not BPR and not GitHub:

```sh
python3 -B scripts/verify_pack.py
python3 -B -m unittest discover -s tests -p 'test_*.py' -v
```

`verify_pack.py` is human-readable by default; use `--json` for machine output. It checks file integrity, plan/card/RFC/acceptance coverage, dependency acyclicity and milestone ordering, local Markdown destinations, synthetic website links, strict example result shapes and raw-byte splice cases. It does not run an AI model, start `gh`, contact a service, mutate a checkout or validate the installed implement-rfc runtime's own schema.

## Evidence limits

This is a design and implementation handoff, **not an implemented BPR feature**. No application build/run, native UI acceptance or live GitHub/GHES publication was performed here. All 43 product rows remain **Not run** in the supplied matrix. Offline pack validation results are recorded separately in [PACK-VALIDATION.json](verification/PACK-VALIDATION.json). Implementation must supply actual production-path, native and claimed-host evidence before those capabilities are declared verified.
