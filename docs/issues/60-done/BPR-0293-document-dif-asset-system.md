---
id: BPR-0293
title: Document the Dif asset system
status: Done
rfc: RFC-BPR-0019
depends_on: [BPR-0287, BPR-0290, BPR-0291, BPR-0292]
---

# Document the Dif asset system

Resolve every planned documentation-ledger entry against the immutable RFC
base without claiming a runtime mascot, package certification, or unattended
approval.

## Required documentation

- Explain the source/generated authority hierarchy, exact clip meanings,
  schema, commands, deterministic resize/alpha/color contract, limits, and
  verdict invalidation rules in the asset README.
- Record identity and accessibility guidance, appearance/high-contrast review,
  security/privacy constraints, build isolation, notice/SBOM disposition, and
  the RFC-BPR-0020 consumption boundary in owning product documents.
- Update stable agent guidance only where future asset edits need mandatory
  regenerate/validate/review steps.
- Reconcile every changed path since the RFC base to an updated owner document
  or a precise no-impact disposition in the verification ledger.

## Completion evidence

- Documentation validation, link checking, diff checks, and trailing-newline
  checks pass.
- The ledger names the proving commit and actual result for every documentation
  surface, including deliberate no-impact regions.

## Completed evidence

- Proving documentation commit:
  `3259cc2b1e479f5fd599fd6743bde3093d052f3d`.
- A clean archive of that exact commit passed
  `python3 scripts/dif_assets.py validate --assets
  src/adapters/qt/assets/dif` with `DIF_ASSETS_VALID` and
  `python3 scripts/verify_documentation.py --root .` with
  `DOCUMENTATION_OK`.
- Every tracked Markdown and `AGENTS.md` file in the clean archive ended with a
  newline, and `git show --check --format= 3259cc2` reported no whitespace
  error.
- The asset README, architecture, UX/accessibility, security, build/test,
  packaging, and Qt-agent owners now state the source/generated hierarchy,
  exact clip and geometry contract, deterministic commands, hard limits,
  review invalidation, non-runtime boundary, and RFC-BPR-0020 handoff.
- The verification ledger partitions every RFC-BPR-0019-owned path family from
  immutable base `58b5116f8706ac336664cc3fc39dabc66745a70d` and records
  exact no-impact dispositions for core, host, current application,
  distribution, and release surfaces.
- Neutral technical-book reporting was used for the owning documentation: it
  reports proven asset facts and limitations without implying runtime,
  package, closeout, or unattended approval.
- The accepted attended-review note is durable: the current reviewed raster
  contains the plus/minus card, but RFC-BPR-0020 may not make it inseparable
  from the core base sprite. Any base/overlay revision must return through the
  RFC-BPR-0019 digest-bound asset and attended-review gates.
