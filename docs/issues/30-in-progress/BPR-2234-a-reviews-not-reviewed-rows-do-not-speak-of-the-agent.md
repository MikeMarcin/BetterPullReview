---
schema: "implement-rfc.card.v1"
id: "BPR-2234"
title: "A review's Not reviewed rows do not speak of the agent"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# A review's Not reviewed rows do not speak of the agent

## Origin

With the report format of [[BPR-2219]] the real agent's summary is about the change, but its Not reviewed rows still said "I did not" twice (Claude Code, Sonnet, medium effort, MikeMarcin/BPR-Test#185, 2026-11-16). The review skill's own wording asks for what could not be verified in a chat voice.

## Scope and acceptance

- **AC-a.** The review skill and the report format agree on how unchecked things are written: the thing, then why.
- **AC-b.** A real-agent review's Not reviewed rows name things and reasons, with no first person.
