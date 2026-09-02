---
id: BPR-0312
title: Validate the positive anonymous-preview build decision
status: In Progress
rfc: RFC-BPR-0017
depends_on: [BPR-0299]
---

# Validate the positive anonymous-preview build decision

## Required final state

- BPR-0299's byte-preserved exact response affirmatively approves building the
  measured public-preview mode; a rejection, question, partial response, or
  `do not build` verdict cannot satisfy this card.
- The validation record binds the subject digest, response bytes, measured
  probe evidence, approved naming/limits/platform scope, result, and commit.
- This card creates the exact
  `scripts/validate_rfc0017_public_preview_decision.py` interface seeded in the
  verification ledger; it accepts only a byte-exact affirmative `build`
  response and emits the bound validation record. Its `--validate-existing`
  mode writes nothing and revalidates the response and record at closeout.
- On rejection this card remains unresolved in backlog, mechanically blocking
  BPR-0300/BPR-0301 and every transitive implementation card.
- This gate grants no additional network, credential, private-read, mutation,
  release, or RFC-closeout authority.

## Lane — 2026-09-01

[[BPR-0299]] is decided: build. The owner's acceptance carried no note, so the
four definitions this card validates — public-preview naming, limit
presentation, sign-in handoff, and supported-platform policy — are still to be
supplied; this card's packet is where they get bound and validated. It moves
to `20-ready/` as the next unattended step of RFC-BPR-0017: write the
decision packet the ledger's "Product build/no-build decision and positive
gate" row names, with the validator script it seeds, and present it.
