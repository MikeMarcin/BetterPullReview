# BPR-1513 design and unattended verification

Mandate: owner testing note of 2026-10-14, carried verbatim on BPR-1513.
Frontend-design was read before UX edits. This is the existing native Settings
language: retain its type, row spacing, header actions, frame and separator.

The compact palette uses the existing application roles: dark surface `#1C2026`,
dark text `#E6EDF3`, light surface `#F6F7F9`, light text `#1F2328`; code surfaces
remain independent (`#0D1117` dark, `#FFFFFF` light). Native interface type and
demibold section names carry hierarchy; all names and controls stay left/right
aligned as before. No new accent, role, stylesheet or per-section palette.

The brief asks for subdued section banners, so the header shares the page's
application surface and retains its hairline separation. The code current-line
fill (`AlternateBase`) is independent of settings chrome and may be customized.

Actual composed offscreen inspection exposed a second problem: the independent
top-level Settings shell did not inherit the workbench palette. The window now
copies its accepted palette into both its private and mounted Settings shells on
palette changes; mounting initializes the shared shell from that window too.
This remains Qt presentation mechanics. No core, host, persistence, provider,
security, typography, layout or command semantics change.

The profile-startup public course selects all six modes while Appearance remains
open, checks the resolved theme's actual surface/text at the shell and every
header, asserts readable title contrast and samples each painted banner. It
restores Light before the pre-existing course continues. Optional captures use
the course's actual composed panel and are not committed.

Negative controls: the new header assertion failed against the old role;
strengthening the shell assertion against the resolved built-in palette failed
before the missing palette propagation was repaired. Both are retained under
`build/bpr1513/`. An early invocation during the focused target build saw missing
executables and the old composition; it is superseded by the completed-build run.

Final unattended checks and exact frozen-review identities are recorded on the
card and handoff. Offscreen evidence does not certify macOS native appearance
integration or supply the owner's visual judgement.
