---
id: BPR-0031
title: Verify RFC-BPR-0002 rendered UX and accessibility
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0028, BPR-0029, BPR-0030]
---

# Verify RFC-BPR-0002 rendered UX and accessibility

Exercise deterministic UI automation through the real application, capture and
inspect rendered states, and verify focus, keyboard access, accessible names,
active host/account visibility, loading, success, each failure class, identity
switching, stale-completion rejection, and capability-disabled actions.

Include a deliberate presentation-polish pass over hierarchy, spacing,
density, typography, control grouping, empty/loading/error states, and
conversation readability. Verify the real native application under the
system-default light and dark appearances. The application must inherit the
active system appearance at startup without forcing a light palette and must
update while running after a live system appearance change. The deterministic
offscreen fixture with a forced cross-platform style is layout evidence only
and cannot satisfy native visual or dark-mode acceptance.
Full user-selectable light/dark/high-contrast appearance profiles remain
RFC-BPR-0006 scope.
