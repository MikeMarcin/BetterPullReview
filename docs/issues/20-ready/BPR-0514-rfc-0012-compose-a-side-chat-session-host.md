---
id: BPR-0514
title: "Host: compose a side-chat session host so a send reaches the agent"
status: Ready
rfc: RFC-BPR-0012
depends_on: [BPR-0504]
---

# Host: compose a side-chat session host so a send reaches the agent

## Why

RFC-BPR-0012's card set ([[BPR-0496]]) allocated the core chat scope
([[BPR-0499]]) and the Qt side-chat panel ([[BPR-0502]]) but no chat
*session* host. [[BPR-0504]] composed the panel's presentation and proved it,
and a send on the panel is therefore refused by name rather than quietly
ignored. The turn itself — carrying a send, a scope change, a note opt-in, or
a continuation choice from the panel to a running agent under the
session-keyed opt-in and the ordered scope the core decides — is not
composed anywhere.

## Required final state

- One application host owns the side-chat session: it takes the panel's
  intents, asks the core for the legal scope and continuation
  ([[BPR-0499]]), and drives the agent through the existing launch and
  protocol path with the frozen snapshot [[BPR-0500]] delivers findings from.
  No second launch path exists.
- A send outside the session's opt-in, or against a stale binding without a
  chosen continuation, is refused by the core and the refusal is presented on
  the panel.
- The host is constructed under the same fixture/production split as the
  other agent hosts, named in `SHARED_REQUIRED`, and has a construction cut.
- The replies the agent returns arrive through the same finding delivery
  bounds [[BPR-0500]] enforces; nothing bypasses them.

## Non-goals

- Drawer placement — [[BPR-0513]].
- Any change to the protocol framing or bounds — [[BPR-0500]].

## Required direct evidence

- A registered end-to-end contract beside
  `application_agent_finding_composition_contract`: one send from the panel
  reaches one agent turn and one delivered reply with zero GitHub mutation
  calls; a stale send without a continuation is refused and presented; a
  continuation choice starts exactly one session.
- `composed_application_hosts` fails when the host's construction is deleted.
- The sibling RFC-0012 selectors stay green.
