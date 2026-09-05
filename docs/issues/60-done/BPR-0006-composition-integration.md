---
schema: "implement-rfc.card.v1"
id: "BPR-0006"
title: "Wire and verify the RFC-BPR-0001 public application path"
status: "Done"
rfcs: ["RFC-BPR-0001"]
depends_on: ["BPR-0005"]
completed: "2026-07-29"
---

# Wire and verify the RFC-BPR-0001 public application path

Create the composition root and exercise Qt event to core transition to host
request/completion to incremental Qt projection through the real application.
Expose build version, platform, and selected non-secret configuration.

## Result

- Added the real `better_pull_review` Qt Widgets executable.
- Added an explicit composition root that owns the local foundation
  observation/effect adapters, Qt decision executor, window, and application
  host in safe destruction order.
- Added a Qt queued event-loop executor so effect completions re-enter the
  serialized application lane.
- Exposed version, operating-system product name, and build configuration in
  the native empty state without credential or environment content.
- Added a public-path self-test that triggers the real QAction and requires two
  command/status projections with no reset before exiting successfully.

## Evidence

```text
QT_QPA_PLATFORM=offscreen build/dev/src/app/better_pull_review --self-test
PUBLIC_PATH_OK
```

`application_public_path` passes in CTest. `otool -L` selects Qt 6.11.1 Core,
Gui, and Widgets plus native macOS frameworks. The generated CMake graph shows
the executable depending on the composition target, with adapters depending
on the application target and the application target depending on core.
