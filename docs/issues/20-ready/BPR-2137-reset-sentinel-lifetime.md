---
schema: "implement-rfc.card.v1"
id: "BPR-2137"
title: "Close reset-fixture SQLite sentinel connections before reset"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "win", "mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2137--BPR-2137--e966444cf48a.json","mandate_revision":"reset-sentinel-lifetime-r1","source_baseline":"6efba00e1afbf874c88c7e5de2623a33528ff36f","milestone_plan":{"id":"bpr-2137-reset-sentinel-lifetime","path":"docs/rfcs/plans/bpr-2137-reset-sentinel-lifetime/milestone-plan.json"}}
---

# Close the fixture's own SQLite handles

The BPR-2135 candidate uses SQLite connection context managers for sentinel creation and copied sentinel reads. Context exit commits or rolls back but does not close; its main-local handle survives across Reset everything and can obstruct Windows deletion. Root's retained memory-only API observation confirms the lifetime distinction, without claiming an observed Windows failure. BPR-2135's original Linux passes remain historical results for its exact candidate and its Done state is preserved. This follow-up owns explicit closure, lifecycle controls and final Linux/Windows courses; no product reset change is presumed.

## Acceptance

- AC1: Close both sentinel-write and copied sentinel-read SQLite connections explicitly on success, early return and exceptions before reset or helper return; preserve transaction semantics, anchored POSIX identity and every original public oracle/time bound.
- AC2: Verify the actual helpers with strongly retained real SQLite connection objects, including committed data, absent/present sentinel paths and exceptional cleanup; require a forced-leak negative to be rejected without relying on garbage collection.
- AC3: Run the final exact corrected script through full native Linux Release/Debug and approved limited-interactive Windows Release/Debug configured courses against immutable compiled60 apps, with protected inputs, original containment/environment/watchdog and empty owned forests.
- AC4: Land only owned fixture/regression/admin paths on canonical local Main with exact frozen60-compatible mapping; preserve BPR-2135 Done and its historical Linux-only candidate proof, record the lifetime risk/follow-up, and return native/writer ownership without broad Main intake or remote push.
