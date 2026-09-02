---
id: BPR-0516
title: Compose a system-browser opener for the device-authorization handoff
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0280, BPR-0318]
---

# Compose a system-browser opener for the device-authorization handoff

## Why

RFC-BPR-0016 requires the verification address to open in the system
browser and never in an embedded one. [[BPR-0280]] delivered the Qt
presentation and the handoff's authority cut and failing-opener contracts,
and said in so many words that the Qt adapter ships no opener because the
opener belongs to composition. Composition never received one:
`src/app/src/github_application.cpp` composes
`device_authorization_browser_handoff{ .Browser = nullptr }` on every
platform under a comment written when no shipped platform could start a
flow. Since [[BPR-0318]], [[BPR-0310]], and [[BPR-0311]] every shipped
platform can, so a reviewer reaches `awaiting_authorization` with a code
and a URL and an "Open browser" action that always answers `unavailable`.
Found by the [[BPR-0283]] scout on 2026-09-02; nothing in `src/` calls
`QDesktopServices::openUrl` or any platform equivalent.

## Required final state

- One production implementation of `device_authorization_browser_port` that
  opens the validated verification address in the operating system's default
  browser through the platform's own launch service, with no embedded engine
  and no network of its own. It is composed under the same
  fixture/production split as the device host, and the fixture profile keeps
  the recording opener the contracts drive.
- The host's authority check still runs before the opener is called; an
  off-authority address reaches no opener (the existing cut stays green).
- A construction cut: with the opener absent the flow still starts and the
  action answers `unavailable`, exactly as today, and the positive marker is
  false. `composed_application_hosts` names the opener's constructor.
- The platform course's `check_platform_surfaces` still proves the absence
  of every embedded browser engine on every platform.
- Target-native observation on each enabled platform that the action opens
  the system browser at the verification address — recorded by each
  station as a row on this card, since no isolated selector can prove a
  real browser launched.

## Non-goals

- Changing the handoff's presentation or the panel — [[BPR-0280]].
- Any change to the authority rule — it is the core's and the host's.

## Verification

```text
ctest --preset dev -R '^(application_device_authorization_host|qt_device_authorization_scale_(100|125|200)|application_device_authorization_(public_path|unavailable|release_contract|registered_course)|composed_application_hosts)$' --output-on-failure
python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-browser --expected-device-selection enabled --fresh
```

The Linux and Windows courses and the browser observation run on the
stations.
