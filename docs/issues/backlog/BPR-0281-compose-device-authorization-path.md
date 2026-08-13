---
id: BPR-0281
title: Compose the production device-authorization path
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0279, BPR-0280]
---

# Compose the production device-authorization path

## Required final state

- The tests-disabled shipped application selects the registered GitHub.com
  device capability on macOS only after BPR-0282's target-native evidence and
  reaches the existing profile enrollment/vault path.
- Linux and Windows keep the same visible typed `device_flow_unavailable`
  selection with zero requests until BPR-0310 and BPR-0311 respectively are
  terminal; build target alone never enables the capability.
- Unregistered builds and every GHES profile keep the visible choice but return
  typed `device_flow_unavailable` before constructing or sending a request.
- Manual token enrollment remains functional before and after device success.
- Fixture factories and testing helpers are absent from Release; the public-path
  contract proves exact composition rather than linked symbols or helper calls.
