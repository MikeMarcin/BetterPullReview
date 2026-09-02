---
id: BPR-0526
title: Mount the device-authorization panel in the shipped workbench
status: In Progress
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
