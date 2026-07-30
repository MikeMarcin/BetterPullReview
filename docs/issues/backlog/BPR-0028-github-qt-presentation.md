---
id: BPR-0028
title: Implement GitHub host and pull-request Qt presentation
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0018, BPR-0027]
---

# Implement GitHub host and pull-request Qt presentation

Add accessible host/account selection, active-identity presentation,
pull-request open and refresh events, normalized metadata/conversation
projection, capability-driven action state, and clear authentication,
permission, network, API-version, and capability errors.

Widgets and Qt models receive only core projections and must not receive
credentials, provider records, transports, or host capability ports.
