---
schema: "implement-rfc.card.v1"
id: "BPR-2303"
title: "A custom agent's program can be an app bundle or a program inside one (macOS)"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"ux","reservation":"docs/issues/_reservations/BPR-2303--BPR-2303--19845728923b.json","origin":"owner chat 2026-11-19 (\"On Mac, can we allow the custom agent executable browser to open .app bundles somehow? The executable I need to point at is inside of an app bundle.\")","related_cards":["BPR-1753","BPR-1918"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2303: A custom agent's program can be an app bundle or a program inside one (macOS)

## The owner's words (2026-11-19, verbatim)

"On Mac, can we allow the custom agent executable browser to open .app
bundles somehow? The executable I need to point at is inside of an app
bundle."

## What was wrong

The agent dialog's Program › Choose… ([[BPR-1753]]) opened
`QFileDialog::getOpenFileName`. On macOS that is the native open panel with
packages shown as files: an `.app` could be chosen only as a whole, and the
field then held the bundle folder, which is not a program. The panel could
not be browsed into a bundle to reach `Contents/MacOS/…`, and Qt has no option
to change that: its Cocoa helper reads `treatsFilePackagesAsDirectories` but
never sets it.

## What changed

- **Core** (`bpr/core/agent_program_bundle.hpp`). `names_app_bundle` decides
  whether a path names a bundle (last component ends in `.app`, any case,
  trailing slashes ignored). `resolve_app_bundle_program` is a pure function
  over what the host read: `CFBundleExecutable` wins when `Contents/MacOS`
  holds an entry of that name that is not a folder. With no name, the single
  executable file there is the program. Otherwise it gives a typed refusal:
  `no_programs_folder`, `named_program_missing`, `no_program` or
  `several_programs`. A name holding `/`, `.` or `..` is never found, so a
  plist cannot point outside `Contents/MacOS`.
- **Host read** (`bpr/adapters/workspace/app_bundle_read.hpp`).
  `read_app_bundle` follows no links. It reads `Contents/Info.plist` (a
  regular file, at most 1 MiB) through CoreFoundation's
  `CFPropertyListCreateWithData`, which takes XML and binary plists, and lists
  `Contents/MacOS` by metadata (at most 256 entries). Away from macOS the plist
  is never read. The workspace adapter links CoreFoundation on Apple only.
- **Qt.** `agent_entry_dialog` takes an `agent_program_host`: `Choose`
  replaces the chooser, and `ReadBundle` is the host read. When the program
  field names a bundle, the dialog asks the host and passes the listing to the
  core. Choose… puts the resolved program in the field. A typed or pasted
  `.app` counts as its program at once (OK takes it), and the field shows the
  program when it is left. A refused bundle keeps its path in the field, says
  why under the form, and disables OK until the program is changed.
- **Package browsing** (`program_chooser_mac.mm`, macOS default chooser). The
  app's own `NSOpenPanel` sets `treatsFilePackagesAsDirectories = YES` and
  allows both files and folders to be chosen. A bundle can be chosen as a
  whole (Choose on a selected `.app`), and a double-click opens it like a
  folder so a helper anywhere inside can be chosen. A panel delegate's
  `validateURL` refuses plain folders ("Choose a program or an app, not a
  folder."). The panel's message says this.
- **Composition** (`window_agent_setup.cpp`). On macOS only, the Settings
  agent list and the Agent tab's Add custom agent get an editor whose host
  reads bundles through `read_app_bundle`. On Windows and Linux there is no
  reader and the chooser stays `QFileDialog`, so nothing changes there.
- How the program is checked is unchanged: the resolved path goes to
  `inspect_agent_entry` and the setup check exactly as a typed path does.

## Acceptance

- AC1. On macOS, choosing an `.app` fills the field with
  `<bundle>/Contents/MacOS/<CFBundleExecutable>`, or with the bundle's only
  executable when it names none.
- AC2. A bundle with no `Contents/MacOS`, with a name it does not hold, or
  with no name and no executable or several, is refused with a message
  naming the app and saying what to choose instead, and OK waits.
- AC3. The macOS chooser can browse inside packages, so a program elsewhere in
  a bundle can be chosen as itself.
- AC4. A typed or pasted `.app` path resolves the same way.
- AC5. Windows and Linux keep `QFileDialog` and keep a `.app` path as written.

## Evidence

Filled in at closeout.
