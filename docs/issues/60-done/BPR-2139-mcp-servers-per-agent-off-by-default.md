---
schema: "implement-rfc.card.v1"
id: "BPR-2139"
title: "MCP servers are a per-agent setting, off by default, exported with the agent"
status: "Done"
rfcs: ["RFC-BPR-0104", "RFC-BPR-0107"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["RFC-BPR-0104 AC10", "RFC-BPR-0107 AC08"]}
acceptance: [{"criterion":"RFC-BPR-0104 AC10","outcome":"pass","revision":"1257a42c7e096039235e1647cf5e44e0c092248c","evidence":{"kind":"command","detail":"MCP off by default (Claude empty strict --mcp-config, Codex mcp_servers={}), own passes nothing, fresh paths stay off, rounds follow the setting; carried-over profile without the table reads Off; Settings row and footer tooltip line","command":"ctest --preset dev -R '^(application_agent_launch_policy|application_agent_launch|application_review_cycle_round_launch|sqlite_agent_setup_storage|qt_agent_launch_canary|application_review_cycle_send_public_path)$'"}},{"criterion":"RFC-BPR-0107 AC08","outcome":"pass","revision":"1257a42c7e096039235e1647cf5e44e0c092248c","evidence":{"kind":"command","detail":"mcp_servers and instructions exported by name, read back, applied on import, shown among differences; older files read Off and System prompt; unknown value refuses","command":"ctest --preset dev -R '^(core_agent_configuration|qt_agent_configuration_dialogs|sqlite_agent_configuration_storage)$'"}}]
---

# MCP servers are a per-agent setting, off by default, exported with the agent

## Scope and acceptance

The owner, 2026-11-12: "The MCP thing seems like it's an agent config we
can add to settings and default MCPs to off". Each agent's **MCP servers**:
Off (default; Claude an empty `--mcp-config` with `--strict-mcp-config`,
Codex `-c mcp_servers={}`) or Use the agent's own configuration (nothing
passed; the checkout's servers still never load). Fresh one-off runs keep
MCP off regardless. Stored with the agent (a profile without it migrates to
Off), exported with it (RFC-BPR-0107), shown in Settings and in the footer
picker's tooltip when on. Load frontend-design before the Qt work.

Acceptance: RFC-BPR-0104 AC10, RFC-BPR-0107 AC08 as amended on 2026-11-12.
