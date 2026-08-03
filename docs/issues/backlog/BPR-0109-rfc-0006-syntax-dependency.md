---
id: BPR-0109
title: Acquire KSyntaxHighlighting as a verified build dependency
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0103]
---

# Acquire KSyntaxHighlighting as a verified build dependency

Add KSyntaxHighlighting to the build under the existing dependency-boundary rules: pinned version, cryptographic digest, offline-capable cache, and a recorded provenance entry.

Caller-installed discovery is not available on this platform — no Homebrew formula exists for it or for the KDE Frameworks — so CPM source acquisition is the only route that satisfies the RFC's mandate to use the library at all. That pulls extra-cmake-modules as a transitive build dependency, which the dependency inventory must record.

Production behaviour is unchanged by acquisition: definitions and approved themes are packaged as resources, and standard-path discovery and downloads stay disabled in the production build, per the RFC's non-goals.
