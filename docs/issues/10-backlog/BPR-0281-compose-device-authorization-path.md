---
id: BPR-0281
title: Compose the production device-authorization path
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0279, BPR-0280]
---

# Compose the production device-authorization path

## Required final state

- The tests-disabled shipped application composes the visible device choice
  and explicit per-platform capability boundary without a client identifier;
  it returns `device_flow_unavailable` before request construction. The tests-
  enabled recorded course composes the private verification client and reaches
  the existing profile enrollment/vault path.
- Production macOS, Linux, and Windows all remain visibly
  `device_flow_unavailable` with zero requests in this slice. BPR-0282 owns the
  complete macOS native/security proof while unavailable; BPR-0318 separately
  owns a distributable identity, the macOS enablement, and the final rerun.
- BPR-0310 and BPR-0311 respectively own the same-slice Linux and Windows
  production enablement plus complete target-native rerun. Build target or card
  folder movement alone never enables the capability.
- Unregistered builds and every GHES profile keep the visible choice but return
  typed `device_flow_unavailable` before constructing or sending a request.
- Manual token enrollment remains functional before and after device success.
- Fixture factories and testing helpers are absent from Release; the public-path
  contract proves exact composition rather than linked symbols or helper calls.
