# Packaging

This root owns distribution metadata and platform packaging. RFC-BPR-0001
establishes the boundary; RFC-BPR-0014 will add signed Windows, macOS, and Linux
artifacts and their release evidence.

Packaging must preserve the core/application/adapters/composition dependency
graph, restrict Qt plugin discovery to trusted installation paths, and must not
introduce an updater, browser runtime, listener, telemetry, or remote loader.
