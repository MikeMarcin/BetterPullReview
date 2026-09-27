# Tour terminal development

The `tour-cli` preset builds the separate `bpr` executable with tests disabled:

```sh
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix /absolute/install/root
/absolute/install/root/bin/bpr tour capabilities
/absolute/install/root/bin/bpr tour capabilities --json
```

`BPR_BUILD_TOUR_CLI` is independent of `BPR_BUILD_QT`. Core-only remains Qt-free;
the terminal uses `QCoreApplication`, Qt Core/Network, the existing GitHub and Git
adapters, and the profile-private SQLite services. It does not initialize a
desktop window or load Qt GUI, Widgets, QPA, WebEngine, syntax, tray, or updater
services. The Git adapter retains its existing isolated process supervisor.

The macOS development install deploys Qt Core, Qt Network and its native TLS
plugin with paths relative to `bin/bpr`. Other platforms currently use their
installed Qt runtime. This development install is not a signed release package;
release dependency notices, corresponding source, native qualification and
distribution are tracked by the extension's later verification milestone.

Capability discovery reports implemented commands explicitly. Human text is the
default; `--json` writes one `bpr.tour.cli-result.v1` envelope. Unsupported options
refuse before command execution. Authoring commands are being added under
BPR-1055 through BPR-1058; the initial runtime candidate exposes discovery only.

Desktop and terminal authoring use `application::review_tour_host::execute` with
the exact owner, draft and expected revision. Admission and durable save are
separate observations. The parent importer, source semantics and staged
publication services remain authoritative.
