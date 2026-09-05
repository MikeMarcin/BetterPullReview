---
provisional_id: PACK-BPR-0022-08
live_id: allocate-on-import
title: Implement native-first off-hunk negotiation and fallback
status: Backlog
rfc: RFC-BPR-0022
depends_on: [PACK-BPR-0022-06, PACK-BPR-0022-07]
---

# Implement native-first off-hunk negotiation and fallback

Extend the existing review mutation coordinator/provider session with one serialized logical-publication state machine.

## Acceptance

- Legacy lines remain native regardless of off-hunk capability evidence.
- Proven supported off-hunk lines publish natively.
- Unknown support uses the user-approved native write as negotiation.
- Exact unsupported rejection is durably recorded before fallback is prepared.
- All ambiguous/native-possibly-created outcomes perform recovery without fallback.
- Existing double target observation, stale-head policy, acting identity, transport lease, cancellation, one-use production carrier, and server refresh remain intact.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.
