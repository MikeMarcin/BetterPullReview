---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-L4"
title: "Offer RFC-BPR-0022's file comment with a permalink for a filtered diff's line GitHub cannot resolve"
status: "Backlog"
rfcs: ["RFC-BPR-0089","RFC-BPR-0022"]
depends_on: ["BPR-PENDING-L2"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0089 'Comments and line anchoring': a left-side line that does not map to the merge base 'takes RFC-BPR-0022's file comment with a permalink'. M4 refuses it before Publish instead.","related_cards":["BPR-PENDING-L1","BPR-PENDING-L3"]}
---

# BPR-PENDING-L4: Offer RFC-BPR-0022's file comment with a permalink for a filtered diff's line GitHub cannot resolve

RFC-BPR-0089 says a left-side line of a filtered diff that the merge base
does not have unchanged "takes RFC-BPR-0022's file comment with a permalink".
M4 refuses such a line before Publish, saying why, and does the same for a
right-side line GitHub resolves nowhere (outside every hunk at the head or on
the diff's commit).

## Why it is not in M4

RFC-BPR-0022's publication policy reaches the file comment only after a
native attempt is definitively unsupported and that evidence is admitted
under the recorded off-hunk contract; it has no transition that starts at the
fallback. Its permalink also names the head with the selected line, which is
wrong for a line of `parent(first)` or of an earlier commit. A direct
fallback needs a core decision, a journal state and a permalink at the diff's
own commit, each with its reconciliation.

## Acceptance

- **AC1.** With only C3 shown, a base line C1 changed publishes, after the
  editor says so, as a file comment whose permalink names that line at the
  diff's left commit; it reconciles once.
- **AC2.** A right-side line GitHub resolves nowhere does the same at the
  diff's right commit.
- **AC3.** No native attempt is made for either, and no existing admission
  check weakens.
