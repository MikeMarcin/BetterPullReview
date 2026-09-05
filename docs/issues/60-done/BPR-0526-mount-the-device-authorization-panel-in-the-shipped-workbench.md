---
id: BPR-0526
title: Mount the device-authorization panel in the shipped workbench
status: Done
rfc: RFC-BPR-0016
depends_on: [BPR-0280]
---

# Mount the device-authorization panel in the shipped workbench

## Why

Every RFC-BPR-0016 layer exists and is composed — the core transitions, the
device host with its refresh-only vault port, the native HTTPS contract, the
system-browser opener ([[BPR-0516]]), and the recorded distributable identity
selected on all three platforms — and `qt::device_authorization_panel`
([[BPR-0280]]) presents the flow with three-scale captures. But nothing
outside `src/adapters/qt` and its own tests references that panel, and
`src/app/src/github_application.cpp` binds
`device_authorization_projection_port` to `device_authorization_projection_sink`,
a recording sink the self-test reads. A reviewer running the shipped
application has no sign-in control that starts the device flow; the flow is
reachable only through `--self-test --device-authorization-probe`. Found by
[[BPR-0284]] while documenting the allocation, on 2026-09-02, after three
staged artifacts had been prepared for an "Open browser" observation that
the shipped window cannot offer.

## Required final state

- The workbench mounts the device-authorization panel where sign-in belongs
  (beside the existing manual token entry, under the same drawer or dialog
  conventions), and the shipped graph binds the device host's projection port
  to it; the fixture graph keeps the recording sink so every existing
  contract still reads what it read.
- The panel's actions dispatch through the device host (begin, cancel,
  open browser), so the browser opener [[BPR-0516]] composed is reachable
  from the window.
- `composed_application_hosts` names the mounted panel's constructor; a
  construction cut leaves the flow reachable through the self-test and the
  positive marker false.
- The three composed-window scale captures show the panel; the sign-in UX
  document's statement that no window mounts the panel is retired.
- The three target-native observations on [[BPR-0516]] become possible, and
  that card records them.

## Non-goals

- Any change to the flow, the identity, or the platform selection.
- Anonymous preview — RFC-BPR-0017.

## Required direct evidence

- `ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|qt_device_authorization_scale_(100\|125\|200)\|application_device_authorization_(public_path\|unavailable\|release_contract\|registered_course\|browser_handoff\|browser_absent)\|composed_application_hosts\|application_review_round_trip_composition_contract)$' --output-on-failure`.
- A composed-window case finds the panel by accessible name and drives
  begin through it to the device host; removing the mount fails by name.
- The two app-source digest pins repinned; the macOS platform course green.

## Scouted mount — 2026-09-02

Read-only findings for the implementing slice, with the decisions it needs
framed:

- **Where sign-in lives today.** `qt::github_session_panel` is constructed
  in `workbench_window.cpp` (about lines 868-883) and mounted in the review
  header column, not a drawer. Its `ConnectButton` (accessible name
  "Connect GitHub host") dispatches `activate_host_profile_intent`, which
  `github_application.cpp`'s `dispatch_intent` turns into
  `prompt_for_credential` — the modal manual-token dialog. No Qt widget
  dispatches `begin_device_authorization_intent`; only the self-test's
  `select_device_authorization()` / `cancel_device_authorization()` reach
  `DeviceHost->dispatch(...)`.
- **The sink is single-target.** `device_authorization_projection_sink`
  is bound as the host's one `device_authorization_projection_port&` at
  construction (about line 3329) and the six device contracts read it
  (`DeviceProjections.last().Stage`, `.published()`, `.last_attempt()`).
  `publish()` is synchronous, so a fan-out projection that forwards every
  call to the sink unconditionally and to the panel when composed keeps
  every contract byte-identical; `composition_agent_finding_projection`
  (about line 3051) is the precedent to copy.
- **The panel.** `device_authorization_panel(core::host_profile_id,
  device_authorization_gesture_dispatch, QWidget*)`, `apply(presentation)`,
  and accessors for begin/open/copy/cancel buttons and the code, address,
  validity, and handoff fields; the scale tests drive it standalone. Its
  gestures are `begin_device_authorization_intent`,
  `cancel_device_authorization_intent`, and
  `open_verification_address_gesture`, all through the host — the Qt
  adapter never opens an address itself.
- **The smallest mount.** Construct the panel in `workbench_window` beside
  the session panel with an accessor; in the composition, a fan-out
  projection bound as the host's port and the panel's dispatch wired to
  the host's begin/cancel/open paths; a `Disable…Composition` flag in the
  fixture profile like the side chat's; the constructor anchor in
  `SHARED_REQUIRED`; a composed-window case that finds the panel by
  accessible name and drives begin through it.
- **Decision framed for the slice.** The panel takes one
  `host_profile_id`; the session panel's picker names several. Bind the
  mounted panel to the profile the picker currently selects and re-present
  it for the profile it changes to; record the choice and its reason on
  the card. The retired UX statement lives in
  `docs/ux/workbench-specification.md` ("Signing in with a device code",
  [[BPR-0284]]).
## What this slice delivered — 2026-09-02

**Where it is mounted.** `workbench_window` constructs
`qt::device_authorization_panel` beside `github_session_panel` and lays it into
the review header column directly under the "Connect GitHub host" control, with
`device_authorization()` as its accessor and
`set_device_authorization_dispatch()` as its one way out. The window supplies no
sink of its own, on the same terms as the side chat: a panel nobody wired stays
drawn and inert. The panel's six stops are declared in the shell's focus order
directly after the session chrome's, each named and described.

**Profile binding, and why.** The mounted panel is bound to
`session_panel()->selected_profile()` — the profile the host picker currently
names — and rebound by `bind_profile()` on `currentIndexChanged` and after an
accepted identity moves the picker with its signals blocked. The reason is that
the picker already names the profile the Connect button acts on: two sign-in
controls side by side that meant two different hosts is the defect this mount is
most likely to introduce, and it would be invisible until a code was typed
against the wrong host. Rebinding changes only the profile a *new* request would
name; it never clears the presentation, because the flow belongs to the host and
not to a profile, and a panel that threw away a live code because the picker
moved would discard the only copy of a code the reviewer is mid-way through
typing. Beginning is forbidden by the core while a flow is live, so a picker
change mid-flow cannot start a second one.

**One port, two readers.** `composition_device_authorization_projection` stands
where `device_authorization_projection_sink` stood on the device host's
`device_authorization_projection_port&`. Every `publish()` reaches the sink
first and unconditionally, then the mounted panel when composed, so all six
existing device contracts read exactly what they read before. The gestures reach
`DeviceHost->dispatch` for begin and cancel and `open_verification_address()`
for the handoff; Qt opens nothing.

| Claim | Command | Result | Commit |
| --- | --- | --- | --- |
| The mount, the fan-out, the three-scale composed captures, the six device contracts unchanged, the linkage anchor, and the repinned digests all hold together | `ctest --preset dev -R '^(qt_composed_github_scale_(100\|125\|200)\|qt_device_authorization_scale_(100\|125\|200)\|application_device_authorization_(public_path\|unavailable\|release_contract\|registered_course\|browser_handoff\|browser_absent\|panel_course\|panel_absent)\|composed_application_hosts\|application_review_round_trip_composition_contract\|qt_workbench_window_scale_100)$' --output-on-failure` | 17/17 passed, 82 s | this commit |
| Beginning from the mounted control reaches the device host: the registered course pressed through the panel found by accessible name enrolls once, stores once, copies nothing durable | `ctest --preset dev -R '^application_device_authorization_panel_course$' --output-on-failure` | Passed 5.45 s; `DEVICE_FLOW_PANEL_BEGAN:true`, `DEVICE_FLOW_PANEL_COMPOSED:true`, `DEVICE_FLOW_ENROLLED_OK` | this commit |
| Removing the mount fails by name rather than by silently starting the flow another way | deleted `column->addWidget(DevicePanel);` from `workbench_window.cpp`, rebuilt, reran the two cases | `application_device_authorization_panel_course` failed on `DEVICE_FLOW_PANEL_BEGAN:false`; `qt_composed_github_scale_100` failed on `FAILED: the live Qt tab chain walks every declared available stop in order`. Restored byte-exact: md5 `2ad12383844fc23bbeb3ee3420a85a98` before and after | this commit |
| The construction cut leaves the flow reachable through the self-test with the positive marker false | `ctest --preset dev -R '^application_device_authorization_panel_absent$' --output-on-failure` | Passed 5.46 s; `DEVICE_FLOW_ENROLLED_OK` beside `DEVICE_FLOW_PANEL_COMPOSED:false` | this commit |
| The composed-host audit actually depends on the new anchor | removed every reference to the panel translation unit from `workbench_window.cpp` and `github_application.cpp` so the static archive drops it | `composed_application_hosts` failed: `COMPOSED_HOSTS_FAILED: required exact demangled anchor is absent: Qt device authorization panel constructor`. Restored byte-exact (md5 `2ad12383844fc23bbeb3ee3420a85a98` and `c9569233624a07d0800b418c0f82d33e`) and it passed again in 1.66 s | this commit |
| The mounted panel renders correctly at all three scales | `qt_composed_github_scale_100`, `_125`, `_200` write `composed-github-<scale>-device-sign-in.png` — the review header cropped to its exact device dimensions, scrolled so the panel is in it | Looked at all three under `build/dev/tests/adapters_qt/`: heading, code, address, validity and the four actions, with "Get a new code" disabled while awaiting and Copy, Open browser and Cancel enabled | this commit |
| Boundaries, format, documentation, and whitespace | `cmake --build build/dev --target check_boundaries`; `clang-format --dry-run --Werror` on all nine touched C++ files; `python3 scripts/verify_documentation.py --root .`; `git diff --check` | Boundaries verified; every file exit 0; `DOCUMENTATION_OK`; no diff-check finding | this commit |

**Digest pins.** `github_application.cpp` is digest-pinned, and the second drift
is hidden until the first is repinned, so both were repinned in two passes and
then again after the formatting pass. `bpr_expected_full_application_digest` is
now `590dc82d6c0e535b970a0c50c1edda13b072f05b1fcc4499b5f4c68103bd20e8` and
`bpr_expected_production_app_manifest_digest` is now
`6366758192d6f11335b776707a98b0e7530c1f4d966cbb43fdfcd572d867e8ee`.

**The retired statement.** `docs/ux/workbench-specification.md`
"Signing in with a device code" no longer says no shipped window mounts the
panel; it now records where the workbench mounts it, what it is bound to, and
which selectors prove each half.

**Carried, not fixed.** `composed_host_verifier_contract` is red at `HEAD`: its
synthetic `SHARED_LINES` fixture in `tests/scripts/verify_composed_hosts_tests.py`
has drifted seven anchors behind `SHARED_REQUIRED` (the agent finding, chat
session, comment workshop and MCP hosts, the side chat panel, the finding queue
view, and the system browser opener). Reproduced at `HEAD` with 6 failures
before this slice; the new panel anchor makes it an eighth absent name in the
same already-failing assertions. Fixing it means editing `tests/scripts/`, which
is outside this slice's allowed files.

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0), `check_boundaries` exit 0, and ran the three
composed-window scales, the three panel scales, the workbench scale, the
eight device contracts including the two new panel ones,
`composed_application_hosts`, the round-trip contract,
`application_composition_contract` (116.02 s), the inventory, and the
finding composition contract: 20/20. `clang-format` clean by exit code on
the nine touched C++ files. The two new selectors are allocated in
`scripts/verify_rfc0016_test_inventory.py` with their required arguments
and named in the ledger's inventory row. The red verifier contract the
slice carried is [[BPR-0529]], fixed in the same push. With the panel
mounted, [[BPR-0516]]'s three observation rows become possible once the
enabled artifacts are restaged from this head.
