---
id: BPR-0312
title: Validate the positive anonymous-preview build decision
status: Backlog
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
